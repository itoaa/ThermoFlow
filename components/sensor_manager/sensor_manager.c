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
 * - Data validation and sanity checks
 * - Average temperature/humidity calculation
 * - I2C bus management
 * - Thread-safe access (via underlying drivers)
 *
 * @author Ola Andersson
 * @version 1.0.0
 * @date 2026-03-22
 *
 * @section changelog Change Log
 * - 1.0.0 (2026-03-22): Initial implementation
 *   - Basic multi-sensor abstraction layer
 *   - Stub implementation for testing (returns dummy data)
 *   - I2C initialization helper
 *   - Data validation framework (SR-001)
 */

#include <string.h>                   /* memcpy, memset */
#include "sensor_manager.h"           /* Public interface */
#include "esp_log.h"                  /* ESP-IDF logging */
#include "driver/i2c.h"               /* I2C driver */

/* Logging tag - appears in log messages from this component */
static const char *TAG = "SENSOR_MGR";

/* Last valid sensor data (cached for quick access) */
static sensor_manager_data_t s_last_data;

/**
 * @brief Initialize sensor manager
 *
 * Sets up internal state and prepares for sensor communication.
 * Currently stub implementation - actual sensor init in sensor_manager_init_i2c().
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_init(void)
{
    ESP_LOGI(TAG, "Sensor manager initialized (stub)");
    return ESP_OK;
}

/**
 * @brief Configure sensor manager parameters
 *
 * @param config Configuration structure (currently unused in stub)
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_configure(const sensor_manager_config_t *config)
{
    (void)config;  /* Unused in stub implementation */
    return ESP_OK;
}

/**
 * @brief Read all configured sensors
 *
 * Populates data structure with readings from all sensors.
 * Current implementation returns stub data for testing.
 *
 * @param[out] data Pointer to data structure to fill
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if data is NULL
 */
esp_err_t sensor_manager_read_all(sensor_manager_data_t *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(data, 0, sizeof(*data));

    /* Stub: return dummy data for testing */
    /* In production, this would read from actual SHT40 sensors */
    for (int i = 0; i < 4; i++) {
        data->temperature[i] = 22.0f + i;  /* Simulated temps: 22-25C */
        data->humidity[i] = 45.0f + i;     /* Simulated RH: 45-48% */
        data->valid[i] = true;             /* Mark as valid */
    }
    data->num_sensors = 4;

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
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(data, &s_last_data, sizeof(*data));
    return ESP_OK;
}

/**
 * @brief Initialize I2C bus for sensors
 *
 * Configures I2C master mode on default pins (GPIO 8/9).
 * Must be called before sensor communication.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if I2C init fails
 */
esp_err_t sensor_manager_init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,           /* Master mode for controlling sensors */
        .sda_io_num = 8,                   /* SDA on GPIO 8 */
        .scl_io_num = 9,                   /* SCL on GPIO 9 */
        .sda_pullup_en = GPIO_PULLUP_ENABLE,  /* Enable internal pullup */
        .scl_pullup_en = GPIO_PULLUP_ENABLE,  /* Enable internal pullup */
        .master.clk_speed = 400000,        /* 400kHz fast mode */
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    ESP_LOGI(TAG, "I2C initialized on GPIO 8/9 at 400kHz");
    return ESP_OK;
}

/**
 * @brief Get number of configured sensors
 *
 * @return Number of sensors (currently hardcoded to 4)
 */
uint8_t sensor_manager_get_num_sensors(void)
{
    return 4;
}

/**
 * @brief Check if sensor data is valid
 *
 * Validates sensor index and checks validity flag.
 *
 * @param index Sensor index (0-3)
 * @return true if valid, false if out of range or invalid data
 */
bool sensor_manager_is_valid(uint8_t index)
{
    return (index < 4) ? s_last_data.valid[index] : false;
}

/**
 * @brief Get average temperature across all sensors
 *
 * Calculates mean of all valid temperature readings.
 * Currently returns stub value (22.5C).
 *
 * @param[out] avg Pointer to store average temperature
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if avg is NULL
 */
esp_err_t sensor_manager_get_avg_temperature(float *avg)
{
    if (!avg) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Stub: return fixed average */
    /* Production: calculate from s_last_data.valid[] entries */
    *avg = 22.5f;
    return ESP_OK;
}

/**
 * @brief Get average humidity across all sensors
 *
 * Calculates mean of all valid humidity readings.
 * Currently returns stub value (45%).
 *
 * @param[out] avg Pointer to store average humidity
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if avg is NULL
 */
esp_err_t sensor_manager_get_avg_humidity(float *avg)
{
    if (!avg) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Stub: return fixed average */
    *avg = 45.0f;
    return ESP_OK;
}

/**
 * @brief Deinitialize sensor manager
 *
 * Releases I2C driver resources.
 *
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_deinit(void)
{
    i2c_driver_delete(I2C_NUM_0);
    ESP_LOGI(TAG, "Sensor manager deinitialized");
    return ESP_OK;
}
