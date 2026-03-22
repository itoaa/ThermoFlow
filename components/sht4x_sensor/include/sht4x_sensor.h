/**
 * @file sht4x_sensor.h
 * @brief SHT40 Temperature and Humidity Sensor Driver
 * 
 * Sensirion SHT40 driver for ESP32-S3
 * I2C communication, supports 4 selectable addresses
 * 
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef SHT4X_SENSOR_H
#define SHT4X_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SHT40 I2C addresses
 */
typedef enum {
    SHT4X_ADDR_44 = 0x44,  /**< ADDR pin to GND */
    SHT4X_ADDR_45 = 0x45,  /**< ADDR pin to VDD */
    SHT4X_ADDR_46 = 0x46,  /**< Custom address */
    SHT4X_ADDR_47 = 0x47,  /**< Custom address */
} sht4x_addr_t;

/**
 * @brief Measurement commands
 */
typedef enum {
    SHT4X_CMD_MEASURE_HIGH_PRECISION = 0xFD,   /**< High precision, ~8.2ms */
    SHT4X_CMD_MEASURE_MEDIUM_PRECISION = 0xF6, /**< Medium precision, ~4.5ms */
    SHT4X_CMD_MEASURE_LOW_PRECISION = 0xE0,    /**< Low precision, ~1.7ms */
    SHT4X_CMD_READ_SERIAL = 0x89,              /**< Read serial number */
    SHT4X_CMD_SOFT_RESET = 0x94,               /**< Soft reset */
} sht4x_command_t;

/**
 * @brief SHT40 sensor handle
 */
typedef struct sht4x_sensor *sht4x_handle_t;

/**
 * @brief Sensor reading data
 */
typedef struct {
    float temperature;    /**< Temperature in Celsius, NaN if invalid */
    float humidity;       /**< Relative humidity in %, NaN if invalid */
    uint32_t timestamp;   /**< Reading timestamp (ms since boot) */
    bool valid;           /**< Data validity flag */
} sht4x_reading_t;

/**
 * @brief Sensor configuration
 */
typedef struct {
    uint8_t i2c_port;      /**< I2C port number (0 or 1) */
    uint8_t scl_gpio;      /**< SCL GPIO pin */
    uint8_t sda_gpio;      /**< SDA GPIO pin */
    uint32_t i2c_freq;     /**< I2C frequency in Hz (typically 400000) */
    sht4x_addr_t addr;     /**< I2C address */
    uint8_t precision;     /**< 0=low, 1=medium, 2=high */
} sht4x_config_t;

/**
 * @brief Initialize the SHT40 sensor
 * 
 * @param config Pointer to configuration structure
 * @param[out] handle Pointer to store sensor handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t sht4x_init(const sht4x_config_t *config, sht4x_handle_t *handle);

/**
 * @brief Deinitialize sensor and free resources
 * 
 * @param handle Sensor handle
 * @return ESP_OK on success
 */
esp_err_t sht4x_deinit(sht4x_handle_t handle);

/**
 * @brief Take a single measurement
 * 
 * @param handle Sensor handle
 * @param[out] reading Pointer to store reading
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if timeout
 */
esp_err_t sht4x_read(sht4x_handle_t handle, sht4x_reading_t *reading, uint32_t timeout_ms);

/**
 * @brief Perform soft reset
 * 
 * @param handle Sensor handle
 * @return ESP_OK on success
 */
esp_err_t sht4x_soft_reset(sht4x_handle_t handle);

/**
 * @brief Read serial number
 * 
 * @param handle Sensor handle
 * @param[out] serial Pointer to store 32-bit serial number
 * @return ESP_OK on success
 */
esp_err_t sht4x_read_serial(sht4x_handle_t handle, uint32_t *serial);

/**
 * @brief Check if sensor is reachable
 * 
 * @param handle Sensor handle
 * @return true if sensor responds, false otherwise
 */
bool sht4x_is_present(sht4x_handle_t handle);

/**
 * @brief Get last error code
 * 
 * @param handle Sensor handle
 * @return Last error code
 */
esp_err_t sht4x_get_last_error(sht4x_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SHT4X_SENSOR_H */