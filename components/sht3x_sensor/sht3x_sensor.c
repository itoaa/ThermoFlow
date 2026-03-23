/**
 * @file sht3x_sensor.c
 * @brief SHT3x Temperature and Humidity Sensor Implementation
 *
 * Sensirion SHT30/SHT31/SHT35 driver for ESP32-S3
 * I2C communication with CRC validation
 *
 * Differences from SHT4x:
 * - Two-byte commands (MSB first)
 * - Longer measurement times (~15ms high precision)
 * - Heater control available
 * - Status register with alert flags
 * - Periodic measurement mode support
 *
 * @version 1.0.0
 * @date 2026-03-23
 *
 * @section changelog Change Log
 * - 1.0.0 (2026-03-23): Initial implementation
 *   - Support for SHT30, SHT31, SHT35
 *   - Single-shot and periodic measurement modes
 *   - Heater control
 *   - Status register reading
 */

#include <string.h>                   /* memset, memcpy */
#include <math.h>                    /* NAN, isnan */
#include "sht3x_sensor.h"              /* Public interface */
#include "driver/i2c.h"                /* ESP-IDF I2C driver */
#include "esp_log.h"                   /* ESP-IDF logging */
#include "esp_timer.h"                 /* High-resolution timer */

/* Logging tag - appears in log messages from this component */
static const char *TAG = "SHT3X";

/* Command definitions (MSB first as per datasheet) */
#define SHT3X_CMD_SINGLE_HIGH      0x2C06  /**< Single shot, high repeatability */
#define SHT3X_CMD_SINGLE_MEDIUM    0x2C0D  /**< Single shot, medium repeatability */
#define SHT3X_CMD_SINGLE_LOW       0x2C10  /**< Single shot, low repeatability */

#define SHT3X_CMD_PERIODIC_0_5_HIGH    0x2032  /**< 0.5 Hz, high repeatability */
#define SHT3X_CMD_PERIODIC_0_5_MEDIUM  0x2024  /**< 0.5 Hz, medium repeatability */
#define SHT3X_CMD_PERIODIC_0_5_LOW     0x202F  /**< 0.5 Hz, low repeatability */

#define SHT3X_CMD_PERIODIC_1_HIGH      0x2130  /**< 1 Hz, high repeatability */
#define SHT3X_CMD_PERIODIC_1_MEDIUM    0x2126  /**< 1 Hz, medium repeatability */
#define SHT3X_CMD_PERIODIC_1_LOW       0x212D  /**< 1 Hz, low repeatability */

#define SHT3X_CMD_PERIODIC_2_HIGH      0x2236  /**< 2 Hz, high repeatability */
#define SHT3X_CMD_PERIODIC_2_MEDIUM    0x2220  /**< 2 Hz, medium repeatability */
#define SHT3X_CMD_PERIODIC_2_LOW       0x222B  /**< 2 Hz, low repeatability */

#define SHT3X_CMD_PERIODIC_4_HIGH      0x2334  /**< 4 Hz, high repeatability */
#define SHT3X_CMD_PERIODIC_4_MEDIUM    0x2322  /**< 4 Hz, medium repeatability */
#define SHT3X_CMD_PERIODIC_4_LOW       0x2329  /**< 4 Hz, low repeatability */

#define SHT3X_CMD_PERIODIC_10_HIGH     0x2737  /**< 10 Hz, high repeatability */
#define SHT3X_CMD_PERIODIC_10_MEDIUM   0x2721  /**< 10 Hz, medium repeatability */
#define SHT3X_CMD_PERIODIC_10_LOW      0x272A  /**< 10 Hz, low repeatability */

#define SHT3X_CMD_READ_PERIODIC  0xE000  /**< Read periodic measurement data */
#define SHT3X_CMD_BREAK        0x3093  /**< Break/stop periodic measurement */
#define SHT3X_CMD_HEATER_ON    0x306D  /**< Enable heater */
#define SHT3X_CMD_HEATER_OFF   0x3066  /**< Disable heater */
#define SHT3X_CMD_READ_STATUS  0xF32D  /**< Read status register */
#define SHT3X_CMD_CLEAR_STATUS 0x3041  /**< Clear status register */
#define SHT3X_CMD_SOFT_RESET   0x30A2  /**< Soft reset */
#define SHT3X_CMD_GENERAL_RESET 0x0006 /**< General call reset (all devices) */

/* Measurement delays in milliseconds */
#define SHT3X_DELAY_HIGH_MS     15   /**< High repeatability measurement time */
#define SHT3X_DELAY_MEDIUM_MS   6    /**< Medium repeatability measurement time */
#define SHT3X_DELAY_LOW_MS      4    /**< Low repeatability measurement time */

/* Timeout defaults */
#define SHT3X_DEFAULT_TIMEOUT_MS  100  /**< Default I2C command timeout */
#define SHT3X_RESET_DELAY_MS    2    /**< Delay after reset command */

/**
 * @brief Internal sensor structure
 */
struct sht3x_sensor {
    uint8_t i2c_port;                    /**< I2C port number */
    uint8_t addr;                        /**< I2C address */
    sht3x_mode_t mode;                 /**< Current measurement mode */
    sht3x_repeatability_t repeatability; /**< Current repeatability */
    uint32_t measure_delay_ms;           /**< Measurement delay for current mode */
    bool heater_enabled;                 /**< Heater status cache */
    bool periodic_active;                /**< Periodic mode active */
    esp_err_t last_error;                /**< Last error code */
};

/**
 * @brief Calculate CRC-8 for SHT3x data validation
 *
 * Polynomial: x^8 + x^5 + x^4 + 1 (0x31)
 * Initialization: 0xFF
 * Same algorithm used by SHT4x series
 *
 * @param data Data buffer
 * @param len Data length
 * @return CRC-8 checksum
 */
static uint8_t sht3x_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;  /* Initialization value */

    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
        }
    }
    return crc;
}

/**
 * @brief Convert raw temperature ticks to Celsius
 *
 * Formula: T = -45 + 175 * (ticks / 65535)
 * Same formula as SHT4x
 *
 * @param ticks Raw 16-bit temperature value
 * @return Temperature in Celsius
 */
static float sht3x_ticks_to_temperature(uint16_t ticks)
{
    return -45.0f + 175.0f * ((float)ticks / 65535.0f);
}

/**
 * @brief Convert raw humidity ticks to percentage
 *
 * Formula: RH = -6 + 125 * (ticks / 65535)
 * Clamped to 0-100% range
 * Same formula as SHT4x
 *
 * @param ticks Raw 16-bit humidity value
 * @return Relative humidity in percent
 */
static float sht3x_ticks_to_humidity(uint16_t ticks)
{
    float rh = -6.0f + 125.0f * ((float)ticks / 65535.0f);
    /* Clamp to valid range */
    if (rh < 0.0f) return 0.0f;
    if (rh > 100.0f) return 100.0f;
    return rh;
}

/**
 * @brief Send command to sensor
 *
 * @param handle Sensor handle
 * @param cmd Two-byte command
 * @return ESP_OK on success
 */
static esp_err_t sht3x_send_command(struct sht3x_sensor *sensor, uint16_t cmd)
{
    uint8_t cmd_bytes[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF) };

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (sensor->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd_handle, cmd_bytes, 2, true);
    i2c_master_stop(cmd_handle);

    esp_err_t err = i2c_master_cmd_begin(sensor->i2c_port, cmd_handle,
                                          pdMS_TO_TICKS(SHT3X_DEFAULT_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd_handle);

    return err;
}

/**
 * @brief Get measurement delay for current repeatability
 *
 * @param repeatability Repeatability setting
 * @return Delay in milliseconds
 */
static uint32_t get_measure_delay(sht3x_repeatability_t repeatability)
{
    switch (repeatability) {
        case SHT3X_REPEATABILITY_LOW:
            return SHT3X_DELAY_LOW_MS;
        case SHT3X_REPEATABILITY_MEDIUM:
            return SHT3X_DELAY_MEDIUM_MS;
        case SHT3X_REPEATABILITY_HIGH:
        default:
            return SHT3X_DELAY_HIGH_MS;
    }
}

/**
 * @brief Get single-shot command for repeatability
 *
 * @param repeatability Repeatability setting
 * @return Command code
 */
static uint16_t get_single_command(sht3x_repeatability_t repeatability)
{
    switch (repeatability) {
        case SHT3X_REPEATABILITY_LOW:
            return SHT3X_CMD_SINGLE_LOW;
        case SHT3X_REPEATABILITY_MEDIUM:
            return SHT3X_CMD_SINGLE_MEDIUM;
        case SHT3X_REPEATABILITY_HIGH:
        default:
            return SHT3X_CMD_SINGLE_HIGH;
    }
}

/**
 * @brief Get periodic command for mode and repeatability
 *
 * @param mode Periodic mode
 * @param repeatability Repeatability setting
 * @return Command code
 */
static uint16_t get_periodic_command(sht3x_mode_t mode, sht3x_repeatability_t repeatability)
{
    switch (mode) {
        case SHT3X_MODE_PERIODIC_0_5_HZ:
            switch (repeatability) {
                case SHT3X_REPEATABILITY_LOW: return SHT3X_CMD_PERIODIC_0_5_LOW;
                case SHT3X_REPEATABILITY_MEDIUM: return SHT3X_CMD_PERIODIC_0_5_MEDIUM;
                default: return SHT3X_CMD_PERIODIC_0_5_HIGH;
            }
        case SHT3X_MODE_PERIODIC_1_HZ:
            switch (repeatability) {
                case SHT3X_REPEATABILITY_LOW: return SHT3X_CMD_PERIODIC_1_LOW;
                case SHT3X_REPEATABILITY_MEDIUM: return SHT3X_CMD_PERIODIC_1_MEDIUM;
                default: return SHT3X_CMD_PERIODIC_1_HIGH;
            }
        case SHT3X_MODE_PERIODIC_2_HZ:
            switch (repeatability) {
                case SHT3X_REPEATABILITY_LOW: return SHT3X_CMD_PERIODIC_2_LOW;
                case SHT3X_REPEATABILITY_MEDIUM: return SHT3X_CMD_PERIODIC_2_MEDIUM;
                default: return SHT3X_CMD_PERIODIC_2_HIGH;
            }
        case SHT3X_MODE_PERIODIC_4_HZ:
            switch (repeatability) {
                case SHT3X_REPEATABILITY_LOW: return SHT3X_CMD_PERIODIC_4_LOW;
                case SHT3X_REPEATABILITY_MEDIUM: return SHT3X_CMD_PERIODIC_4_MEDIUM;
                default: return SHT3X_CMD_PERIODIC_4_HIGH;
            }
        case SHT3X_MODE_PERIODIC_10_HZ:
            switch (repeatability) {
                case SHT3X_REPEATABILITY_LOW: return SHT3X_CMD_PERIODIC_10_LOW;
                case SHT3X_REPEATABILITY_MEDIUM: return SHT3X_CMD_PERIODIC_10_MEDIUM;
                default: return SHT3X_CMD_PERIODIC_10_HIGH;
            }
        default:
            return SHT3X_CMD_SINGLE_HIGH;  /* Fallback */
    }
}

esp_err_t sht3x_init(const sht3x_config_t *config, sht3x_handle_t *handle)
{
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Validate I2C address */
    if (config->addr != SHT3X_ADDR_44 && config->addr != SHT3X_ADDR_45) {
        ESP_LOGE(TAG, "Invalid I2C address: 0x%02X", config->addr);
        return ESP_ERR_INVALID_ARG;
    }

    /* Allocate sensor structure */
    struct sht3x_sensor *sensor = calloc(1, sizeof(struct sht3x_sensor));
    if (!sensor) {
        return ESP_ERR_NO_MEM;
    }

    /* Store configuration */
    sensor->i2c_port = config->i2c_port;
    sensor->addr = config->addr;
    sensor->mode = config->mode;
    sensor->repeatability = config->repeatability;
    sensor->measure_delay_ms = get_measure_delay(config->repeatability);
    sensor->heater_enabled = false;
    sensor->periodic_active = false;
    sensor->last_error = ESP_OK;

    /* Configure I2C */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->i2c_freq,
    };

    esp_err_t err = i2c_param_config(sensor->i2c_port, &i2c_conf);
    if (err != ESP_OK) {
        free(sensor);
        return err;
    }

    err = i2c_driver_install(sensor->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        free(sensor);
        return err;
    }

    /* Verify sensor presence */
    if (!sht3x_is_present(sensor)) {
        ESP_LOGE(TAG, "Sensor not found at address 0x%02X", sensor->addr);
        free(sensor);
        return ESP_ERR_NOT_FOUND;
    }

    /* Start periodic measurements if requested */
    if (config->mode != SHT3X_MODE_SINGLE_SHOT) {
        uint16_t cmd = get_periodic_command(config->mode, config->repeatability);
        err = sht3x_send_command(sensor, cmd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start periodic mode");
            free(sensor);
            return err;
        }
        sensor->periodic_active = true;
        ESP_LOGI(TAG, "Periodic mode started (%d Hz)",
                 config->mode == SHT3X_MODE_PERIODIC_0_5_HZ ? 0 :
                 config->mode == SHT3X_MODE_PERIODIC_1_HZ ? 1 :
                 config->mode == SHT3X_MODE_PERIODIC_2_HZ ? 2 :
                 config->mode == SHT3X_MODE_PERIODIC_4_HZ ? 4 : 10);
    }

    *handle = sensor;
    ESP_LOGI(TAG, "SHT3x initialized at 0x%02X", sensor->addr);
    return ESP_OK;
}

esp_err_t sht3x_deinit(sht3x_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;

    /* Stop periodic measurements if active */
    if (sensor->periodic_active) {
        sht3x_send_command(sensor, SHT3X_CMD_BREAK);
    }

    /* Turn off heater */
    sht3x_send_command(sensor, SHT3X_CMD_HEATER_OFF);

    free(sensor);
    ESP_LOGI(TAG, "SHT3x deinitialized");
    return ESP_OK;
}

esp_err_t sht3x_read(sht3x_handle_t handle, sht3x_reading_t *reading, uint32_t timeout_ms)
{
    if (!handle || !reading) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;

    /* Initialize reading to invalid state */
    memset(reading, 0, sizeof(*reading));
    reading->temperature = NAN;
    reading->humidity = NAN;
    reading->valid = false;

    if (sensor->mode == SHT3X_MODE_SINGLE_SHOT) {
        /* Trigger single measurement */
        uint16_t cmd = get_single_command(sensor->repeatability);
        esp_err_t err = sht3x_send_command(sensor, cmd);
        if (err != ESP_OK) {
            sensor->last_error = err;
            return err;
        }

        /* Wait for measurement */
        uint32_t wait_time = timeout_ms > 0 ? timeout_ms : sensor->measure_delay_ms;
        vTaskDelay(pdMS_TO_TICKS(wait_time));
    } else if (!sensor->periodic_active) {
        ESP_LOGE(TAG, "Periodic mode not active");
        return ESP_ERR_INVALID_STATE;
    }

    /* Read data: 6 bytes (temp MSB, temp LSB, temp CRC, hum MSB, hum LSB, hum CRC) */
    uint8_t data[6];
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (sensor->addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);

    esp_err_t err = i2c_master_cmd_begin(sensor->i2c_port, cmd_handle,
                                          pdMS_TO_TICKS(SHT3X_DEFAULT_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd_handle);

    if (err != ESP_OK) {
        sensor->last_error = err;
        return err;
    }

    /* Verify CRCs */
    if (sht3x_crc8(data, 2) != data[2]) {
        ESP_LOGW(TAG, "Temperature CRC error");
        sensor->last_error = ESP_ERR_INVALID_CRC;
        return ESP_ERR_INVALID_CRC;
    }
    if (sht3x_crc8(data + 3, 2) != data[5]) {
        ESP_LOGW(TAG, "Humidity CRC error");
        sensor->last_error = ESP_ERR_INVALID_CRC;
        return ESP_ERR_INVALID_CRC;
    }

    /* Convert data */
    uint16_t temp_ticks = ((uint16_t)data[0] << 8) | data[1];
    uint16_t hum_ticks = ((uint16_t)data[3] << 8) | data[4];

    reading->temperature = sht3x_ticks_to_temperature(temp_ticks);
    reading->humidity = sht3x_ticks_to_humidity(hum_ticks);
    reading->timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    reading->valid = true;

    sensor->last_error = ESP_OK;
    return ESP_OK;
}

esp_err_t sht3x_set_mode(sht3x_handle_t handle, sht3x_mode_t mode,
                          sht3x_repeatability_t repeatability)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;

    /* Stop current periodic mode if active */
    if (sensor->periodic_active) {
        sht3x_send_command(sensor, SHT3X_CMD_BREAK);
        sensor->periodic_active = false;
    }

    /* Update settings */
    sensor->mode = mode;
    sensor->repeatability = repeatability;
    sensor->measure_delay_ms = get_measure_delay(repeatability);

    /* Start new periodic mode if requested */
    if (mode != SHT3X_MODE_SINGLE_SHOT) {
        uint16_t cmd = get_periodic_command(mode, repeatability);
        esp_err_t err = sht3x_send_command(sensor, cmd);
        if (err != ESP_OK) {
            return err;
        }
        sensor->periodic_active = true;
    }

    return ESP_OK;
}

esp_err_t sht3x_set_heater(sht3x_handle_t handle, sht3x_heater_t heater)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;

    uint16_t cmd = (heater == SHT3X_HEATER_ON) ? SHT3X_CMD_HEATER_ON : SHT3X_CMD_HEATER_OFF;
    esp_err_t err = sht3x_send_command(sensor, cmd);

    if (err == ESP_OK) {
        sensor->heater_enabled = (heater == SHT3X_HEATER_ON);
        ESP_LOGI(TAG, "Heater %s", sensor->heater_enabled ? "enabled" : "disabled");
    }

    return err;
}

bool sht3x_is_heater_on(sht3x_handle_t handle)
{
    if (!handle) {
        return false;
    }

    sht3x_status_t status;
    if (sht3x_read_status(handle, &status) != ESP_OK) {
        return false;
    }

    return status.heater_status;
}

esp_err_t sht3x_read_status(sht3x_handle_t handle, sht3x_status_t *status)
{
    if (!handle || !status) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;

    /* Clear output structure */
    memset(status, 0, sizeof(*status));

    /* Send status read command */
    esp_err_t err = sht3x_send_command(sensor, SHT3X_CMD_READ_STATUS);
    if (err != ESP_OK) {
        return err;
    }

    /* Read status (3 bytes: MSB, LSB, CRC) */
    uint8_t data[3];
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (sensor->addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, 3, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);

    err = i2c_master_cmd_begin(sensor->i2c_port, cmd_handle,
                                pdMS_TO_TICKS(SHT3X_DEFAULT_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd_handle);

    if (err != ESP_OK) {
        return err;
    }

    /* Verify CRC */
    if (sht3x_crc8(data, 2) != data[2]) {
        ESP_LOGW(TAG, "Status CRC error");
        return ESP_ERR_INVALID_CRC;
    }

    /* Parse status word */
    uint16_t status_word = ((uint16_t)data[0] << 8) | data[1];

    status->checksum_error = (status_word & 0x0001) != 0;
    status->command_error = (status_word & 0x0002) != 0;
    status->reset_detected = (status_word & 0x0010) != 0;
    status->alert_humidity_high = (status_word & 0x0800) != 0;
    status->alert_humidity_low = (status_word & 0x0400) != 0;
    status->alert_temp_high = (status_word & 0x0200) != 0;
    status->alert_temp_low = (status_word & 0x0100) != 0;
    status->heater_status = (status_word & 0x2000) != 0;

    return ESP_OK;
}

esp_err_t sht3x_clear_status(sht3x_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;
    return sht3x_send_command(sensor, SHT3X_CMD_CLEAR_STATUS);
}

esp_err_t sht3x_soft_reset(sht3x_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;

    esp_err_t err = sht3x_send_command(sensor, SHT3X_CMD_SOFT_RESET);
    if (err != ESP_OK) {
        return err;
    }

    /* Wait for reset to complete */
    vTaskDelay(pdMS_TO_TICKS(SHT3X_RESET_DELAY_MS));

    /* Reset internal state */
    sensor->periodic_active = false;
    sensor->heater_enabled = false;

    return ESP_OK;
}

bool sht3x_is_present(sht3x_handle_t handle)
{
    if (!handle) {
        return false;
    }

    /* Try to read status register */
    sht3x_status_t status;
    esp_err_t err = sht3x_read_status(handle, &status);
    return (err == ESP_OK);
}

esp_err_t sht3x_get_last_error(sht3x_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht3x_sensor *sensor = (struct sht3x_sensor *)handle;
    return sensor->last_error;
}
