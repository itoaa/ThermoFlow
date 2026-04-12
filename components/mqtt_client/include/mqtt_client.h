/**
 * @file mqtt_client.h
 * @brief MQTT Client Interface - ESP-IDF Native with TLS Support
 * 
 * Implements MQTT over TLS (MQTTS) with certificate validation and pinning.
 * Complies with IEC 62443 security requirements.
 * 
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-016: MQTT-TLS Implementation (CVSS 9.2 -> Remediated)
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
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
 * @brief MQTT connection status
 */
typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,
    MQTT_STATUS_CONNECTING,
    MQTT_STATUS_CONNECTED,
    MQTT_STATUS_ERROR_TLS,
    MQTT_STATUS_ERROR_CERT,
    MQTT_STATUS_ERROR_NETWORK
} mqtt_status_t;

/**
 * @brief TLS configuration for MQTT connection
 */
typedef struct {
    bool use_tls;                              /*!< Enable TLS encryption */
    bool verify_server_cert;                   /*!< Verify server certificate */
    bool skip_common_name_check;               /*!< Skip CN verification (dev only) */
    bool use_client_cert;                      /*!< Enable mTLS authentication */
    bool certificate_pinning;                  /*!< Enable certificate pinning */
    uint8_t pinned_cert_hash[MQTT_TLS_PIN_HASH_LEN]; /*!< SHA-256 hash of expected cert */
    const char *broker_hostname;               /*!< For hostname verification */
    const char *ca_cert;                       /*!< CA certificate (PEM) */
    size_t ca_cert_len;                        /*!< CA certificate length */
    const char *client_cert;                   /*!< Client certificate (PEM) */
    size_t client_cert_len;                    /*!< Client certificate length */
    const char *client_key;                    /*!< Client private key (PEM) */
    size_t client_key_len;                     /*!< Client private key length */
} mqtt_tls_config_t;

/**
 * @brief MQTT client configuration
 */
typedef struct {
    char broker_host[MQTT_MAX_BROKER_LEN];     /*!< MQTT broker hostname/IP */
    uint16_t port;                             /*!< Broker port (8883 for MQTTS) */
    char client_id[MQTT_MAX_CLIENT_ID_LEN];    /*!< Client identifier */
    char username[MQTT_MAX_USERNAME_LEN];      /*!< Authentication username */
    char password[MQTT_MAX_PASSWORD_LEN];      /*!< Authentication password */
    uint16_t keepalive_sec;                    /*!< Keepalive interval in seconds */
    mqtt_tls_config_t tls;                     /*!< TLS configuration */
    uint32_t connect_timeout_ms;               /*!< Connection timeout in ms */
    uint32_t reconnect_delay_ms;               /*!< Initial reconnect delay */
    uint32_t max_reconnect_delay_ms;           /*!< Maximum reconnect delay */
} mqtt_config_t;

/**
 * @brief MQTT message callback
 */
typedef void (*mqtt_message_callback_t)(const char *topic, const uint8_t *payload, size_t len);

/**
 * @brief MQTT connection callback
 */
typedef void (*mqtt_connect_callback_t)(bool connected, const char *error_msg);

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
 * @brief Opaque MQTT client handle
 */
typedef struct mqtt_client *mqtt_client_handle_t;

/* ============================================
 * Core MQTT Functions
 * ============================================ */

/**
 * @brief Initialize the MQTT client library
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_init(void);

/**
 * @brief Create a new MQTT client instance
 * @param[out] client Pointer to store client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_create(mqtt_client_handle_t *client);

/**
 * @brief Configure the MQTT client
 * @param client Client handle
 * @param config Configuration structure
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_configure(mqtt_client_handle_t client, const mqtt_config_t *config);

/**
 * @brief Start the MQTT client connection
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_start(mqtt_client_handle_t client);

/**
 * @brief Stop the MQTT client and disconnect
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_stop(mqtt_client_handle_t client);

/**
 * @brief Subscribe to an MQTT topic
 * @param client Client handle
 * @param topic Topic to subscribe to
 * @param qos Quality of service level
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_subscribe(mqtt_client_handle_t client, const char *topic, mqtt_qos_t qos);

/**
 * @brief Unsubscribe from an MQTT topic
 * @param client Client handle
 * @param topic Topic to unsubscribe from
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_unsubscribe(mqtt_client_handle_t client, const char *topic);

/**
 * @brief Publish an MQTT message
 * @param client Client handle
 * @param topic Topic to publish to
 * @param payload Message payload
 * @param len Payload length
 * @param qos Quality of service level
 * @param retain Whether to retain the message
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_publish(mqtt_client_handle_t client, const char *topic, 
                               const uint8_t *payload, size_t len, 
                               mqtt_qos_t qos, bool retain);

/**
 * @brief Get current connection status
 * @param client Client handle
 * @return Current MQTT status
 */
mqtt_status_t mqtt_client_get_status(mqtt_client_handle_t client);

/**
 * @brief Check if client is connected
 * @param client Client handle
 * @return true if connected
 */
bool mqtt_client_is_connected(mqtt_client_handle_t client);

/**
 * @brief Process MQTT events (call periodically)
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_loop(mqtt_client_handle_t client);

/**
 * @brief Set event callback
 * @param client Client handle
 * @param callback Event callback function
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_set_event_callback(mqtt_client_handle_t client, mqtt_event_callback_t callback);

/**
 * @brief Destroy MQTT client instance
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_destroy(mqtt_client_handle_t client);

/**
 * @brief Deinitialize the MQTT client library
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_deinit(void);

/* ============================================
 * TLS-Specific Functions
 * ============================================ */

/**
 * @brief Configure TLS from NVS (load certificates from secure storage)
 * @param tls_config TLS configuration structure to populate
 * @return ESP_OK on success
 * @note Loads CA cert and optionally client cert/key from NVS
 */
esp_err_t mqtt_tls_config_from_nvs(mqtt_tls_config_t *tls_config);

/**
 * @brief Set certificate pinning hash
 * @param tls_config TLS configuration structure
 * @param hash SHA-256 hash of expected certificate (32 bytes)
 * @return ESP_OK on success
 */
esp_err_t mqtt_tls_set_pinning(mqtt_tls_config_t *tls_config, const uint8_t *hash);

/**
 * @brief Calculate certificate hash for pinning
 * @param cert_pem Certificate in PEM format
 * @param[out] hash Output buffer (32 bytes)
 * @return ESP_OK on success
 */
esp_err_t mqtt_tls_calc_cert_hash(const char *cert_pem, uint8_t *hash);

/**
 * @brief Get TLS error description
 * @param status MQTT status code (MQTT_STATUS_ERROR_*)
 * @return Human-readable error description
 */
const char *mqtt_tls_get_error_string(mqtt_status_t status);

/* ============================================
 * Convenience Functions (Legacy API)
 * ============================================ */

/**
 * @brief Publish sensor data (legacy wrapper)
 * @param sensor_id Sensor identifier
 * @param temperature Temperature value
 * @param humidity Humidity value
 * @param valid Whether data is valid
 * @return ESP_OK on success
 * @deprecated Use mqtt_client_publish with JSON payload
 */
esp_err_t mqtt_client_publish_sensor(uint8_t sensor_id, float temperature, 
                                      float humidity, bool valid);

/**
 * @brief Publish fan data (legacy wrapper)
 * @param fan_id Fan identifier
 * @param speed_percent Fan speed percentage
 * @param mode Fan mode string
 * @return ESP_OK on success
 * @deprecated Use mqtt_client_publish with JSON payload
 */
esp_err_t mqtt_client_publish_fan(uint8_t fan_id, uint8_t speed_percent, const char *mode);

/**
 * @brief Publish system stats (legacy wrapper)
 * @param free_heap Free heap memory
 * @param uptime_sec System uptime in seconds
 * @return ESP_OK on success
 * @deprecated Use mqtt_client_publish with JSON payload
 */
esp_err_t mqtt_client_publish_system(uint32_t free_heap, uint32_t uptime_sec);

/**
 * @brief Publish condensation alert (legacy wrapper)
 * @param rh Relative humidity percentage
 * @param active Whether alert is active
 * @return ESP_OK on success
 * @deprecated Use mqtt_client_publish with JSON payload
 */
esp_err_t mqtt_client_publish_condensation_alert(float rh, bool active);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H */
