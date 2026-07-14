/**
 * @file wifi_manager.h
 * @brief WiFi Manager for ThermoFlow - AP Mode Setup with MAC-based naming
 * 
 * First boot: Creates AP "ThermoFlow-XXXX" (last 4 hex of MAC)
 * User connects, configures WiFi via web interface
 * Credentials saved to NVS (encrypted), device reboots and connects to configured WiFi
 * 
 * Security: SEC-021 - WiFi credentials encrypted using AES-256-CBC
 * Encryption: Credentials encrypted with device-unique key derivation
 * 
 * Features:
 * - AP mode with unique name based on MAC address
 * - Web-based WiFi configuration
 * - WiFi credentials saved to encrypted NVS (flash)
 * - Automatic reconnection on boot
 * - Fallback to AP mode if connection fails
 * - Secure credential migration from legacy plaintext storage
 * 
 * @author Ola Andersson
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-021
 */

#ifndef WIFI_MANAGER_H

#include "wifi_types.h"
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"  /* For wifi_sta_config_t compatibility */
#include "wifi_secure_storage.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define WIFI_AP_SSID_PREFIX     "ThermoFlow-"   // Will be suffixed with last 4 MAC hex
#define WIFI_AP_PASSWORD        ""              // Open network (no password for AP)
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_MAX_CONNECTIONS 1
#define WIFI_CONFIG_WEB_PORT    80

/* NVS namespace for WiFi credentials */
#define WIFI_NVS_NAMESPACE      "wifi_config"
#define WIFI_NVS_KEY_SSID       "ssid"
#define WIFI_NVS_KEY_PASSWORD   "password"

/* NVS namespace for device identity */
#define DEVICE_NVS_NAMESPACE    "device_config"
#define DEVICE_NVS_KEY_NAME       "name"
#define DEVICE_NAME_MAX_LEN       32

/* WiFi connection timeout (seconds) — routers may need time after reboot */
#define WIFI_CONNECT_TIMEOUT_S  90

/* STA reconnect attempts before AP+STA fallback (credentials kept in NVS) */
#define WIFI_MAX_RETRIES        15

/**
 * @brief WiFi manager states
 */
typedef enum {
    WIFI_STATE_INIT = 0,           // Initializing
    WIFI_STATE_AP_MODE,              // Running in AP mode (setup)
    WIFI_STATE_CONNECTING,           // Connecting to configured WiFi
    WIFI_STATE_CONNECTED,            // Successfully connected
    WIFI_STATE_DISCONNECTED,         // Connection lost
    WIFI_STATE_ERROR                 // Error state
} wifi_manager_state_t;

/**
 * @brief WiFi configuration structure (ThermoFlow custom)
 * 
 * Renamed to avoid conflict with esp_wifi.h wifi_config_t union
 */
typedef struct {
    char ssid[33];                   // WiFi SSID (max 32 chars + null)
    char password[65];               // WiFi password (max 64 chars + null)
    bool configured;                 // true if credentials saved
} tf_wifi_config_t;

/**
 * @brief WiFi manager statistics
 */
typedef struct {
    wifi_manager_state_t state;      // Current state
    uint32_t connection_attempts;    // Number of connection attempts
    uint32_t connection_failures;    // Number of failed connections
    int8_t rssi;                     // Signal strength (when connected)
    char ap_name[32];                // AP name used (with MAC suffix)
    char ip_address[16];             // Assigned IP address
} wifi_manager_status_t;

/**
 * @brief Initialize WiFi manager
 * 
 * Checks NVS for saved credentials:
 * - If found: Attempts to connect to configured WiFi
 * - If not found: Starts AP mode for configuration
 * 
 * Credentials are stored using AES-256 encryption (SEC-021).
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start WiFi manager main loop
 * 
 * Should be called after init. Handles state machine:
 * - Connection monitoring
 * - Automatic reconnection
 * - Fallback to AP mode
 * 
 * This function can be called periodically or run in a task.
 */
void wifi_manager_run(void);

/**
 * @brief Get current WiFi manager status
 * 
 * @param status Pointer to status structure to fill
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_status(wifi_manager_status_t *status);

/**
 * @brief Store WiFi credentials in NVS (encrypted when available)
 *
 * @param ssid WiFi SSID
 * @param password WiFi password (may be empty string for open networks)
 * @return ESP_OK if credentials saved successfully
 */
esp_err_t wifi_manager_store_credentials(const char *ssid, const char *password);

/**
 * @brief Configure WiFi credentials
 * 
 * Saves credentials to encrypted NVS and reboots for STA connection.
 * 
 * Security: Credentials are encrypted with AES-256 before storage.
 * 
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK if credentials saved successfully (does not return after reboot)
 */
esp_err_t wifi_manager_configure(const char *ssid, const char *password);

/**
 * @brief Reset WiFi configuration
 * 
 * Clears saved credentials from encrypted NVS.
 * Device will start in AP mode on next boot.
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_reset(void);

/**
 * @brief Check if WiFi is currently connected
 * 
 * @return true if connected to WiFi, false otherwise
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Check if WiFi credentials are stored in NVS (even if not currently connected)
 */
bool wifi_manager_has_saved_credentials(void);

/**
 * @brief Check if running in AP mode (setup mode)
 * 
 * @return true if in AP mode, false otherwise
 */
bool wifi_manager_is_ap_mode(void);

/**
 * @brief Check if AP+STA fallback is active (saved credentials, still retrying home WiFi)
 */
bool wifi_manager_is_ap_fallback_mode(void);

/**
 * @brief Get saved WiFi SSID (empty if none stored)
 */
esp_err_t wifi_manager_get_saved_ssid(char *ssid, size_t len);

/**
 * @brief Get the immutable device ID / AP SSID suffix
 *
 * Format: "ThermoFlow-XXXX" where XXXX is last 4 hex digits of MAC.
 * Derived from factory MAC and never user-editable.
 *
 * @return Device ID string (static buffer)
 */
const char* wifi_manager_get_ap_name(void);

/**
 * @brief Get the resolved name shown in legacy APIs (display name or device ID)
 *
 * @param name Output buffer
 * @param len Buffer size
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_device_name(char *name, size_t len);

/**
 * @brief Get the user-facing display name (custom label or device ID)
 *
 * @param name Output buffer
 * @param len Buffer size
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_display_name(char *name, size_t len);

/**
 * @brief Check whether a custom display name is stored in NVS
 */
bool wifi_manager_has_custom_name(void);

/**
 * @brief Set a custom display name (persisted in NVS)
 *
 * Must not use the ThermoFlow-XXXX device ID format. Updates DHCP hostname.
 *
 * @param name Display name (1-32 chars, alphanumeric and hyphen)
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_set_device_name(const char *name);

/**
 * @brief Check if credentials are stored with encryption
 * 
 * @return true if encrypted credentials exist
 */
bool wifi_manager_has_encrypted_credentials(void);

/**
 * @brief Get encryption status
 * 
 * @param[out] encrypted Set to true if using encrypted storage
 * @param[out] key_source Current key source type
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_get_encryption_status(bool *encrypted, wifi_key_source_t *key_source);

/**
 * @brief Deinitialize WiFi manager
 * 
 * Stops WiFi and cleans up resources.
 */
void wifi_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H
