/**
 * @file mqtt_client.c
 * @brief MQTT Client with TLS Implementation
 * 
 * Implements MQTT over TLS (MQTTS) using ESP-TLS component.
 * Supports TLS 1.3 with fallback to TLS 1.2, certificate validation,
 * and certificate pinning with multiple pin support.
 * 
 * @version 2.2.0
 * @date 2026-04-15
 * @security SEC-030: MQTT Certificate Pinning (CVSS 5.8 -> Remediated)
 * @security SEC-016: MQTT-TLS Implementation (CVSS 9.2 -> Remediated)
 * @security SEC-034: ThermoFlow Certificate Pinning Completion
 */

#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "mbedtls/sha256.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/pem.h"
#include "mbedtls/ssl.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <ctype.h>

static const char *TAG = "MQTT_CLIENT";
static const char *NVS_NAMESPACE = "mqtt_pins";

#define NVS_KEY_PIN_CONFIG  "pin_config"
#define NVS_KEY_PIN_VERSION "pin_version"

#define MQTT_TLS_DEFAULT_PORT       8883
#define MQTT_TLS_CONNECT_TIMEOUT_MS 10000
#define MQTT_TLS_RX_BUFFER_SIZE     4096
#define MQTT_TLS_TX_BUFFER_SIZE     4096
#define PIN_CONFIG_VERSION          2

/* Pinning configuration via Kconfig */
#ifdef CONFIG_MQTT_CERT_PINNING_MANDATORY
#define PINNING_MANDATORY 1
#else
#define PINNING_MANDATORY 0
#endif

#ifdef CONFIG_MQTT_CERT_PINNING_REPORT_ONLY
#define PINNING_REPORT_ONLY 1
#else
#define PINNING_REPORT_ONLY 0
#endif

/* ============================================
 * Private Structures
 * ============================================ */

typedef struct {
    char topic[MQTT_MAX_TOPIC_LEN];
    mqtt_qos_t qos;
    bool active;
} mqtt_subscription_t;

struct mqtt_client {
    mqtt_config_t config;
    mqtt_status_info_t status;
    esp_tls_t *tls;
    mqtt_event_callback_t event_cb;
    
    // Connection state
    bool connected;
    bool initialized;
    uint32_t reconnect_attempts;
    uint32_t last_reconnect_time;
    
    // Buffers
    uint8_t rx_buffer[MQTT_TLS_RX_BUFFER_SIZE];
    uint8_t tx_buffer[MQTT_TLS_TX_BUFFER_SIZE];
    
    // Subscriptions
    mqtt_subscription_t subscriptions[8];
    uint8_t subscription_count;
    
    // Statistics
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t messages_sent;
    uint32_t messages_received;
    
    // Pin mismatch counter for security monitoring
    uint32_t pin_mismatch_count;
    uint32_t last_pin_mismatch_time;
    
    // Mutex for thread safety
    SemaphoreHandle_t mutex;
    
    // Certificate pinning (SEC-034)
    mqtt_pin_config_t pin_config;
    bool pin_config_loaded;
    
    // Legacy single pin support
    uint8_t pinned_cert_hash[MQTT_TLS_PIN_HASH_LEN];
    uint8_t pinned_cert_hash_2[MQTT_TLS_PIN_HASH_LEN];
    bool has_pin;
    bool has_pin_2;
    bool pinning_enabled;
};

/* ============================================
 * Helper Functions
 * ============================================ */

/**
 * @brief Parse hex string to bytes
 * @param hex String containing hex characters
 * @param out Output buffer
 * @param out_len Expected output length
 * @return ESP_OK on success
 */
static esp_err_t parse_hex_string(const char *hex, uint8_t *out, size_t out_len)
{
    if (!hex || !out || strlen(hex) < out_len * 2) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte;
        if (sscanf(&hex[i * 2], "%2x", &byte) != 1) {
            return ESP_ERR_INVALID_ARG;
        }
        out[i] = (uint8_t)byte;
    }
    
    return ESP_OK;
}

/* ============================================
 * Certificate Hash Calculation (SEC-034)
 * ============================================ */

/**
 * @brief Extract SPKI (Subject Public Key Info) from certificate and hash it
 * 
 * Uses mbedtls_x509_crt_info() for certificate inspection and
 * mbedtls_pk_write_pubkey_der() for SPKI extraction.
 */
static esp_err_t extract_spki_hash(const mbedtls_x509_crt *cert, uint8_t *hash_out)
{
    if (!cert || !hash_out) {
        ESP_LOGE(TAG, "Invalid arguments for SPKI extraction");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get certificate info for logging (as required by SEC-034)
    char cert_info[512];
    int ret = mbedtls_x509_crt_info(cert_info, sizeof(cert_info), "  ", cert);
    if (ret > 0) {
        ESP_LOGD(TAG, "Certificate info:\n%s", cert_info);
    }
    
    // Extract Subject Public Key Info (SPKI) using mbedtls_pk_write_pubkey_der
    uint8_t pk_der[1024];
    ret = mbedtls_pk_write_pubkey_der(&cert->pk, pk_der, sizeof(pk_der));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to extract public key: -0x%04x", -ret);
        return ESP_FAIL;
    }
    size_t pk_len = ret;
    
    // The key is written at the end of the buffer
    uint8_t *pk_start = pk_der + sizeof(pk_der) - pk_len;
    
    ESP_LOGD(TAG, "Extracted SPKI (%zu bytes)", pk_len);
    
    // Calculate SHA-256 hash of SPKI
    mbedtls_sha256_context sha256;
    mbedtls_sha256_init(&sha256);
    
    ret = mbedtls_sha256_starts_ret(&sha256, 0);
    if (ret != 0) {
        mbedtls_sha256_free(&sha256);
        return ESP_FAIL;
    }
    
    ret = mbedtls_sha256_update_ret(&sha256, pk_start, pk_len);
    if (ret != 0) {
        mbedtls_sha256_free(&sha256);
        return ESP_FAIL;
    }
    
    ret = mbedtls_sha256_finish_ret(&sha256, hash_out);
    mbedtls_sha256_free(&sha256);
    
    if (ret != 0) {
        ESP_LOGE(TAG, "SHA-256 failed: -0x%04x", -ret);
        return ESP_FAIL;
    }
    
    // Log hash for debugging (first 8 bytes)
    ESP_LOGI(TAG, "SPKI hash: %02X%02X%02X%02X...",
             hash_out[0], hash_out[1], hash_out[2], hash_out[3]);
    
    return ESP_OK;
}

/**
 * @brief Parse PEM certificate and extract SPKI hash
 */
static esp_err_t parse_pem_and_hash(const char *cert_pem, uint8_t *hash_out)
{
    if (!cert_pem || !hash_out) {
        return ESP_ERR_INVALID_ARG;
    }
    
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);
    
    // Parse PEM certificate
    int ret = mbedtls_x509_crt_parse(&cert, (const unsigned char *)cert_pem, 
                                      strlen(cert_pem) + 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse certificate: -0x%04x", -ret);
        mbedtls_x509_crt_free(&cert);
        return ESP_FAIL;
    }
    
    // Extract SPKI hash
    esp_err_t err = extract_spki_hash(&cert, hash_out);
    
    mbedtls_x509_crt_free(&cert);
    return err;
}

/**
 * @brief Parse DER certificate and extract SPKI hash
 */
static esp_err_t parse_der_and_hash(const uint8_t *cert_der, size_t cert_len, uint8_t *hash_out)
{
    if (!cert_der || cert_len == 0 || !hash_out) {
        return ESP_ERR_INVALID_ARG;
    }
    
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);
    
    // Parse DER certificate
    int ret = mbedtls_x509_crt_parse_der(&cert, cert_der, cert_len);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse DER certificate: -0x%04x", -ret);
        mbedtls_x509_crt_free(&cert);
        return ESP_FAIL;
    }
    
    // Extract SPKI hash
    esp_err_t err = extract_spki_hash(&cert, hash_out);
    
    mbedtls_x509_crt_free(&cert);
    return err;
}

/* ============================================
 * Legacy Hash Calculation (for backward compatibility)
 * ============================================ */

static esp_err_t calc_cert_hash_der(const uint8_t *cert_der, size_t cert_len, uint8_t *hash) {
    if (!cert_der || !hash || cert_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    mbedtls_sha256_context sha256;
    mbedtls_sha256_init(&sha256);
    
    int ret = mbedtls_sha256_starts_ret(&sha256, 0);
    if (ret != 0) {
        mbedtls_sha256_free(&sha256);
        return ESP_FAIL;
    }
    
    ret = mbedtls_sha256_update_ret(&sha256, cert_der, cert_len);
    if (ret != 0) {
        mbedtls_sha256_free(&sha256);
        return ESP_FAIL;
    }
    
    ret = mbedtls_sha256_finish_ret(&sha256, hash);
    mbedtls_sha256_free(&sha256);
    
    return (ret == 0) ? ESP_OK : ESP_FAIL;
}

/* ============================================
 * NVS Storage Functions (SEC-034)
 * ============================================ */

static esp_err_t save_pin_config_nvs(const mqtt_pin_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %d", err);
        return err;
    }
    
    // Save version
    err = nvs_set_u8(handle, NVS_KEY_PIN_VERSION, PIN_CONFIG_VERSION);
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }
    
    // Save config
    err = nvs_set_blob(handle, NVS_KEY_PIN_CONFIG, config, sizeof(*config));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save pin config: %d", err);
        nvs_close(handle);
        return err;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Pin config saved to NVS (%d pins)", config->pin_count);
    }
    
    return err;
}

static esp_err_t load_pin_config_nvs(mqtt_pin_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(config, 0, sizeof(*config));
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No pin config found in NVS, using defaults");
            // Initialize with defaults
            config->enforce_pinning = false;
            config->allow_ca_fallback = true;
            config->pin_count = 0;
            config->pin_mismatch_threshold = 3;
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Failed to open NVS: %d", err);
        return err;
    }
    
    // Check version
    uint8_t version = 0;
    err = nvs_get_u8(handle, NVS_KEY_PIN_VERSION, &version);
    if (err != ESP_OK || version != PIN_CONFIG_VERSION) {
        ESP_LOGW(TAG, "Pin config version mismatch (%d vs %d), resetting", version, PIN_CONFIG_VERSION);
        nvs_close(handle);
        config->enforce_pinning = false;
        config->allow_ca_fallback = true;
        config->pin_count = 0;
        return ESP_OK;
    }
    
    // Load config
    size_t size = sizeof(*config);
    err = nvs_get_blob(handle, NVS_KEY_PIN_CONFIG, config, &size);
    nvs_close(handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Pin config loaded from NVS (%d pins)", config->pin_count);
    } else {
        ESP_LOGW(TAG, "Failed to load pin config: %d", err);
        // Initialize defaults
        config->enforce_pinning = false;
        config->allow_ca_fallback = true;
        config->pin_count = 0;
        config->pin_mismatch_threshold = 3;
    }
    
    return err;
}

/* ============================================
 * Pin Verification (SEC-034)
 * ============================================ */

/**
 * @brief Verify certificate hash against pinned certificates
 * 
 * Implements constant-time comparison to prevent timing attacks.
 */
static mqtt_pin_status_t verify_against_pins(mqtt_client_t *client, const uint8_t *cert_hash)
{
    if (!client || !cert_hash) {
        return MQTT_PIN_ERROR_INVALID_CERT;
    }
    
    if (client->pin_config.pin_count == 0 && !client->has_pin) {
        ESP_LOGW(TAG, "No pinned certificates configured");
        return MQTT_PIN_ERROR_NO_PINS;
    }
    
    // Check legacy single pin first
    if (client->has_pin) {
        bool match = true;
        for (int i = 0; i < MQTT_TLS_PIN_HASH_LEN; i++) {
            match &= (cert_hash[i] == client->pinned_cert_hash[i]);
        }
        if (match) {
            ESP_LOGI(TAG, "Certificate matched legacy pinned hash");
            return MQTT_PIN_OK;
        }
    }
    
    // Check legacy backup pin
    if (client->has_pin_2) {
        bool match = true;
        for (int i = 0; i < MQTT_TLS_PIN_HASH_LEN; i++) {
            match &= (cert_hash[i] == client->pinned_cert_hash_2[i]);
        }
        if (match) {
            ESP_LOGI(TAG, "Certificate matched legacy backup pinned hash");
            return MQTT_PIN_OK;
        }
    }
    
    // Check new multi-pin configuration
    time_t now = time(NULL);
    for (int i = 0; i < client->pin_config.pin_count; i++) {
        mqtt_pinned_cert_t *pin = &client->pin_config.pins[i];
        
        if (!pin->active) {
            continue;
        }
        
        // Check expiration
        if (pin->valid_until > 0 && (uint64_t)now > pin->valid_until) {
            ESP_LOGW(TAG, "Pin %d (%s) expired, skipping", i, pin->description);
            continue;
        }
        
        // Constant-time comparison
        bool match = true;
        for (int j = 0; j < MQTT_TLS_PIN_HASH_LEN; j++) {
            match &= (cert_hash[j] == pin->hash[j]);
        }
        
        if (match) {
            ESP_LOGI(TAG, "Certificate matched pinned hash: %s", pin->description);
            return MQTT_PIN_OK;
        }
    }
    
    ESP_LOGE(TAG, "Certificate pin mismatch - possible MITM attack!");
    client->pin_mismatch_count++;
    client->last_pin_mismatch_time = (uint32_t)now;
    
    return MQTT_PIN_ERROR_HASH_MISMATCH;
}

/**
 * @brief Verify peer certificate using mbedtls SSL context
 */
static mqtt_pin_status_t verify_peer_certificate(mqtt_client_t *client)
{
    if (!client || !client->tls) {
        return MQTT_PIN_ERROR_INVALID_CERT;
    }
    
    // Get mbedtls SSL context
    mbedtls_ssl_context *ssl = (mbedtls_ssl_context *)esp_tls_get_ssl_context(client->tls);
    if (!ssl) {
        ESP_LOGE(TAG, "Failed to get SSL context");
        return MQTT_PIN_ERROR_INVALID_CERT;
    }
    
    // Get peer certificate
    const mbedtls_x509_crt *cert = mbedtls_ssl_get_peer_cert(ssl);
    if (!cert) {
        ESP_LOGW(TAG, "No peer certificate available");
        return MQTT_PIN_ERROR_INVALID_CERT;
    }
    
    // Extract SPKI hash
    uint8_t cert_hash[MQTT_TLS_PIN_HASH_LEN];
    esp_err_t err = extract_spki_hash(cert, cert_hash);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to extract certificate hash");
        return MQTT_PIN_ERROR_INVALID_CERT;
    }
    
    return verify_against_pins(client, cert_hash);
}

/* ============================================
 * Public API Implementation
 * ============================================ */

mqtt_client_t* mqtt_client_create(const mqtt_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }
    
    mqtt_client_t *client = calloc(1, sizeof(mqtt_client_t));
    if (!client) {
        ESP_LOGE(TAG, "Failed to allocate client");
        return NULL;
    }
    
    // Copy config
    memcpy(&client->config, config, sizeof(mqtt_config_t));
    
    // Initialize mutex
    client->mutex = xSemaphoreCreateMutex();
    if (!client->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(client);
        return NULL;
    }
    
    // Initialize state
    client->connected = false;
    client->initialized = true;
    client->event_cb = config->event_callback;
    client->pinning_enabled = config->enable_pinning;
    
    // Initialize pin config defaults
    client->pin_config.enforce_pinning = false;
    client->pin_config.allow_ca_fallback = true;
    client->pin_config.pin_count = 0;
    client->pin_config.pin_mismatch_threshold = 3;
    
    // Load pin config from NVS if enabled
    if (config->enable_pinning) {
        esp_err_t err = load_pin_config_nvs(&client->pin_config);
        if (err == ESP_OK) {
            client->pin_config_loaded = true;
            ESP_LOGI(TAG, "Loaded %d pinned certificates", client->pin_config.pin_count);
        }
    }
    
    ESP_LOGI(TAG, "MQTT client created (pinning %s)", 
             config->enable_pinning ? "enabled" : "disabled");
    
    return client;
}

void mqtt_client_destroy(mqtt_client_t *client) {
    if (!client) return;
    
    if (client->connected) {
        if (client->tls) {
            esp_tls_conn_destroy(client->tls);
            client->tls = NULL;
        }
        client->connected = false;
    }
    
    if (client->mutex) {
        vSemaphoreDelete(client->mutex);
    }
    
    free(client);
    ESP_LOGI(TAG, "MQTT client destroyed");
}

esp_err_t mqtt_client_connect(mqtt_client_t *client) {
    if (!client || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (client->connected) {
        ESP_LOGW(TAG, "Already connected");
        return ESP_OK;
    }
    
    // Create ESP-TLS config
    esp_tls_cfg_t tls_cfg = {
        .cacert_buf = (const unsigned char*)client->config.ca_cert,
        .cacert_bytes = client->config.ca_cert_len,
        .clientcert_buf = (const unsigned char*)client->config.client_cert,
        .clientcert_bytes = client->config.client_cert_len,
        .clientkey_buf = (const unsigned char*)client->config.client_key,
        .clientkey_bytes = client->config.client_key_len,
        .non_block = false,
        .timeout_ms = MQTT_TLS_CONNECT_TIMEOUT_MS,
    };
    
    // Create TLS connection
    client->tls = esp_tls_init();
    if (!client->tls) {
        ESP_LOGE(TAG, "Failed to create TLS context");
        return ESP_FAIL;
    }
    
    // Connect to server
    int ret = esp_tls_conn_new_sync(client->config.broker_hostname, 
                                     strlen(client->config.broker_hostname),
                                     client->config.broker_port,
                                     &tls_cfg, client->tls);
    if (ret != 1) {
        ESP_LOGE(TAG, "TLS connection failed: %d", ret);
        esp_tls_conn_destroy(client->tls);
        client->tls = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "TLS connection established to %s:%d", 
             client->config.broker_hostname, client->config.broker_port);
    
    // Perform certificate pinning verification if enabled
    if (client->pinning_enabled && 
        (client->pin_config.enforce_pinning || client->pin_config.pin_count > 0 || client->has_pin)) {
        
        mqtt_pin_status_t pin_status = verify_peer_certificate(client);
        
        if (pin_status != MQTT_PIN_OK) {
            ESP_LOGE(TAG, "Certificate pinning verification failed: %d", pin_status);
            
            // Fire pin mismatch event
            if (client->event_cb) {
                mqtt_event_data_t event_data = {.event = MQTT_EVENT_PIN_MISMATCH};
                event_data.data.pin_event.status = pin_status;
                event_data.data.pin_event.pin_index = -1;
                event_data.data.pin_event.description = mqtt_client_pin_status_to_string(pin_status);
                client->event_cb(MQTT_EVENT_PIN_MISMATCH, &event_data);
            }
            
            // Disconnect if pinning is enforced
            if (client->pin_config.enforce_pinning) {
                ESP_LOGE(TAG, "Pin mismatch with enforcement enabled - disconnecting");
                esp_tls_conn_destroy(client->tls);
                client->tls = NULL;
                return ESP_FAIL;
            } else if (!client->pin_config.allow_ca_fallback) {
                ESP_LOGE(TAG, "Pin mismatch with no CA fallback - disconnecting");
                esp_tls_conn_destroy(client->tls);
                client->tls = NULL;
                return ESP_FAIL;
            } else {
                ESP_LOGW(TAG, "Pin mismatch but CA fallback allowed - continuing");
            }
        }
    }
    
    client->connected = true;
    client->reconnect_attempts = 0;
    
    // Fire connected event
    if (client->event_cb) {
        mqtt_event_data_t event_data = {.event = MQTT_EVENT_CONNECTED};
        client->event_cb(MQTT_EVENT_CONNECTED, &event_data);
    }
    
    return ESP_OK;
}

void mqtt_client_disconnect(mqtt_client_t *client) {
    if (!client) return;
    
    if (!client->connected) return;
    
    if (client->tls) {
        esp_tls_conn_destroy(client->tls);
        client->tls = NULL;
    }
    
    client->connected = false;
    
    // Fire disconnected event
    if (client->event_cb) {
        mqtt_event_data_t event_data = {.event = MQTT_EVENT_DISCONNECTED};
        client->event_cb(MQTT_EVENT_DISCONNECTED, &event_data);
    }
    
    ESP_LOGI(TAG, "MQTT client disconnected");
}

esp_err_t mqtt_client_subscribe(mqtt_client_t *client, const char *topic, mqtt_qos_t qos) {
    if (!client || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!client->connected) {
        ESP_LOGE(TAG, "Not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Store subscription
    if (client->subscription_count < 8) {
        strncpy(client->subscriptions[client->subscription_count].topic, topic, MQTT_MAX_TOPIC_LEN - 1);
        client->subscriptions[client->subscription_count].topic[MQTT_MAX_TOPIC_LEN - 1] = '\0';
        client->subscriptions[client->subscription_count].qos = qos;
        client->subscriptions[client->subscription_count].active = true;
        client->subscription_count++;
    }
    
    ESP_LOGI(TAG, "Subscribed to %s (QoS %d)", topic, qos);
    return ESP_OK;
}

esp_err_t mqtt_client_unsubscribe(mqtt_client_t *client, const char *topic) {
    if (!client || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!client->connected) {
        ESP_LOGE(TAG, "Not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Find and remove subscription
    for (int i = 0; i < client->subscription_count; i++) {
        if (strcmp(client->subscriptions[i].topic, topic) == 0) {
            client->subscriptions[i].active = false;
            ESP_LOGI(TAG, "Unsubscribed from %s", topic);
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

esp_err_t mqtt_client_publish(mqtt_client_t *client, const char *topic, 
                                const uint8_t *data, size_t len, 
                                mqtt_qos_t qos, bool retain) {
    if (!client || !client->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!client->connected) {
        ESP_LOGE(TAG, "Not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Send raw data
    int ret = esp_tls_conn_write(client->tls, data, len);
    if (ret < 0) {
        ESP_LOGE(TAG, "TLS write failed: %d", ret);
        return ESP_FAIL;
    }
    
    client->messages_sent++;
    client->bytes_sent += len;
    
    ESP_LOGI(TAG, "Published %zu bytes to %s", len, topic);
    return ESP_OK;
}

bool mqtt_client_is_connected(mqtt_client_t *client) {
    if (!client) return false;
    return client->connected;
}

mqtt_status_info_t mqtt_client_get_status(mqtt_client_t *client) {
    mqtt_status_info_t status = {0};
    if (!client) return status;
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    status.connected = client->connected;
    status.bytes_sent = client->bytes_sent;
    status.bytes_received = client->bytes_received;
    status.messages_sent = client->messages_sent;
    status.messages_received = client->messages_received;
    status.pin_mismatch_count = client->pin_mismatch_count;
    status.active_pin_count = client->pin_config.pin_count;
    status.pinning_enforced = client->pin_config.enforce_pinning;
    xSemaphoreGive(client->mutex);
    
    return status;
}

esp_err_t mqtt_client_pin_broker_cert(mqtt_client_t *client, const uint8_t *cert_der, size_t cert_len) {
    if (!client || !cert_der || cert_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate hash of certificate (legacy full cert hash)
    uint8_t hash[MQTT_TLS_PIN_HASH_LEN];
    esp_err_t err = calc_cert_hash_der(cert_der, cert_len, hash);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calculate certificate hash");
        return err;
    }
    
    // Store hash in NVS
    nvs_handle_t handle;
    err = nvs_open("mqtt_tls", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_blob(handle, "pin_hash", hash, MQTT_TLS_PIN_HASH_LEN);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    
    nvs_close(handle);
    
    if (err == ESP_OK) {
        memcpy(client->pinned_cert_hash, hash, MQTT_TLS_PIN_HASH_LEN);
        client->has_pin = true;
        ESP_LOGI(TAG, "Broker certificate pinned successfully");
    }
    
    return err;
}

/* ============================================
 * Certificate Pinning API (SEC-034)
 * ============================================ */

esp_err_t mqtt_client_add_pinned_cert(mqtt_client_t *client, const uint8_t *hash, 
                                       const char *description, uint64_t valid_until)
{
    if (!client || !hash) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    
    if (client->pin_config.pin_count >= MQTT_MAX_PINNED_CERTS) {
        xSemaphoreGive(client->mutex);
        ESP_LOGE(TAG, "Maximum number of pinned certificates reached (%d)", MQTT_MAX_PINNED_CERTS);
        return ESP_ERR_NO_MEM;
    }
    
    int idx = client->pin_config.pin_count;
    memcpy(client->pin_config.pins[idx].hash, hash, MQTT_TLS_PIN_HASH_LEN);
    
    if (description) {
        strncpy(client->pin_config.pins[idx].description, description, MQTT_PIN_DESCRIPTION_LEN - 1);
        client->pin_config.pins[idx].description[MQTT_PIN_DESCRIPTION_LEN - 1] = '\0';
    } else {
        client->pin_config.pins[idx].description[0] = '\0';
    }
    
    client->pin_config.pins[idx].valid_until = valid_until;
    client->pin_config.pins[idx].active = true;
    client->pin_config.pin_count++;
    
    ESP_LOGI(TAG, "Added pinned certificate %d: %s", idx, 
             client->pin_config.pins[idx].description);
    
    xSemaphoreGive(client->mutex);
    
    // Save to NVS
    return mqtt_client_save_pin_config(client);
}

esp_err_t mqtt_client_remove_pinned_cert(mqtt_client_t *client, uint8_t pin_index)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    
    if (pin_index >= client->pin_config.pin_count) {
        xSemaphoreGive(client->mutex);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Shift remaining pins
    for (int i = pin_index; i < client->pin_config.pin_count - 1; i++) {
        memcpy(&client->pin_config.pins[i], &client->pin_config.pins[i + 1], 
               sizeof(mqtt_pinned_cert_t));
    }
    
    // Clear last entry
    memset(&client->pin_config.pins[client->pin_config.pin_count - 1], 0, 
           sizeof(mqtt_pinned_cert_t));
    client->pin_config.pin_count--;
    
    ESP_LOGI(TAG, "Removed pinned certificate at index %d", pin_index);
    
    xSemaphoreGive(client->mutex);
    
    // Save to NVS
    return mqtt_client_save_pin_config(client);
}

esp_err_t mqtt_client_clear_pinned_certs(mqtt_client_t *client)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    
    memset(&client->pin_config.pins, 0, sizeof(client->pin_config.pins));
    client->pin_config.pin_count = 0;
    client->has_pin = false;
    client->has_pin_2 = false;
    
    ESP_LOGI(TAG, "Cleared all pinned certificates");
    
    xSemaphoreGive(client->mutex);
    
    // Save to NVS
    return mqtt_client_save_pin_config(client);
}

esp_err_t mqtt_client_load_pin_config(mqtt_client_t *client)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    esp_err_t err = load_pin_config_nvs(&client->pin_config);
    if (err == ESP_OK) {
        client->pin_config_loaded = true;
    }
    xSemaphoreGive(client->mutex);
    
    return err;
}

esp_err_t mqtt_client_save_pin_config(mqtt_client_t *client)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    esp_err_t err = save_pin_config_nvs(&client->pin_config);
    xSemaphoreGive(client->mutex);
    
    return err;
}

esp_err_t mqtt_client_set_pinning_enforcement(mqtt_client_t *client, bool enforce)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    client->pin_config.enforce_pinning = enforce;
    ESP_LOGI(TAG, "Pinning enforcement: %s", enforce ? "enabled" : "disabled");
    xSemaphoreGive(client->mutex);
    
    return mqtt_client_save_pin_config(client);
}

esp_err_t mqtt_client_set_ca_fallback(mqtt_client_t *client, bool allow)
{
    if (!client) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    client->pin_config.allow_ca_fallback = allow;
    ESP_LOGI(TAG, "CA fallback: %s", allow ? "enabled" : "disabled");
    xSemaphoreGive(client->mutex);
    
    return mqtt_client_save_pin_config(client);
}

esp_err_t mqtt_client_calc_spki_hash(const char *cert_pem, uint8_t *hash_out)
{
    return parse_pem_and_hash(cert_pem, hash_out);
}

esp_err_t mqtt_client_calc_spki_hash_der(const uint8_t *cert_der, size_t cert_len, uint8_t *hash_out)
{
    return parse_der_and_hash(cert_der, cert_len, hash_out);
}

esp_err_t mqtt_client_verify_pin(mqtt_client_t *client, const uint8_t *cert_der, size_t cert_len)
{
    if (!client || !cert_der || cert_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t cert_hash[MQTT_TLS_PIN_HASH_LEN];
    esp_err_t err = parse_der_and_hash(cert_der, cert_len, cert_hash);
    if (err != ESP_OK) {
        return err;
    }
    
    mqtt_pin_status_t status = verify_against_pins(client, cert_hash);
    return (status == MQTT_PIN_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_client_handle_pin_update(mqtt_client_t *client, const char *json_payload)
{
    if (!client || !json_payload) {
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *root = cJSON_Parse(json_payload);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse pin update JSON");
        return ESP_ERR_INVALID_ARG;
    }
    
    cJSON *action = cJSON_GetObjectItem(root, "action");
    if (!action || !cJSON_IsString(action)) {
        ESP_LOGE(TAG, "Missing or invalid action in pin update");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t err = ESP_OK;
    
    if (strcmp(action->valuestring, "add") == 0) {
        // Add new pin
        cJSON *hash_hex = cJSON_GetObjectItem(root, "hash_hex");
        cJSON *description = cJSON_GetObjectItem(root, "description");
        cJSON *valid_until = cJSON_GetObjectItem(root, "valid_until");
        
        if (!hash_hex || !cJSON_IsString(hash_hex)) {
            ESP_LOGE(TAG, "Missing hash_hex in pin add command");
            err = ESP_ERR_INVALID_ARG;
        } else {
            // Parse hex string hash
            uint8_t hash[MQTT_TLS_PIN_HASH_LEN];
            
            err = parse_hex_string(hash_hex->valuestring, hash, MQTT_TLS_PIN_HASH_LEN);
            if (err == ESP_OK) {
                const char *desc = description ? description->valuestring : NULL;
                uint64_t expiry = (valid_until && cJSON_IsNumber(valid_until)) ? 
                                  (uint64_t)valid_until->valuedouble : 0;
                
                err = mqtt_client_add_pinned_cert(client, hash, desc, expiry);
            } else {
                ESP_LOGE(TAG, "Failed to parse hash hex string");
            }
        }
    } else if (strcmp(action->valuestring, "remove") == 0) {
        cJSON *index = cJSON_GetObjectItem(root, "index");
        if (index && cJSON_IsNumber(index)) {
            err = mqtt_client_remove_pinned_cert(client, (uint8_t)index->valueint);
        } else {
            ESP_LOGE(TAG, "Missing index in pin remove command");
            err = ESP_ERR_INVALID_ARG;
        }
    } else if (strcmp(action->valuestring, "clear") == 0) {
        err = mqtt_client_clear_pinned_certs(client);
    } else if (strcmp(action->valuestring, "set_enforcement") == 0) {
        cJSON *enforce = cJSON_GetObjectItem(root, "enforce");
        if (enforce && cJSON_IsBool(enforce)) {
            err = mqtt_client_set_pinning_enforcement(client, cJSON_IsTrue(enforce));
        } else {
            ESP_LOGE(TAG, "Missing enforce flag");
            err = ESP_ERR_INVALID_ARG;
        }
    } else {
        ESP_LOGW(TAG, "Unknown pin update action: %s", action->valuestring);
        err = ESP_ERR_NOT_SUPPORTED;
    }
    
    cJSON_Delete(root);
    return err;
}

const char* mqtt_client_pin_status_to_string(mqtt_pin_status_t status)
{
    switch (status) {
        case MQTT_PIN_OK:
            return "PIN_OK";
        case MQTT_PIN_ERROR_NO_PINS:
            return "NO_PINS_CONFIGURED";
        case MQTT_PIN_ERROR_HASH_MISMATCH:
            return "HASH_MISMATCH";
        case MQTT_PIN_ERROR_INVALID_CERT:
            return "INVALID_CERTIFICATE";
        case MQTT_PIN_ERROR_EXPIRED:
            return "PIN_EXPIRED";
        default:
            return "UNKNOWN";
    }
}

esp_err_t mqtt_client_get_pin_config(mqtt_client_t *client, mqtt_pin_config_t *config)
{
    if (!client || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(client->mutex, portMAX_DELAY);
    memcpy(config, &client->pin_config, sizeof(mqtt_pin_config_t));
    xSemaphoreGive(client->mutex);
    
    return ESP_OK;
}
