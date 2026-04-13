/**
 * @file mqtt_client.h
 * @brief MQTT Client Interface - ESP-IDF Native with TLS Support
 * 
 * Implements MQTT over TLS (MQTTS) with certificate validation and pinning.
 * Complies with IEC 62443 security requirements.
 * 
 * @version 2.1.0
 * @date 2026-04-13
 * @security SEC-030: MQTT Certificate Pinning (CVSS 5.8 -> Remediated)
 * @security SEC-016: MQTT-TLS Implementation (CVSS 9.2 -> Remediated)
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_MAX_TOPIC_LEN              128
#define MQTT_MAX_PAYLOAD_LEN            1024
#define MQTT_MAX_BROKER_LEN             128
#define MQTT_MAX_CLIENT_ID_LEN          64
#define MQTT_MAX_USERNAME_LEN           64
#define MQTT_MAX_PASSWORD_LEN           64
#define MQTT_MAX_CA_CERT_LEN            2048
#define MQTT_MAX_CLIENT_CERT_LEN        2048
#define MQTT_MAX_CLIENT_KEY_LEN         2048
#define MQTT_TLS_PIN_HASH_LEN           32

/**
 * @brief MQTT Quality of Service levels
 */
typedef enum {
    MQTT_QOS_0 = 0,
    MQTT_QOS_1 = 1,
    MQTT_QOS_2 = 2
} mqtt_qos_t;

/**
 * @brief MQTT event types
 */
typedef enum {
    MQTT_EVENT_CONNECTED = 0,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_RECEIVED,
    MQTT_EVENT_ERROR
} mqtt_event_t;

/**
 * @brief MQTT event data
 */
typedef struct {
    mqtt_event_t event;
    union {
        struct {
            const char *topic;
            const uint8_t *payload;
            size_t len;
        } received;
        struct {
            const char *error_msg;
            int error_code;
        } error;
    } data;
} mqtt_event_data_t;

/**
 * @brief MQTT event callback
 */
typedef void (*mqtt_event_callback_t)(mqtt_event_t event, const mqtt_event_data_t *data);

/**
 * @brief MQTT client configuration
 */
typedef struct {
    char broker_hostname[MQTT_MAX_BROKER_LEN];
    uint16_t broker_port;
    char client_id[MQTT_MAX_CLIENT_ID_LEN];
    char username[MQTT_MAX_USERNAME_LEN];
    char password[MQTT_MAX_PASSWORD_LEN];
    uint16_t keepalive_sec;
    
    // TLS settings
    const char *ca_cert;
    size_t ca_cert_len;
    const char *client_cert;
    size_t client_cert_len;
    const char *client_key;
    size_t client_key_len;
    
    // Callbacks
    mqtt_event_callback_t event_callback;
    
    // Timeouts
    uint32_t connect_timeout_ms;
    uint32_t reconnect_delay_ms;
} mqtt_config_t;

/**
 * @brief Opaque MQTT client handle
 */
typedef struct mqtt_client mqtt_client_t;

/**
 * @brief MQTT connection status structure
 */
typedef struct {
    bool connected;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t pin_mismatch_count;
} mqtt_status_info_t;

/* ============================================
 * Core MQTT Functions
 * ============================================ */

/**
 * @brief Create a new MQTT client instance
 * @param config Client configuration
 * @return Client handle or NULL on error
 */
mqtt_client_t* mqtt_client_create(const mqtt_config_t *config);

/**
 * @brief Destroy MQTT client instance
 * @param client Client handle
 */
void mqtt_client_destroy(mqtt_client_t *client);

/**
 * @brief Connect to MQTT broker
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_connect(mqtt_client_t *client);

/**
 * @brief Disconnect from MQTT broker
 * @param client Client handle
 */
void mqtt_client_disconnect(mqtt_client_t *client);

/**
 * @brief Subscribe to an MQTT topic
 * @param client Client handle
 * @param topic Topic to subscribe to
 * @param qos Quality of service level
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_subscribe(mqtt_client_t *client, const char *topic, mqtt_qos_t qos);

/**
 * @brief Unsubscribe from an MQTT topic
 * @param client Client handle
 * @param topic Topic to unsubscribe from
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_unsubscribe(mqtt_client_t *client, const char *topic);

/**
 * @brief Publish an MQTT message
 * @param client Client handle
 * @param topic Topic to publish to
 * @param data Message payload
 * @param len Payload length
 * @param qos Quality of service level
 * @param retain Whether to retain the message
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_publish(mqtt_client_t *client, const char *topic, 
                               const uint8_t *data, size_t len, 
                               mqtt_qos_t qos, bool retain);

/**
 * @brief Check if client is connected
 * @param client Client handle
 * @return true if connected
 */
bool mqtt_client_is_connected(mqtt_client_t *client);

/**
 * @brief Get current connection status
 * @param client Client handle
 * @return Status information
 */
mqtt_status_info_t mqtt_client_get_status(mqtt_client_t *client);

/**
 * @brief Pin broker certificate for validation
 * @param client Client handle
 * @param cert_der Certificate in DER format
 * @param cert_len Certificate length
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_pin_broker_cert(mqtt_client_t *client, const uint8_t *cert_der, size_t cert_len);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H */
