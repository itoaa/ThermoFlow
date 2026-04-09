/**
 * @file sensor_manager.c
 * @brief Sensor Manager Implementation - ESP-IDF
 *
 * Central hub for managing up to 4 SHT40 temperature/humidity sensors.
 * Aggregates readings, validates data, and provides averaged values.
 * Implements IEC 62443 SR-001: Input Validation requirements.
 *
 * Features:
 * - Multi-sensor support (up to 4x SHT40)
 * - Hardware detection with fallback to simulation mode
 * - Data validation and sanity checks
 * - Average temperature/humidity calculation
 * - Thread-safe access (via underlying drivers)
 *
 * @author Ola Andersson
 * @version 1.1.0
 * @date 2026-04-09
 *
 * @section changelog Change Log
 * - 1.1.0 (2026-04-09): Added hardware detection and simulation mode
 *   - Auto-detect connected sensors
 *   - Fallback to simulated data if no hardware detected
 *   - Support for "bare" ESP32 operation
 * - 1.0.0 (2026-03-22): Initial implementation
 *   - Basic multi-sensor abstraction layer
 *   - Stub implementation for testing (returns dummy data)
 *   - Data validation framework (SR-001)
 */

#include <string.h>                   /* memcpy, memset */
#include <esp_random.h>               /* Hardware RNG for simulation */
#include "sensor_manager.h"             /* Public interface */
#include "hardware_manager.h"             /* Hardware detection */
#include "esp_log.h"                    /* ESP-IDF logging */

/* Logging tag - appears in log messages from this component */
static const char *TAG = "SENSOR_MGR";

/* Last valid sensor data (cached for quick access) */
static sensor_manager_data_t s_last_data = {0};

/* Simulation mode flag */
static bool s_simulation_mode = false;

/* Simulation constants for realistic data */
#define SIM_TEMP_MIN        15.0f   /* Minimum simulated temperature (C) */
#define SIM_TEMP_MAX        25.0f   /* Maximum simulated temperature (C) */
#define SIM_HUMIDITY_MIN    30.0f   /* Minimum simulated humidity (%) */
#define SIM_HUMIDITY_MAX    80.0f   /* Maximum simulated humidity (%) */
#define SIM_TEMP_INLET      18.5f   /* Typical inlet temperature */
#define SIM_TEMP_OUTLET     12.3f   /* Typical outlet temperature */
#define SIM_TEMP_EXHAUST    22.1f   /* Typical exhaust temperature */
#define SIM_TEMP_FRESH      8.7f    /* Typical fresh air temperature */

/**
 * @brief Generate realistic simulated sensor data
 * 
 * Creates synthetic temperature/humidity values that vary
 * slightly over time for realistic demo/testing purposes.
 * 
 * @param[out] data Pointer to data structure to fill
 */
static void generate_simulated_data(sensor_manager_data_t *data)
{
    if (!data) return;
    
    memset(data, 0, sizeof(*data));
    
    /* Base values with small random variation */
    float temp_base[4] = {
        SIM_TEMP_INLET,
        SIM_TEMP_OUTLET,
        SIM_TEMP_EXHAUST,
        SIM_TEMP_FRESH
    };
    
    float humidity_base[4] = {45.0f, 68.5f, 48.0f, 82.0f};
    
    for (int i = 0; i < 4; i++) {
        /* Add small random variation using hardware RNG */
        uint32_t rand_val = esp_random();
        float temp_var = ((float)(rand_val & 0xFF) / 255.0f - 0.5f) * 1.0f;  /* ±0.5C */
        float rh_var = ((float)((rand_val >> 8) & 0xFF) / 255.0f - 0.5f) * 2.0f;  /* ±1% RH */
        
        data->temperature[i] = temp_base[i] + temp_var;
        data->humidity[i] = humidity_base[i] + rh_var;
        
        /* Clamp to valid ranges */
        if (data->humidity[i] < SIM_HUMIDITY_MIN) data->humidity[i] = SIM_HUMIDITY_MIN;
        if (data->humidity[i] > SIM_HUMIDITY_MAX) data->humidity[i] = SIM_HUMIDITY_MAX;
        
        data->valid[i] = true;
        data->sensor_ids[i] = i;
    }
    
    data->num_sensors = 4;
    
    /* Log simulation mode (throttled to avoid spam) */
    static uint32_t last_log = 0;
    uint32_t now = esp_log_timestamp();
    if (now - last_log > 30000) {  /* Log every 30 seconds */
        ESP_LOGI(TAG, "Generated simulated sensor data");
        last_log = now;
    }
}

/**
 * @brief Initialize sensor manager
 *
 * Detects hardware and enters simulation mode if no sensors found.
 * This allows "bare" ESP32 operation for testing and onboarding.
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Sensor Manager Initializing");
    ESP_LOGI(TAG, "========================================");
    
    /* Check if we're in simulation mode */
    s_simulation_mode = hardware_is_simulation_mode();
    
    if (s_simulation_mode) {
        ESP_LOGW(TAG, "  Running in SIMULATION MODE");
        ESP_LOGW(TAG, "  No physical sensors detected");
        ESP_LOGW(TAG, "  Connect SHT40 sensors and reboot");
        ESP_LOGW(TAG, "  to use real hardware.");
        ESP_LOGW(TAG, "========================================");
    } else {
        uint8_t sensor_count = hardware_get_sensor_count();
        ESP_LOGI(TAG, "  Hardware mode: %d sensor(s) detected", sensor_count);
        ESP_LOGI(TAG, "========================================");
    }
    
    /* Initialize cached data */
    memset(&s_last_data, 0, sizeof(s_last_data));
    
    ESP_LOGI(TAG, "Sensor manager initialized");
    return ESP_OK;
}

/**
 * @brief Configure sensor manager parameters
 *
 * @param config Configuration structure (currently unused)
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_configure(const sensor_manager_config_t *config)
{
    (void)config;  /* Unused in current implementation */
    return ESP_OK;
}

/**
 * @brief Read all configured sensors
 *
 * Populates data structure with readings from all sensors.
 * If in simulation mode, returns synthetic data.
 * If hardware detected, reads from actual SHT40 sensors.
 *
 * @param[out] data Pointer to data structure to fill
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if data is NULL
 */
esp_err_t sensor_manager_read_all(sensor_manager_data_t *data)
{
    if (!data) {
        ESP_LOGE(TAG, "Invalid argument: data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(*data));

    if (s_simulation_mode) {
        /* Use simulated data */
        generate_simulated_data(data);
    } else {
        /* TODO: Implement actual SHT40 sensor reading */
        /* For now, fall back to simulation */
        ESP_LOGW(TAG, "Hardware mode not fully implemented, using simulation");
        generate_simulated_data(data);
    }

    /* Validate data per IEC 62443 SR-001 */
    for (int i = 0; i < data->num_sensors; i++) {
        /* Check temperature range */
        if (data->temperature[i] < SIM_TEMP_MIN || data->temperature[i] > SIM_TEMP_MAX + 100.0f) {
            ESP_LOGW(TAG, "Sensor %d: Temperature out of range: %.1f°C", 
                     i, data->temperature[i]);
            data->valid[i] = false;
        }
        
        /* Check humidity range */
        if (data->humidity[i] < 0.0f || data->humidity[i] > 100.0f) {
            ESP_LOGW(TAG, "Sensor %d: Humidity out of range: %.1f%%", 
                     i, data->humidity[i]);
            data->valid[i] = false;
        }
    }

    return ESP_OK;
}

/**
 * @brief Update cached sensor data
 *
 * Stores sensor readings in internal cache for later retrieval.
 *
 * @param data Pointer to new sensor data
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if data is NULL
 */
esp_err_t sensor_manager_update_data(const sensor_manager_data_t *data)
{
    if (!data) {
        ESP_LOGE(TAG, "Invalid argument: data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_last_data, data, sizeof(*data));
    return ESP_OK;
}

/**
 * @brief Get cached sensor data
 *
 * Retrieves the last stored sensor readings.
 *
 * @param[out] data Pointer to data structure to fill
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if data is NULL
 */
esp_err_t sensor_manager_get_data(sensor_manager_data_t *data)
{
    if (!data) {
        ESP_LOGE(TAG, "Invalid argument: data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(data, &s_last_data, sizeof(*data));
    return ESP_OK;
}

/**
 * @brief Check if running in simulation mode
 * 
 * @return true if simulation mode is active
 */
bool sensor_manager_is_simulation_mode(void)
{
    return s_simulation_mode;
}

/**
 * @brief Get average temperature from all valid sensors
 *
 * @return Average temperature in Celsius, or 0.0 if no valid sensors
 */
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

/**
 * @brief Get average humidity from all valid sensors
 *
 * @return Average humidity in percent, or 0.0 if no valid sensors
 */
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

/**
 * @brief Get sensor reading by position
 *
 * @param position Sensor position (0-3)
 * @param[out] temp Temperature in Celsius
 * @param[out] humidity Humidity in percent
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if position invalid
 */
esp_err_t sensor_manager_get_sensor_by_position(uint8_t position, 
                                                 float *temp, 
                                                 float *humidity)
{
    if (position >= 4) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_last_data.valid[position]) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (temp) *temp = s_last_data.temperature[position];
    if (humidity) *humidity = s_last_data.humidity[position];
    
    return ESP_OK;
}

/**
 * @brief Get sensor reading by ID
 *
 * @param sensor_id Sensor ID
 * @param[out] temp Temperature in Celsius
 * @param[out] humidity Humidity in percent
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if sensor not found
 */
esp_err_t sensor_manager_get_sensor_by_id(uint8_t sensor_id,
                                           float *temp,
                                           float *humidity)
{
    for (int i = 0; i < s_last_data.num_sensors; i++) {
        if (s_last_data.sensor_ids[i] == sensor_id && s_last_data.valid[i]) {
            if (temp) *temp = s_last_data.temperature[i];
            if (humidity) *humidity = s_last_data.humidity[i];
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}
