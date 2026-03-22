/**
 * @file mqtt_client.c
 * @brief MQTT Client Stub Implementation
 */

#include "mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "MQTT_CLIENT";

esp_err_t mqtt_client_init(void)
{
    ESP_LOGI(TAG, "MQTT client initialized (stub)");
    return ESP_OK;
}

esp_err_t mqtt_client_configure(const mqtt_config_t *config)
{
    return ESP_OK;
}

esp_err_t mqtt_client_start(void)
{
    return ESP_OK;
}

esp_err_t mqtt_client_stop(void)
{
    return ESP_OK;
}

esp_err_t mqtt_client_publish(const char *topic, const uint8_t *payload, size_t len, mqtt_qos_t qos, bool retain)
{
    return ESP_OK;
}

mqtt_status_t mqtt_client_get_status(void)
{
    return MQTT_STATUS_DISCONNECTED;
}

bool mqtt_client_is_connected(void)
{
    return false;
}

esp_err_t mqtt_client_deinit(void)
{
    return ESP_OK;
}