/**
 * @file mqtt_client.h
 * @brief MQTT Client Interface - ESP-IDF Native
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_MAX_TOPIC_LEN      128

typedef enum {
    MQTT_QOS_0 = 0,
    MQTT_QOS_1 = 1,
    MQTT_QOS_2 = 2
} mqtt_qos_t;

typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTING,
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_ERROR
} mqtt_status_t;

// Configuration
typedef struct {
    const char* broker_host;
    uint16_t port;
    const char* client_id;
    const char* username;
    const char* password;
    uint16_t keepalive_sec;
} mqtt_config_t;

esp_err_t mqtt_client_init(void);
esp_err_t mqtt_client_configure(const mqtt_config_t *config);
esp_err_t mqtt_client_start(void);
esp_err_t mqtt_client_stop(void);
esp_err_t mqtt_client_publish(const char *topic, const uint8_t *payload, size_t len, mqtt_qos_t qos, bool retain);
mqtt_status_t mqtt_client_get_status(void);
bool mqtt_client_is_connected(void);
esp_err_t mqtt_client_publish_sensor(uint8_t sensor_id, float temperature, float humidity, bool valid);
esp_err_t mqtt_client_publish_fan(uint8_t fan_id, uint8_t speed_percent, const char *mode);
esp_err_t mqtt_client_publish_system(uint32_t free_heap, uint32_t uptime_sec);
esp_err_t mqtt_client_publish_condensation_alert(float rh, bool active);
esp_err_t mqtt_client_loop(void);
esp_err_t mqtt_client_deinit(void);

#ifdef __cplusplus
}
#endif

#endif