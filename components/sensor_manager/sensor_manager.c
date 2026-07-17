/**
 * @file sensor_manager.c
 * @brief Sensor Manager Implementation - ESP-IDF
 */

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <esp_random.h>
#include "sensor_manager.h"
#include "hardware_manager.h"
#include "sht4x_sensor.h"
#include "thermoflow_config.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SENSOR_MGR";

static sensor_manager_data_t s_last_data = {0};
static bool s_simulation_mode = false;

/* Fixed role slots: 0=supply, 1=extract, 2=exhaust, 3=outdoor (not packed) */
static sht4x_handle_t s_sensors[SENSOR_MANAGER_MAX_SENSORS] = {0};
static uint8_t s_sensor_addrs[SENSOR_MANAGER_MAX_SENSORS] = {0};
static bool s_slot_active[SENSOR_MANAGER_MAX_SENSORS] = {false};
static uint8_t s_active_sensor_count = 0;

#define SIM_TEMP_MIN        15.0f
#define SIM_TEMP_MAX        25.0f
#define SIM_HUMIDITY_MIN    30.0f
#define SIM_HUMIDITY_MAX    80.0f

static const uint8_t s_probe_addrs[4] = {
    SHT40_ADDR_A, SHT40_ADDR_B, SHT40_ADDR_C, SHT40_ADDR_D
};

static const hw_component_t s_hw_map[4] = {
    HW_COMPONENT_SHT40_SENSOR_1,
    HW_COMPONENT_SHT40_SENSOR_2,
    HW_COMPONENT_SHT40_SENSOR_3,
    HW_COMPONENT_SHT40_SENSOR_4,
};

static void generate_simulated_data(sensor_manager_data_t *data)
{
    if (!data) {
        return;
    }

    memset(data, 0, sizeof(*data));

    /* Simulation: slow daily variation. Values map to fixed slots
     * 0=supply, 1=extract, 2=exhaust, 3=outdoor (AC: varmsida intag). */
    uint32_t now_s = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    float day_phase = (float)(now_s % 86400) / 86400.0f * 6.2831853f;
    float hour_wobble = sinf((float)(now_s % 3600) / 3600.0f * 6.2831853f) * 0.4f;
    uint32_t rand_val = esp_random();
    float noise = ((float)(rand_val & 0xFF) / 255.0f - 0.5f) * 0.3f;

    /* Portable AC-like stream (also usable as FTX demo numbers) */
    float room = 24.0f + 1.2f * sinf(day_phase + 0.3f) + noise * 0.4f;
    float extract_temp = room;                                          /* kall in */
    float supply_temp = room - (9.0f + 2.0f * sinf(day_phase + 1.0f) + noise); /* kall ut */
    float outdoor_temp = room - 0.4f + hour_wobble * 0.3f;              /* varmsida intag */
    float exhaust_temp = outdoor_temp + (13.0f + 2.5f * sinf(day_phase) + noise); /* varm ut */

    float extract_rh = 48.0f + 8.0f * sinf(day_phase + 0.5f);
    float supply_rh = extract_rh + 22.0f;
    if (supply_rh > SIM_HUMIDITY_MAX) {
        supply_rh = SIM_HUMIDITY_MAX;
    }
    float outdoor_rh = extract_rh - 2.0f;
    float exhaust_rh = outdoor_rh - 12.0f;

    float temps[4] = {supply_temp, extract_temp, exhaust_temp, outdoor_temp};
    float humidity[4] = {supply_rh, extract_rh, exhaust_rh, outdoor_rh};

    for (int i = 0; i < 4; i++) {
        data->temperature[i] = temps[i];
        data->humidity[i] = humidity[i];

        if (data->humidity[i] < SIM_HUMIDITY_MIN) {
            data->humidity[i] = SIM_HUMIDITY_MIN;
        }
        if (data->humidity[i] > SIM_HUMIDITY_MAX) {
            data->humidity[i] = SIM_HUMIDITY_MAX;
        }

        data->valid[i] = true;
        data->sensor_ids[i] = (uint8_t)i;
    }

    data->num_sensors = 4;
    data->timestamp = now_s;
}

static esp_err_t init_hardware_sensors(void)
{
    s_active_sensor_count = 0;
    memset(s_slot_active, 0, sizeof(s_slot_active));
    memset(s_sensors, 0, sizeof(s_sensors));
    memset(s_sensor_addrs, 0, sizeof(s_sensor_addrs));

    for (int i = 0; i < SENSOR_MANAGER_MAX_SENSORS; i++) {
        if (!hardware_is_detected(s_hw_map[i])) {
            continue;
        }

        sht4x_config_t cfg = {
            .i2c_port = I2C_MASTER_NUM,
            .scl_gpio = I2C_MASTER_SCL_IO,
            .sda_gpio = I2C_MASTER_SDA_IO,
            .i2c_freq = I2C_MASTER_FREQ_HZ,
            .addr = (sht4x_addr_t)s_probe_addrs[i],
            .precision = 2,
        };

        /* Keep fixed slot index so roles stay supply/extract/exhaust/outdoor */
        esp_err_t err = sht4x_init(&cfg, &s_sensors[i]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to init SHT40 at 0x%02X (slot %d): %s",
                     s_probe_addrs[i], i, esp_err_to_name(err));
            continue;
        }

        s_sensor_addrs[i] = s_probe_addrs[i];
        s_slot_active[i] = true;
        s_active_sensor_count++;
        ESP_LOGI(TAG, "SHT40 initialized at 0x%02X (fixed slot %d)", s_probe_addrs[i], i);
    }

    if (s_active_sensor_count == 0) {
        if (hardware_get_data_source() == HW_DATA_SOURCE_HARDWARE) {
            ESP_LOGW(TAG, "No SHT40 sensors initialized — hardware mode forced");
            s_simulation_mode = false;
            return ESP_ERR_NOT_FOUND;
        }
        ESP_LOGW(TAG, "No SHT40 sensors initialized — falling back to simulation");
        s_simulation_mode = true;
        return ESP_ERR_NOT_FOUND;
    }

    s_simulation_mode = false;
    ESP_LOGI(TAG, "Hardware mode: %d SHT40 sensor(s) active (fixed slots)", s_active_sensor_count);
    return ESP_OK;
}

static bool validate_reading(float temp, float humidity)
{
    if (isnan(temp) || isnan(humidity) || isinf(temp) || isinf(humidity)) {
        return false;
    }
    if (temp < TEMP_MIN_VALID || temp > TEMP_MAX_VALID) {
        return false;
    }
    if (humidity < HUMIDITY_MIN_VALID || humidity > HUMIDITY_MAX_VALID) {
        return false;
    }
    return true;
}

esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "Sensor manager initializing");

    s_simulation_mode = hardware_is_simulation_mode();
    memset(&s_last_data, 0, sizeof(s_last_data));

    if (s_simulation_mode) {
        ESP_LOGW(TAG, "Simulation mode — no sensors detected at boot");
        return ESP_OK;
    }

    return init_hardware_sensors();
}

esp_err_t sensor_manager_configure(const sensor_manager_config_t *config)
{
    (void)config;
    return ESP_OK;
}

esp_err_t sensor_manager_read_all(sensor_manager_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(*data));

    if (s_simulation_mode) {
        generate_simulated_data(data);
        memcpy(&s_last_data, data, sizeof(*data));
        return ESP_OK;
    }

    /* Always expose 4 fixed slots; missing/failed sensors stay valid=false */
    data->num_sensors = SENSOR_MANAGER_MAX_SENSORS;

    for (uint8_t i = 0; i < SENSOR_MANAGER_MAX_SENSORS; i++) {
        data->sensor_ids[i] = i;
        data->valid[i] = false;

        if (!s_slot_active[i]) {
            continue;
        }

        sht4x_reading_t reading = {0};
        esp_err_t err = sht4x_read(s_sensors[i], &reading, 100);

        if (err == ESP_OK && reading.valid &&
            validate_reading(reading.temperature, reading.humidity)) {
            data->temperature[i] = reading.temperature;
            data->humidity[i] = reading.humidity;
            data->valid[i] = true;
        } else {
            ESP_LOGW(TAG, "Sensor slot %d (0x%02X) read failed: %s",
                     i, s_sensor_addrs[i], esp_err_to_name(err));
        }
    }

    data->timestamp = esp_log_timestamp();
    memcpy(&s_last_data, data, sizeof(*data));
    return ESP_OK;
}

esp_err_t sensor_manager_update_data(const sensor_manager_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&s_last_data, data, sizeof(*data));
    return ESP_OK;
}

esp_err_t sensor_manager_get_data(sensor_manager_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(data, &s_last_data, sizeof(*data));
    return ESP_OK;
}

bool sensor_manager_is_simulation_mode(void)
{
    return s_simulation_mode;
}

esp_err_t sensor_manager_refresh_mode(void)
{
    bool target_sim = hardware_is_simulation_mode();
    if (target_sim == s_simulation_mode) {
        return ESP_OK;
    }

    sensor_manager_deinit();
    s_simulation_mode = target_sim;
    memset(&s_last_data, 0, sizeof(s_last_data));

    if (s_simulation_mode) {
        ESP_LOGI(TAG, "Switched to simulation mode");
        return ESP_OK;
    }

    esp_err_t ret = init_hardware_sensors();
    if (ret != ESP_OK && hardware_get_data_source() != HW_DATA_SOURCE_HARDWARE) {
        ESP_LOGW(TAG, "Hardware sensor init failed, staying in simulation");
        s_simulation_mode = true;
    }

    ESP_LOGI(TAG, "Switched to hardware sensor mode");
    return ESP_OK;
}

float sensor_manager_get_avg_temperature(void)
{
    float sum = 0.0f;
    int count = 0;

    for (int i = 0; i < s_last_data.num_sensors; i++) {
        if (s_last_data.valid[i]) {
            sum += s_last_data.temperature[i];
            count++;
        }
    }

    return (count > 0) ? (sum / count) : 0.0f;
}

float sensor_manager_get_avg_humidity(void)
{
    float sum = 0.0f;
    int count = 0;

    for (int i = 0; i < s_last_data.num_sensors; i++) {
        if (s_last_data.valid[i]) {
            sum += s_last_data.humidity[i];
            count++;
        }
    }

    return (count > 0) ? (sum / count) : 0.0f;
}

esp_err_t sensor_manager_get_sensor_by_position(uint8_t position, float *temp, float *humidity)
{
    if (position >= SENSOR_MANAGER_MAX_SENSORS) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_last_data.valid[position]) {
        return ESP_ERR_INVALID_STATE;
    }
    if (temp) {
        *temp = s_last_data.temperature[position];
    }
    if (humidity) {
        *humidity = s_last_data.humidity[position];
    }
    return ESP_OK;
}

esp_err_t sensor_manager_get_sensor_by_id(uint8_t sensor_id, float *temp, float *humidity)
{
    for (int i = 0; i < s_last_data.num_sensors; i++) {
        if (s_last_data.sensor_ids[i] == sensor_id && s_last_data.valid[i]) {
            if (temp) {
                *temp = s_last_data.temperature[i];
            }
            if (humidity) {
                *humidity = s_last_data.humidity[i];
            }
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_manager_deinit(void)
{
    for (uint8_t i = 0; i < SENSOR_MANAGER_MAX_SENSORS; i++) {
        if (s_slot_active[i] && s_sensors[i]) {
            sht4x_deinit(s_sensors[i]);
            s_sensors[i] = NULL;
        }
        s_slot_active[i] = false;
        s_sensor_addrs[i] = 0;
    }
    s_active_sensor_count = 0;
    return ESP_OK;
}