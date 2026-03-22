/**
 * @file sensor_manager.h
 * @brief Sensor Manager Interface
 * 
 * Manages multiple SHT40 sensors and aggregates data
 * 
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sht4x_sensor.h"

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
    sht4x_addr_t addresses[SENSOR_MANAGER_MAX_SENSORS];
} sensor_manager_config_t;

/**
 * @brief Initialize sensor manager
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
 * @param[out] data Sensor data structure
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_read_all(sensor_manager_data_t *data);

/**
 * @brief Get last sensor data
 * 
 * @param[out] data Sensor data structure
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_get_data(sensor_manager_data_t *data);

/**
 * @brief Initialize I2C bus
 * 
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_init_i2c(void);

/**
 * @brief Get number of configured sensors
 * 
 * @return Number of sensors
 */
uint8_t sensor_manager_get_num_sensors(void);

/**
 * @brief Check if sensor is valid
 * 
 * @param index Sensor index
 * @return true if sensor data is valid
 */
bool sensor_manager_is_valid(uint8_t index);

/**
 * @brief Get average temperature
 * 
 * @param[out] avg Temperature average
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_get_avg_temperature(float *avg);

/**
 * @brief Get average humidity
 * 
 * @param[out] avg Humidity average
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_get_avg_humidity(float *avg);

/**
 * @brief Deinitialize sensor manager
 * 
 * @return ESP_OK on success
 */
esp_err_t sensor_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_MANAGER_H */