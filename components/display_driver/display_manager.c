/**
 * @file display_manager.c
 * @brief SSD1306 OLED display driver (128x64 I2C)
 */

#include "display_manager.h"
#include "thermoflow_config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "DISPLAY";
static bool s_initialized = false;

#define SSD1306_ADDR          DISPLAY_I2C_ADDR
#define SSD1306_CMD          0x00
#define SSD1306_DATA         0x40

static esp_err_t ssd1306_write_cmd(uint8_t cmd)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (SSD1306_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, SSD1306_CMD, true);
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(handle);
    return ret;
}

static esp_err_t ssd1306_write_data(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (SSD1306_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, SSD1306_DATA, true);
    i2c_master_write(handle, (uint8_t *)data, len, true);
    i2c_master_stop(handle);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, handle, pdMS_TO_TICKS(200));
    i2c_cmd_link_delete(handle);
    return ret;
}

static esp_err_t ssd1306_init_panel(void)
{
    const uint8_t init_seq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };

    for (size_t i = 0; i < sizeof(init_seq); i++) {
        esp_err_t err = ssd1306_write_cmd(init_seq[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    return ESP_OK;
}

static void render_line(uint8_t page, const char *text)
{
    uint8_t buffer[128];
    memset(buffer, 0, sizeof(buffer));

    /* Simple 1-line bitmap font: store ASCII in page 0 only as column patterns */
    size_t len = strlen(text);
    if (len > 21) {
        len = 21;
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t ch = (uint8_t)text[i];
        for (int col = 0; col < 6; col++) {
            buffer[i * 6 + col] = (ch + col) & 0x7F;
        }
    }

    ssd1306_write_cmd(0xB0 | page);
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(0x10);
    ssd1306_write_data(buffer, sizeof(buffer));
}

esp_err_t display_manager_init(void)
{
    esp_err_t ret = ssd1306_init_panel();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SSD1306 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    render_line(0, "ThermoFlow ready");
    ESP_LOGI(TAG, "SSD1306 display initialized");
    return ESP_OK;
}

esp_err_t display_update_sensors(const display_sensor_data_t *data)
{
    if (!s_initialized || !data) {
        return ESP_ERR_INVALID_STATE;
    }

    char line[32];
    if (data->num_sensors > 0 && data->valid[0]) {
        snprintf(line, sizeof(line), "T:%.1f H:%.0f%%", data->temp[0], data->humidity[0]);
    } else {
        snprintf(line, sizeof(line), "No sensor data");
    }

    render_line(0, line);

    if (data->num_sensors > 1 && data->valid[1]) {
        snprintf(line, sizeof(line), "S2:%.1f/%.0f%%", data->temp[1], data->humidity[1]);
        render_line(2, line);
    }

    return ESP_OK;
}

esp_err_t display_show_alert(const char *title, const char *message, uint32_t duration_ms)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    (void)duration_ms;
    render_line(4, title ? title : "ALERT");
    render_line(6, message ? message : "");
    ESP_LOGW(TAG, "ALERT: %s - %s", title, message);
    return ESP_OK;
}

esp_err_t display_clear_alert(void)
{
    return ESP_OK;
}

esp_err_t display_set_power(bool on)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return ssd1306_write_cmd(on ? 0xAF : 0xAE);
}

bool display_is_available(void)
{
    return s_initialized;
}

esp_err_t display_manager_deinit(void)
{
    s_initialized = false;
    return ESP_OK;
}