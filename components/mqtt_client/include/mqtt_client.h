/**
 * @file mqtt_client.h
 * @brief MQTT Client Interface - ESP-IDF Native with TLS Support
 * 
 * Implements MQTT over TLS (MQTTS) with certificate validation and pinning.
 * Supports multiple pinned certificates for rotation and fallback.
 * Complies with IEC 62443 security requirements.
 * 
 * @version 2.2.0
 * @date 2026-04-15
 * @security SEC-030: MQTT Certificate Pinning (CVSS 5.8 -> Remediated)
 * @security SEC-016: MQTT-TLS Implementation (CVSS 9.2 -> Remediated)
 * @security SEC-034: ThermoFlow Certificate Pinning Completion
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
#define MQTT_MAX_PINNED_CERTS           5
#define MQTT_PIN_DESCRIPTION_LEN        32

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
    MQTT_EVENT_ERROR,
    MQTT_EVENT_PIN_MISMATCH
} mqtt_event_t;

/**
 * @brief Certificate pinning error codes
 */
typedef enum {
    MQTT_PIN_OK = 0,
    MQTT_PIN_ERROR_NO_PINS,
    MQTT_PIN_ERROR_HASH_MISMATCH,
    MQTT_PIN_ERROR_INVALID_CERT,
    MQTT_PIN_ERROR_EXPIRED
} mqtt_pin_status_t;

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
        struct {
            mqtt_pin_status_t status;
            int pin_index;
            const char *description;
        } pin_event;
    } data;
} mqtt_event_data_t;

/**
 * @brief MQTT event callback
 */
typedef void (*mqtt_event_callback_t)(mqtt_event_t event, const mqtt_event_data_t *data);

/**
 * @brief Pinned certificate entry
 */
typedef struct {
    uint8_t hash[MQTT_TLS_PIN_HASH_LEN];
    char description[MQTT_PIN_DESCRIPTION_LEN];
    uint64_t valid_until;
    bool active;
} mqtt_pinned_cert_t;

/**
 * @brief Certificate pinning configuration
 */
typedef struct {
    mqtt_pinned_cert_t pins[MQTT_MAX_PINNED_CERTS];
    uint8_t pin_count;
    bool enforce_pinning;
    bool allow_ca_fallback;
    uint32_t pin_mismatch_threshold;
} mqtt_pin_config_t;

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
    
    // Certificate pinning
    bool enable_pinning;
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
    uint8_t active_pin_count;
    bool pinning_enforced;
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

/* ============================================
 * Certificate Pinning Functions (SEC-034)
 * ============================================ */

/**
 * @brief Pin broker certificate for validation (legacy single pin)
 * @param client Client handle
 * @param cert_der Certificate in DER format
 * @param cert_len Certificate length
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_pin_broker_cert(mqtt_client_t *client, const uint8_t *cert_der, size_t cert_len);

/**
 * @brief Add a pinned certificate hash
 * @param client Client handle
 * @param hash SPKI SHA-256 hash (32 bytes)
 * @param description Human-readable description
 * @param valid_until Unix timestamp when pin expires (0 for no expiry)
 * @return ESP_OK on success, ESP_ERR_NO_MEM if max pins reached
 */
esp_err_t mqtt_client_add_pinned_cert(mqtt_client_t *client, const uint8_t *hash, 
                                       const char *description, uint64_t valid_until);

/**
 * @brief Remove a pinned certificate by index
 * @param client Client handle
 * @param pin_index Index of pin to remove
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_remove_pinned_cert(mqtt_client_t *client, uint8_t pin_index);

/**
 * @brief Clear all pinned certificates
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_clear_pinned_certs(mqtt_client_t *client);

/**
 * @brief Load pinning configuration from NVS
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_load_pin_config(mqtt_client_t *client);

/**
 * @brief Save pinning configuration to NVS
 * @param client Client handle
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_save_pin_config(mqtt_client_t *client);

/**
 * @brief Enable or disable certificate pinning enforcement
 * @param client Client handle
 * @param enforce true to reject connections on pin mismatch
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_set_pinning_enforcement(mqtt_client_t *client, bool enforce);

/**
 * @brief Enable or disable CA fallback when pinning fails
 * @param client Client handle
 * @param allow true to allow CA validation if pinning fails
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_set_ca_fallback(mqtt_client_t *client, bool allow);

/**
 * @brief Calculate SPKI hash from certificate PEM
 * @param cert_pem Certificate in PEM format
 * @param hash_out Output buffer (32 bytes for SHA-256)
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_calc_spki_hash(const char *cert_pem, uint8_t *hash_out);

/**
 * @brief Calculate SPKI hash from certificate DER
 * @param cert_der Certificate in DER format
 * @param cert_len Certificate length
 * @param hash_out Output buffer (32 bytes for SHA-256)
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_calc_spki_hash_der(const uint8_t *cert_der, size_t cert_len, uint8_t *hash_out);

/**
 * @brief Verify certificate against pinned hashes
 * @param client Client handle
 * @param cert_der Certificate in DER format
 * @param cert_len Certificate length
 * @return ESP_OK if pin matches, ESP_FAIL if mismatch
 */
esp_err_t mqtt_client_verify_pin(mqtt_client_t *client, const uint8_t *cert_der, size_t cert_len);

/**
 * @brief Handle pin update command via MQTT
 * @param client Client handle
 * @param json_payload JSON payload with pin update command
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_handle_pin_update(mqtt_client_t *client, const char *json_payload);

/**
 * @brief Get pin status as string
 * @param status Pin status code
 * @return Human-readable status string
 */
const char* mqtt_client_pin_status_to_string(mqtt_pin_status_t status);

/**
 * @brief Get current pin configuration
 * @param client Client handle
 * @param config Output configuration structure
 * @return ESP_OK on success
 */
esp_err_t mqtt_client_get_pin_config(mqtt_client_t *client, mqtt_pin_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_CLIENT_H */
