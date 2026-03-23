/**
 * @file sht3x_sensor.h
 * @brief SHT3x Temperature and Humidity Sensor Driver
 *
 * Sensirion SHT3x driver for ESP32-S3
 * Supports SHT30, SHT31, SHT35 (SHT31-D breakout boards)
 * I2C communication with CRC validation
 *
 * Features:
 * - Single-shot and periodic measurement modes
 * - Heater control (for moisture removal)
 * - Alert threshold configuration (SHT35)
 * - Status register reading
 * - CRC-8 data validation
 *
 * @version 1.0.0
 * @date 2026-03-23
 */

#ifndef SHT3X_SENSOR_H
#define SHT3X_SENSOR_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SHT3x I2C addresses
 *
 * ADDR pin to GND = 0x44
 * ADDR pin to VDD = 0x45
 */
typedef enum {
    SHT3X_ADDR_44 = 0x44,  /**< ADDR pin to GND */
    SHT3X_ADDR_45 = 0x45,  /**< ADDR pin to VDD */
} sht3x_addr_t;

/**
 * @brief Measurement modes
 *
 * Single-shot: One measurement, then sleep (low power)
 * Periodic: Continuous measurements at set interval
 */
typedef enum {
    SHT3X_MODE_SINGLE_SHOT,     /**< Single measurement on request */
    SHT3X_MODE_PERIODIC_0_5_HZ, /**< 0.5 measurements per second */
    SHT3X_MODE_PERIODIC_1_HZ,   /**< 1 measurement per second */
    SHT3X_MODE_PERIODIC_2_HZ,   /**< 2 measurements per second */
    SHT3X_MODE_PERIODIC_4_HZ,   /**< 4 measurements per second */
    SHT3X_MODE_PERIODIC_10_HZ,  /**< 10 measurements per second */
} sht3x_mode_t;

/**
 * @brief Measurement repeatability
 *
 * High: Best accuracy, longest measurement time (~15ms)
 * Medium: Balanced accuracy and speed (~6ms)
 * Low: Fastest measurement, lowest accuracy (~4ms)
 */
typedef enum {
    SHT3X_REPEATABILITY_HIGH,    /**< High accuracy, ~15ms */
    SHT3X_REPEATABILITY_MEDIUM,  /**< Medium accuracy, ~6ms */
    SHT3X_REPEATABILITY_LOW,     /**< Low accuracy, ~4ms */
} sht3x_repeatability_t;

/**
 * @brief Heater status
 */
typedef enum {
    SHT3X_HEATER_OFF = 0,
    SHT3X_HEATER_ON = 1,
} sht3x_heater_t;

/**
 * @brief SHT3x sensor handle (opaque pointer)
 */
typedef struct sht3x_sensor *sht3x_handle_t;

/**
 * @brief Sensor reading data
 */
typedef struct {
    float temperature;    /**< Temperature in Celsius, NaN if invalid */
    float humidity;       /**< Relative humidity in %, NaN if invalid */
    uint32_t timestamp;   /**< Reading timestamp (ms since boot) */
    bool valid;           /**< Data validity flag (CRC passed) */
} sht3x_reading_t;

/**
 * @brief Status register flags
 */
typedef struct {
    bool checksum_error;      /**< Last command/write had checksum error */
    bool command_error;       /**< Last command not successful */
    bool reset_detected;      /**< Reset detected since last clear */
    bool alert_humidity_high; /**< Humidity alert: above high threshold */
    bool alert_humidity_low;  /**< Humidity alert: below low threshold */
    bool alert_temp_high;     /**< Temperature alert: above high threshold */
    bool alert_temp_low;      /**< Temperature alert: below low threshold */
    bool heater_status;       /**< Heater is currently on */
} sht3x_status_t;

/**
 * @brief Sensor configuration
 */
typedef struct {
    uint8_t i2c_port;                    /**< I2C port number (0 or 1) */
    uint8_t scl_gpio;                    /**< SCL GPIO pin */
    uint8_t sda_gpio;                    /**< SDA GPIO pin */
    uint32_t i2c_freq;                   /**< I2C frequency (typically 100000 or 400000) */
    sht3x_addr_t addr;                   /**< I2C address (0x44 or 0x45) */
    sht3x_mode_t mode;                   /**< Measurement mode */
    sht3x_repeatability_t repeatability; /**< Measurement repeatability */
} sht3x_config_t;

/**
 * @brief Initialize the SHT3x sensor
 *
 * Configures I2C and sensor for operation.
 * In periodic mode, measurements start automatically.
 * In single-shot mode, call sht3x_read() to trigger measurement.
 *
 * @param config Pointer to configuration structure
 * @param[out] handle Pointer to store sensor handle
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if config/handle NULL,
 *         ESP_ERR_NO_MEM if allocation fails,
 *         ESP_ERR_NOT_FOUND if sensor not detected
 */
esp_err_t sht3x_init(const sht3x_config_t *config, sht3x_handle_t *handle);

/**
 * @brief Deinitialize sensor and free resources
 *
 * Stops periodic measurements and releases I2C driver if no other devices.
 *
 * @param handle Sensor handle
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if handle NULL
 */
esp_err_t sht3x_deinit(sht3x_handle_t handle);

/**
 * @brief Read temperature and humidity
 *
 * Single-shot mode: Triggers measurement, waits for completion, returns data.
 * Periodic mode: Returns latest measurement from buffer (non-blocking).
 *
 * @param handle Sensor handle
 * @param[out] reading Pointer to store reading
 * @param timeout_ms Timeout in milliseconds (single-shot only, 0 for default)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if arguments invalid,
 *         ESP_ERR_TIMEOUT if measurement timeout,
 *         ESP_ERR_INVALID_CRC if data CRC error
 */
esp_err_t sht3x_read(sht3x_handle_t handle, sht3x_reading_t *reading, uint32_t timeout_ms);

/**
 * @brief Set measurement mode
 *
 * Changes mode and repeatability without reinitializing.
 * Stops any active periodic measurements first.
 *
 * @param handle Sensor handle
 * @param mode New measurement mode
 * @param repeatability New repeatability setting
 * @return ESP_OK on success
 */
esp_err_t sht3x_set_mode(sht3x_handle_t handle, sht3x_mode_t mode, sht3x_repeatability_t repeatability);

/**
 * @brief Control internal heater
 *
 * Heater can remove condensation from sensor surface.
 * Increases power consumption significantly when on.
 *
 * @param handle Sensor handle
 * @param heater SHT3X_HEATER_ON to enable, SHT3X_HEATER_OFF to disable
 * @return ESP_OK on success
 */
esp_err_t sht3x_set_heater(sht3x_handle_t handle, sht3x_heater_t heater);

/**
 * @brief Check if heater is currently active
 *
 * @param handle Sensor handle
 * @return true if heater is on, false if off or error
 */
bool sht3x_is_heater_on(sht3x_handle_t handle);

/**
 * @brief Read status register
 *
 * Returns all status flags including alerts and errors.
 * Useful for diagnostics and alert monitoring.
 *
 * @param handle Sensor handle
 * @param[out] status Pointer to status structure
 * @return ESP_OK on success
 */
esp_err_t sht3x_read_status(sht3x_handle_t handle, sht3x_status_t *status);

/**
 * @brief Clear status register
 *
 * Clears reset detected flag, command errors, and checksum errors.
 * Alert flags persist until condition clears.
 *
 * @param handle Sensor handle
 * @return ESP_OK on success
 */
esp_err_t sht3x_clear_status(sht3x_handle_t handle);

/**
 * @brief Perform soft reset
 *
 * Resets sensor to power-up state.
 * All configuration lost, heater turned off, periodic mode stopped.
 *
 * @param handle Sensor handle
 * @return ESP_OK on success
 */
esp_err_t sht3x_soft_reset(sht3x_handle_t handle);

/**
 * @brief Check if sensor is reachable
 *
 * Attempts to read status register as presence check.
 * Can be used for bus scanning.
 *
 * @param handle Sensor handle
 * @return true if sensor responds, false otherwise
 */
bool sht3x_is_present(sht3x_handle_t handle);

/**
 * @brief Get last error code
 *
 * Returns error from last operation (cached in handle).
 * Useful for debugging after failed read.
 *
 * @param handle Sensor handle
 * @return Last error code, ESP_ERR_INVALID_ARG if handle NULL
 */
esp_err_t sht3x_get_last_error(sht3x_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SHT3X_SENSOR_H */
