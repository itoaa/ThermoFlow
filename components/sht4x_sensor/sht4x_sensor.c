/**
 * @file sht4x_sensor.c
 * @brief SHT40 Temperature and Humidity Sensor Implementation
 */

#include <string.h>
#include <math.h>
#include "sht4x_sensor.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "SHT4X";

#define SHT4X_MEASURE_DELAY_HIGH_MS     9
#define SHT4X_MEASURE_DELAY_MEDIUM_MS   5
#define SHT4X_MEASURE_DELAY_LOW_MS      2

// CRC-8 calculation for SHT4x (polynomial 0x31)
static uint8_t sht4x_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x31) : (crc << 1);
        }
    }
    return crc;
}

// Convert ticks to temperature
static float sht4x_ticks_to_temperature(uint16_t ticks)
{
    return -45.0f + 175.0f * ((float)ticks / 65535.0f);
}

// Convert ticks to humidity
static float sht4x_ticks_to_humidity(uint16_t ticks)
{
    float rh = -6.0f + 125.0f * ((float)ticks / 65535.0f);
    return (rh < 0.0f) ? 0.0f : ((rh > 100.0f) ? 100.0f : rh);
}

// Internal sensor structure
struct sht4x_sensor {
    uint8_t i2c_port;
    uint8_t addr;
    uint8_t precision;
    uint32_t measure_delay_ms;
    esp_err_t last_error;
};

esp_err_t sht4x_init(const sht4x_config_t *config, sht4x_handle_t *handle)
{
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct sht4x_sensor *sensor = calloc(1, sizeof(struct sht4x_sensor));
    if (!sensor) {
        return ESP_ERR_NO_MEM;
    }

    sensor->i2c_port = config->i2c_port;
    sensor->addr = config->addr;
    sensor->precision = config->precision;

    // Set measurement delay based on precision
    switch (config->precision) {
        case 0:
            sensor->measure_delay_ms = SHT4X_MEASURE_DELAY_LOW_MS;
            break;
        case 1:
            sensor->measure_delay_ms = SHT4X_MEASURE_DELAY_MEDIUM_MS;
            break;
        default:
            sensor->measure_delay_ms = SHT4X_MEASURE_DELAY_HIGH_MS;
            break;
    }

    // Configure I2C if not already done
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
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {  // Invalid state = already installed
        free(sensor);
        return err;
    }

    // Verify sensor is present
    if (!sht4x_is_present(sensor)) {
        ESP_LOGE(TAG, "Sensor not found at address 0x%02X", sensor->addr);
        free(sensor);
        return ESP_ERR_NOT_FOUND;
    }

    *handle = sensor;
    ESP_LOGI(TAG, "SHT40 initialized at 0x%02X", sensor->addr);
    return ESP_OK;
}

esp_err_t sht4x_deinit(sht4x_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    free(handle);
    return ESP_OK;
}

esp_err_t sht4x_read(sht4x_handle_t handle, sht4x_reading_t *reading, uint32_t timeout_ms)
{
    if (!handle || !reading) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(reading, 0, sizeof(*reading));
    reading->temperature = NAN;
    reading->humidity = NAN;

    // Select measurement command
    sht4x_command_t cmd;
    switch (handle->precision) {
        case 0: cmd = SHT4X_CMD_MEASURE_LOW_PRECISION; break;
        case 1: cmd = SHT4X_CMD_MEASURE_MEDIUM_PRECISION; break;
        default: cmd = SHT4X_CMD_MEASURE_HIGH_PRECISION; break;
    }

    // Send measurement command
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (handle->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, cmd, true);
    i2c_master_stop(cmd_handle);

    esp_err_t err = i2c_master_cmd_begin(handle->i2c_port, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);

    if (err != ESP_OK) {
        handle->last_error = err;
        return err;
    }

    // Wait for measurement
    uint32_t wait_time = (timeout_ms < handle->measure_delay_ms) ? 
                         handle->measure_delay_ms : timeout_ms;
    vTaskDelay(pdMS_TO_TICKS(wait_time));

    // Read 6 bytes: temp MSB, temp LSB, temp CRC, hum MSB, hum LSB, hum CRC
    uint8_t data[6];
    cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (handle->addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);

    err = i2c_master_cmd_begin(handle->i2c_port, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);

    if (err != ESP_OK) {
        handle->last_error = err;
        return err;
    }

    // Verify CRC
    if (sht4x_crc8(data, 2) != data[2]) {
        ESP_LOGW(TAG, "Temperature CRC error");
        handle->last_error = ESP_ERR_INVALID_CRC;
        return ESP_ERR_INVALID_CRC;
    }
    if (sht4x_crc8(data + 3, 2) != data[5]) {
        ESP_LOGW(TAG, "Humidity CRC error");
        handle->last_error = ESP_ERR_INVALID_CRC;
        return ESP_ERR_INVALID_CRC;
    }

    // Convert data
    uint16_t temp_ticks = (data[0] << 8) | data[1];
    uint16_t hum_ticks = (data[3] << 8) | data[4];

    reading->temperature = sht4x_ticks_to_temperature(temp_ticks);
    reading->humidity = sht4x_ticks_to_humidity(hum_ticks);
    reading->timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    reading->valid = true;

    handle->last_error = ESP_OK;
    return ESP_OK;
}

esp_err_t sht4x_soft_reset(sht4x_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SHT4X_CMD_SOFT_RESET, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    vTaskDelay(pdMS_TO_TICKS(10));  // Wait for reset

    handle->last_error = err;
    return err;
}

esp_err_t sht4x_read_serial(sht4x_handle_t handle, uint32_t *serial)
{
    if (!handle || !serial) {
        return ESP_ERR_INVALID_ARG;
    }

    // Send read serial command
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, SHT4X_CMD_READ_SERIAL, true);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        handle->last_error = err;
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    // Read serial (4 bytes + 2 CRC)
    uint8_t data[6];
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (handle->addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    err = i2c_master_cmd_begin(handle->i2c_port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK) {
        handle->last_error = err;
        return err;
    }

    // Verify CRCs
    if (sht4x_crc8(data, 2) != data[2] || sht4x_crc8(data + 3, 2) != data[5]) {
        handle->last_error = ESP_ERR_INVALID_CRC;
        return ESP_ERR_INVALID_CRC;
    }

    *serial = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
              ((uint32_t)data[3] << 8) | (uint32_t)data[4];

    handle->last_error = ESP_OK;
    return ESP_OK;
}

bool sht4x_is_present(sht4x_handle_t handle)
{
    if (!handle) {
        return false;
    }

    // Try to read serial number as presence check
    uint32_t serial;
    esp_err_t err = sht4x_read_serial(handle, &serial);
    return (err == ESP_OK);
}

esp_err_t sht4x_get_last_error(sht4x_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    return handle->last_error;
}