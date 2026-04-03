/**
 * @file wifi_manager.h
 * @brief WiFi Manager for ThermoFlow - AP Mode Setup with MAC-based naming
 * 
 * First boot: Creates AP "ThermoFlow-XXXX" (last 4 hex of MAC)
 * User connects, configures WiFi via web interface
 * Credentials saved to NVS, device reboots and connects to configured WiFi
 * 
 * Features:
 * - AP mode with unique name based on MAC address
 * - Web-based WiFi configuration
 * - WiFi credentials saved to NVS (flash)
 * - Automatic reconnection on boot
 * - Fallback to AP mode if connection fails
 * 
 * @author Ola Andersson
 * @version 1.0.0
 * @date 2026-04-03
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include "esp_wifi_types.h"  /* For wifi_sta_config_t compatibility */
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

/* WiFi connection timeout (seconds) */
#define WIFI_CONNECT_TIMEOUT_S  30

/* Number of retry attempts before falling back to AP mode */
#define WIFI_MAX_RETRIES        3

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
 * @brief Configure WiFi credentials
 * 
 * Saves credentials to NVS and attempts connection.
 * Will reboot device on success.
 * 
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK if credentials saved successfully
 */
esp_err_t wifi_manager_configure(const char *ssid, const char *password);

/**
 * @brief Reset WiFi configuration
 * 
 * Clears saved credentials from NVS.
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
 * @brief Check if running in AP mode (setup mode)
 * 
 * @return true if in AP mode, false otherwise
 */
bool wifi_manager_is_ap_mode(void);

/**
 * @brief Get the AP name being used
 * 
 * Format: "ThermoFlow-XXXX" where XXXX is last 4 hex digits of MAC
 * 
 * @return AP name string (static buffer)
 */
const char* wifi_manager_get_ap_name(void);

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
