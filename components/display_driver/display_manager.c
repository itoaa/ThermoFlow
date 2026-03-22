/**
 * @file display_manager.c
 * @brief Display Manager Stub - ESP-IDF
 */

#include "display_manager.h"
#include "esp_log.h"

static const char *TAG = "DISPLAY";
static bool s_initialized = false;

esp_err_t display_manager_init(void)
{
    ESP_LOGI(TAG, "Display initialized (stub)");
    s_initialized = true;
    return ESP_OK;
}

esp_err_t display_update_sensors(const display_sensor_data_t *data)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    // Stub - would update OLED display
    return ESP_OK;
}

esp_err_t display_show_alert(const char *title, const char *message, uint32_t duration_ms)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    ESP_LOGW(TAG, "ALERT: %s - %s", title, message);
    return ESP_OK;
}

esp_err_t display_clear_alert(void)
{
    return ESP_OK;
}

esp_err_t display_set_power(bool on)
{
    return ESP_OK;
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