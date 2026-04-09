/**
 * @file sensor_manager.h
 * @brief Sensor Manager Interface
 * 
 * Manages multiple SHT40 sensors and aggregates data.
 * Supports hardware detection with automatic fallback to simulation mode.
 * 
 * @version 1.1.0
 * @date 2026-04-09
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SENSOR_MANAGER_MAX_SENSORS  4

/**
 * @brief Sensor data for all sensors
 */
typedef struct {
    float temperature[SENSOR_MANAGER_MAX_SENSORS];
    float humidity[SENSOR_MANAGER_MAX_SENSORS];
    bool valid[SENSOR_MANAGER_MAX_SENSORS];
    uint8_t sensor_ids[SENSOR_MANAGER_MAX_SENSORS];  /* Added sensor IDs */
    uint32_t timestamp;
    uint8_t num_sensors;
} sensor_manager_data_t;

/**
 * @brief Sensor configuration
 */
typedef struct {
    uint8_t i2c_port;
    uint8_t scl_gpio;
    uint8_t sda_gpio;
    uint32_t i2c_freq;
    uint8_t num_sensors;
} sensor_manager_config_t;

/**
 * @brief Initialize sensor manager
 * 
 * Automatically detects hardware and enters simulation mode
 * if no sensors are detected.
 * 
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_init(void);

/**
 * @brief Configure sensor manager
 * 
 * @param config Configuration
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_configure(const sensor_manager_config_t *config);

/**
 * @brief Read all sensors
 * 
 * Reads from hardware or generates simulated data depending
 * on hardware detection status.
 * 
 * @param[out] data Sensor data structure
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_read_all(sensor_manager_data_t *data);

/**
 * @brief Update cached sensor data
 * 
 * @param data Sensor data to cache
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_update_data(const sensor_manager_data_t *data);

/**
 * @brief Get last sensor data
 * 
 * @param[out] data Sensor data structure
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_get_data(sensor_manager_data_t *data);

/**
 * @brief Check if running in simulation mode
 * 
 * @return true if simulation mode is active
 */
bool sensor_manager_is_simulation_mode(void);

/**
 * @brief Get average temperature from all valid sensors
 * 
 * @return Average temperature in Celsius, or 0.0 if no valid sensors
 */
float sensor_manager_get_avg_temperature(void);

/**
 * @brief Get average humidity from all valid sensors
 * 
 * @return Average humidity in percent, or 0.0 if no valid sensors
 */
float sensor_manager_get_avg_humidity(void);

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
                                                 float *humidity);

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
                                           float *humidity);

/* Legacy functions for backward compatibility */
esp_err_t sensor_manager_init_i2c(void);  /* Deprecated: I2C now handled by hardware_manager */
uint8_t sensor_manager_get_num_sensors(void);
bool sensor_manager_is_valid(uint8_t index);
esp_err_t sensor_manager_get_avg_temperature_legacy(float *avg);  /* Use sensor_manager_get_avg_temperature() instead */
esp_err_t sensor_manager_get_avg_humidity_legacy(float *avg);  /* Use sensor_manager_get_avg_humidity() instead */
esp_err_t sensor_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MANAGER_H */
