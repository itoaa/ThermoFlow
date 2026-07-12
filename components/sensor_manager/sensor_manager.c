/**
 * @file sensor_manager.c
 * @brief Sensor Manager Implementation - ESP-IDF
 */

#include <string.h>
#include <math.h>
#include <esp_random.h>
#include "sensor_manager.h"
#include "hardware_manager.h"
#include "sht4x_sensor.h"
#include "thermoflow_config.h"
#include "esp_log.h"

static const char *TAG = "SENSOR_MGR";

static sensor_manager_data_t s_last_data = {0};
static bool s_simulation_mode = false;

static sht4x_handle_t s_sensors[SENSOR_MANAGER_MAX_SENSORS] = {0};
static uint8_t s_sensor_addrs[SENSOR_MANAGER_MAX_SENSORS] = {0};
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

    float temp_base[4] = {18.5f, 12.3f, 22.1f, 8.7f};
    float humidity_base[4] = {45.0f, 68.5f, 48.0f, 82.0f};

    for (int i = 0; i < 4; i++) {
        uint32_t rand_val = esp_random();
        float temp_var = ((float)(rand_val & 0xFF) / 255.0f - 0.5f) * 1.0f;
        float rh_var = ((float)((rand_val >> 8) & 0xFF) / 255.0f - 0.5f) * 2.0f;

        data->temperature[i] = temp_base[i] + temp_var;
        data->humidity[i] = humidity_base[i] + rh_var;

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
}

static esp_err_t init_hardware_sensors(void)
{
    s_active_sensor_count = 0;

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

        esp_err_t err = sht4x_init(&cfg, &s_sensors[s_active_sensor_count]);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to init SHT40 at 0x%02X: %s",
                     s_probe_addrs[i], esp_err_to_name(err));
            continue;
        }

        s_sensor_addrs[s_active_sensor_count] = s_probe_addrs[i];
        s_active_sensor_count++;
        ESP_LOGI(TAG, "SHT40 initialized at 0x%02X (slot %d)", s_probe_addrs[i], i);
    }

    if (s_active_sensor_count == 0) {
        ESP_LOGW(TAG, "No SHT40 sensors initialized — falling back to simulation");
        s_simulation_mode = true;
        return ESP_ERR_NOT_FOUND;
    }

    s_simulation_mode = false;
    ESP_LOGI(TAG, "Hardware mode: %d SHT40 sensor(s) active", s_active_sensor_count);
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

    data->num_sensors = s_active_sensor_count;

    for (uint8_t i = 0; i < s_active_sensor_count; i++) {
        sht4x_reading_t reading = {0};
        esp_err_t err = sht4x_read(s_sensors[i], &reading, 100);

        data->sensor_ids[i] = i;
        if (err == ESP_OK && reading.valid &&
            validate_reading(reading.temperature, reading.humidity)) {
            data->temperature[i] = reading.temperature;
            data->humidity[i] = reading.humidity;
            data->valid[i] = true;
        } else {
            data->valid[i] = false;
            ESP_LOGW(TAG, "Sensor %d (0x%02X) read failed: %s",
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
    for (uint8_t i = 0; i < s_active_sensor_count; i++) {
        if (s_sensors[i]) {
            sht4x_deinit(s_sensors[i]);
            s_sensors[i] = NULL;
        }
    }
    s_active_sensor_count = 0;
    return ESP_OK;
}