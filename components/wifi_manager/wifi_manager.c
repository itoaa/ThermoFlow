/**
 * @file wifi_manager.c
 * @brief WiFi Manager Implementation - AP Mode Setup with MAC-based naming
 * 
 * Security: SEC-021 - WiFi credentials now stored encrypted in NVS
 * Encryption: AES-256-CBC with HMAC-SHA256 integrity protection
 * 
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-021
 */

#include "wifi_manager.h"
#include "wifi_secure_storage.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI_MGR";

/* FreeRTOS event group to signal connection status */
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

/* WiFi configuration storage */
static tf_wifi_config_t s_wifi_config = {0};
static wifi_manager_status_t s_status = {0};
static esp_netif_t *s_wifi_netif = NULL;

/* AP name with MAC suffix */
static char s_ap_name[32] = {0};
static char s_custom_device_name[DEVICE_NAME_MAX_LEN + 1] = {0};

/* Feature flags */
static bool s_use_encrypted_storage = true;
static bool s_secure_storage_initialized = false;

/* Forward declarations */
static esp_err_t wifi_init_nvs(void);
static esp_err_t wifi_load_config(void);
static esp_err_t wifi_save_legacy_config(void);
static esp_err_t wifi_ensure_legacy_backup(void);
static esp_err_t wifi_save_config(void);
static void wifi_generate_ap_name(void);
static esp_err_t wifi_read_sta_mac(uint8_t mac[6]);
static void wifi_sanitize_custom_device_name(void);
static esp_err_t wifi_load_device_name(void);
static esp_err_t wifi_save_device_name(const char *name);
static void wifi_apply_hostname(void);
static bool wifi_is_valid_device_name(const char *name);
static esp_err_t wifi_start_ap(void);
static esp_err_t wifi_connect(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

esp_err_t wifi_manager_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi manager (SEC-021: encrypted storage)");
    
    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize NVS
    esp_err_t ret = wifi_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize secure storage for credentials
    ret = wifi_secure_storage_init(WIFI_KEY_SOURCE_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Secure storage init failed (%s), falling back to plaintext", 
                 esp_err_to_name(ret));
        s_use_encrypted_storage = false;
    } else {
        s_secure_storage_initialized = true;
        ESP_LOGI(TAG, "Secure WiFi storage initialized (encrypted)");
        
        // Attempt to migrate legacy credentials
        wifi_secure_migrate_from_legacy();
    }
    
    // Initialize TCP/IP stack
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Netif init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create default event loop
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event loop init failed: %s", esp_err_to_name(ret));
        // Continue anyway - might already exist
    }
    
    // Device ID from factory MAC (works before esp_wifi_init)
    wifi_generate_ap_name();
    strncpy(s_status.ap_name, s_ap_name, sizeof(s_status.ap_name) - 1);
    wifi_load_device_name();
    wifi_sanitize_custom_device_name();
    
    // Load saved configuration
    ret = wifi_load_config();
    if (ret == ESP_OK && s_wifi_config.configured) {
        ESP_LOGI(TAG, "Found saved WiFi config, attempting connection");
        s_status.state = WIFI_STATE_CONNECTING;
        
        // Initialize WiFi in station mode
        ret = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // Register event handlers
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
        
        // Try to connect
        ret = wifi_connect();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Connection failed, will start AP mode");
            esp_wifi_stop();
            esp_wifi_deinit();
            s_status.state = WIFI_STATE_AP_MODE;
            return wifi_start_ap();
        }
        
        // Wait for connection with timeout
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_S * 1000));
        
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to WiFi");
            s_status.state = WIFI_STATE_CONNECTED;
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "Connection timeout, starting AP mode");
            esp_wifi_stop();
            esp_wifi_deinit();
            s_status.state = WIFI_STATE_AP_MODE;
            return wifi_start_ap();
        }
    } else {
        ESP_LOGI(TAG, "No saved WiFi config, starting AP mode");
        s_status.state = WIFI_STATE_AP_MODE;
        return wifi_start_ap();
    }
}

static esp_err_t wifi_init_nvs(void) {
    /* NVS is initialized once in app_main(); avoid a second erase here. */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NOT_INITIALIZED) {
        ESP_LOGE(TAG, "NVS not initialized before WiFi manager");
    }
    return (ret == ESP_ERR_NVS_NOT_INITIALIZED) ? ret : ESP_OK;
}

static esp_err_t wifi_read_sta_mac(uint8_t mac[6]) {
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read STA MAC: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void wifi_generate_ap_name(void) {
    uint8_t mac[6] = {0};

    if (wifi_read_sta_mac(mac) != ESP_OK) {
        snprintf(s_ap_name, sizeof(s_ap_name), "%s0000", WIFI_AP_SSID_PREFIX);
        ESP_LOGW(TAG, "Using fallback device ID: %s", s_ap_name);
        return;
    }

    /* Format: ThermoFlow-XXXX (last 4 hex digits of MAC) */
    snprintf(s_ap_name, sizeof(s_ap_name), "%s%02X%02X",
             WIFI_AP_SSID_PREFIX, mac[4], mac[5]);

    ESP_LOGI(TAG, "Device ID: %s (MAC %02X:%02X:%02X:%02X:%02X:%02X)",
             s_ap_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void wifi_sanitize_custom_device_name(void) {
    if (s_custom_device_name[0] == '\0') {
        return;
    }

    if (strcmp(s_custom_device_name, s_ap_name) == 0) {
        ESP_LOGI(TAG, "Dropping redundant custom name '%s'", s_custom_device_name);
        goto clear_custom;
    }

    /* Clear stale ThermoFlow-XXXX names from old MAC read bug */
    const size_t prefix_len = strlen(WIFI_AP_SSID_PREFIX);
    if (strncmp(s_custom_device_name, WIFI_AP_SSID_PREFIX, prefix_len) == 0 &&
        strlen(s_custom_device_name) == prefix_len + 4) {
        ESP_LOGW(TAG, "Clearing stale MAC-style name '%s' (device ID is %s)",
                 s_custom_device_name, s_ap_name);
        goto clear_custom;
    }

    return;

clear_custom:
    s_custom_device_name[0] = '\0';
    nvs_handle_t nvs_handle;
    if (nvs_open(DEVICE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_erase_key(nvs_handle, DEVICE_NVS_KEY_NAME);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

static bool wifi_is_valid_device_name(const char *name) {
    if (!name || name[0] == '\0' || strlen(name) > DEVICE_NAME_MAX_LEN) {
        return false;
    }
    for (const char *p = name; *p; p++) {
        if (!((*p >= 'A' && *p <= 'Z') ||
              (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') ||
              *p == '-')) {
            return false;
        }
    }
    return true;
}

static esp_err_t wifi_load_device_name(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(DEVICE_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t name_len = sizeof(s_custom_device_name);
    ret = nvs_get_str(nvs_handle, DEVICE_NVS_KEY_NAME, s_custom_device_name, &name_len);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded custom device name: %s", s_custom_device_name);
    } else {
        s_custom_device_name[0] = '\0';
    }
    return ret;
}

static esp_err_t wifi_save_device_name(const char *name) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(DEVICE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(nvs_handle, DEVICE_NVS_KEY_NAME, name);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);
    return ret;
}

static void wifi_apply_hostname(void) {
    if (!s_wifi_netif) {
        return;
    }

    const char *hostname = s_custom_device_name[0] ? s_custom_device_name : s_ap_name;
    esp_err_t ret = esp_netif_set_hostname(s_wifi_netif, hostname);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Hostname set to %s", hostname);
    } else {
        ESP_LOGW(TAG, "Failed to set hostname: %s", esp_err_to_name(ret));
    }
}

static esp_err_t wifi_load_config(void) {
    // Try encrypted storage first
    if (s_use_encrypted_storage && s_secure_storage_initialized) {
        if (wifi_secure_has_credentials()) {
            ESP_LOGI(TAG, "Loading encrypted WiFi credentials");
            
            esp_err_t ret = wifi_secure_load_credentials(
                s_wifi_config.ssid, sizeof(s_wifi_config.ssid),
                s_wifi_config.password, sizeof(s_wifi_config.password));
            
            if (ret == ESP_OK) {
                s_wifi_config.configured = true;
                ESP_LOGI(TAG, "Loaded encrypted WiFi config: SSID=%s", s_wifi_config.ssid);
                wifi_ensure_legacy_backup();
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "Failed to load encrypted credentials: %s", esp_err_to_name(ret));
                // Fall through to legacy loading
            }
        }
    }
    
    // Legacy plaintext loading (for backwards compatibility)
    ESP_LOGW(TAG, "Falling back to legacy plaintext storage");
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No NVS namespace found, using defaults");
        return ret;
    }
    
    size_t ssid_len = sizeof(s_wifi_config.ssid);
    ret = nvs_get_str(nvs_handle, WIFI_NVS_KEY_SSID, s_wifi_config.ssid, &ssid_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    size_t pass_len = sizeof(s_wifi_config.password);
    ret = nvs_get_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, s_wifi_config.password, &pass_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }
    
    s_wifi_config.configured = true;
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Loaded legacy WiFi config: SSID=%s", s_wifi_config.ssid);
    
    // Attempt migration to encrypted storage
    if (s_secure_storage_initialized) {
        ESP_LOGI(TAG, "Migrating legacy credentials to encrypted storage");
        wifi_secure_migrate_from_legacy();
    }
    
    return ESP_OK;
}

static esp_err_t wifi_save_legacy_config(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_str(nvs_handle, WIFI_NVS_KEY_SSID, s_wifi_config.ssid);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, s_wifi_config.password);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return ret;
}

static esp_err_t wifi_ensure_legacy_backup(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return wifi_save_legacy_config();
    }

    char existing[33] = {0};
    size_t len = sizeof(existing);
    ret = nvs_get_str(nvs_handle, WIFI_NVS_KEY_SSID, existing, &len);
    nvs_close(nvs_handle);

    if (ret == ESP_OK && existing[0] != '\0') {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Restoring legacy NVS backup for WiFi credentials");
    return wifi_save_legacy_config();
}

static esp_err_t wifi_save_config(void) {
    if (s_use_encrypted_storage && s_secure_storage_initialized) {
        ESP_LOGI(TAG, "Saving WiFi config with encryption");
        esp_err_t enc_ret = wifi_secure_store_credentials(s_wifi_config.ssid, s_wifi_config.password);
        if (enc_ret == ESP_OK) {
            esp_err_t legacy_ret = wifi_save_legacy_config();
            if (legacy_ret != ESP_OK) {
                ESP_LOGW(TAG, "Encrypted save OK but legacy backup failed: %s",
                         esp_err_to_name(legacy_ret));
            } else {
                ESP_LOGI(TAG, "WiFi credentials also backed up in legacy NVS");
            }
            return ESP_OK;
        }
        ESP_LOGW(TAG, "Encrypted save failed (%s), falling back to legacy NVS",
                 esp_err_to_name(enc_ret));
    }

    ESP_LOGW(TAG, "Saving WiFi config to legacy NVS");
    return wifi_save_legacy_config();
}

static esp_err_t wifi_start_ap(void) {
    ESP_LOGI(TAG, "Starting AP mode: %s", s_ap_name);
    
    // Initialize WiFi
    esp_err_t ret = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register event handler
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    
    // Create AP interface
    s_wifi_netif = esp_netif_create_default_wifi_ap();
    wifi_apply_hostname();
    
    // Configure AP using ESP-IDF wifi_config_t
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(s_ap_name),
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,  // No password for AP
        },
    };
    strncpy((char*)wifi_config.ap.ssid, s_ap_name, sizeof(wifi_config.ap.ssid) - 1);
    
    // Set IP address (192.168.4.1)
    esp_netif_ip_info_t ip_info = {
        .ip = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
        .gw = {.addr = ESP_IP4TOADDR(192, 168, 4, 1)},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
    };
    esp_netif_dhcps_stop(s_wifi_netif);
    esp_netif_set_ip_info(s_wifi_netif, &ip_info);
    esp_netif_dhcps_start(s_wifi_netif);
    
    // Start WiFi
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set mode failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set config failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "AP mode started. Connect to %s and open http://192.168.4.1", s_ap_name);
    s_status.state = WIFI_STATE_AP_MODE;
    strncpy(s_status.ip_address, "192.168.4.1", sizeof(s_status.ip_address) - 1);
    
    return ESP_OK;
}

static esp_err_t wifi_connect(void) {
    if (!s_wifi_config.configured) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Connecting to %s", s_wifi_config.ssid);
    
    // Create STA interface
    s_wifi_netif = esp_netif_create_default_wifi_sta();
    wifi_apply_hostname();
    
    // Configure WiFi using ESP-IDF wifi_config_t
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, s_wifi_config.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, s_wifi_config.password, sizeof(wifi_config.sta.password) - 1);
    
    esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) return ret;
    
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) return ret;
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) return ret;
    
    ret = esp_wifi_connect();
    if (ret != ESP_OK) return ret;
    
    return ESP_OK;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA started");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "STA disconnected");
                s_status.state = WIFI_STATE_DISCONNECTED;
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "STA connected to AP");
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started");
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "Station connected to AP, MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                         event->mac[0], event->mac[1], event->mac[2],
                         event->mac[3], event->mac[4], event->mac[5]);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "Station disconnected from AP");
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        snprintf(s_status.ip_address, sizeof(s_status.ip_address), IPSTR, IP2STR(&event->ip_info.ip));
        s_status.state = WIFI_STATE_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_manager_run(void) {
    // This function can be called periodically to monitor connection
    // For now, connection handling is done in event handler
    vTaskDelay(pdMS_TO_TICKS(1000));
}

esp_err_t wifi_manager_get_status(wifi_manager_status_t *status) {
    if (!status) return ESP_ERR_INVALID_ARG;
    memcpy(status, &s_status, sizeof(wifi_manager_status_t));
    return ESP_OK;
}

esp_err_t wifi_manager_store_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Storing WiFi credentials: SSID=%s", ssid);

    strncpy(s_wifi_config.ssid, ssid, sizeof(s_wifi_config.ssid) - 1);
    s_wifi_config.ssid[sizeof(s_wifi_config.ssid) - 1] = '\0';
    strncpy(s_wifi_config.password, password, sizeof(s_wifi_config.password) - 1);
    s_wifi_config.password[sizeof(s_wifi_config.password) - 1] = '\0';
    s_wifi_config.configured = true;

    return wifi_save_config();
}

esp_err_t wifi_manager_configure(const char *ssid, const char *password) {
    esp_err_t ret = wifi_manager_store_credentials(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi config saved, restarting...");
    esp_restart();

    return ESP_OK;
}

esp_err_t wifi_manager_reset(void) {
    ESP_LOGI(TAG, "Resetting WiFi configuration");
    
    // Delete from encrypted storage first
    if (s_secure_storage_initialized) {
        wifi_secure_delete_credentials();
    }
    
    // Also clear legacy storage
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_erase_key(nvs_handle, WIFI_NVS_KEY_SSID);
        nvs_erase_key(nvs_handle, WIFI_NVS_KEY_PASSWORD);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
    
    s_wifi_config.configured = false;
    memset(&s_wifi_config, 0, sizeof(s_wifi_config));
    
    ESP_LOGI(TAG, "WiFi config reset, restarting...");
    esp_restart();
    
    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    return (s_status.state == WIFI_STATE_CONNECTED);
}

bool wifi_manager_is_ap_mode(void) {
    return (s_status.state == WIFI_STATE_AP_MODE);
}

const char* wifi_manager_get_ap_name(void) {
    return s_ap_name;
}

esp_err_t wifi_manager_get_device_name(char *name, size_t len) {
    if (!name || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *resolved = s_custom_device_name[0] ? s_custom_device_name : s_ap_name;
    strncpy(name, resolved, len - 1);
    name[len - 1] = '\0';
    return ESP_OK;
}

esp_err_t wifi_manager_set_device_name(const char *name) {
    if (!wifi_is_valid_device_name(name)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Display names must not mimic the immutable MAC-based device ID */
    const size_t prefix_len = strlen(WIFI_AP_SSID_PREFIX);
    if (strncmp(name, WIFI_AP_SSID_PREFIX, prefix_len) == 0 &&
        strlen(name) == prefix_len + 4) {
        ESP_LOGW(TAG, "Rejecting display name '%s' (reserved device ID format)", name);
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_custom_device_name, name, sizeof(s_custom_device_name) - 1);
    s_custom_device_name[sizeof(s_custom_device_name) - 1] = '\0';

    esp_err_t ret = wifi_save_device_name(name);
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_apply_hostname();
    ESP_LOGI(TAG, "Display name updated to %s (device ID %s)", name, s_ap_name);
    return ESP_OK;
}

bool wifi_manager_has_custom_name(void) {
    return s_custom_device_name[0] != '\0';
}

esp_err_t wifi_manager_get_display_name(char *name, size_t len) {
    if (!name || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_custom_device_name[0] != '\0') {
        strncpy(name, s_custom_device_name, len - 1);
    } else {
        strncpy(name, s_ap_name, len - 1);
    }
    name[len - 1] = '\0';
    return ESP_OK;
}

bool wifi_manager_has_encrypted_credentials(void) {
    return s_secure_storage_initialized && wifi_secure_has_credentials();
}

esp_err_t wifi_manager_get_encryption_status(bool *encrypted, wifi_key_source_t *key_source) {
    if (!encrypted) return ESP_ERR_INVALID_ARG;
    return wifi_secure_get_status(encrypted, key_source);
}

void wifi_manager_deinit(void) {
    esp_wifi_stop();
    esp_wifi_deinit();
    
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
    
    wifi_secure_storage_deinit();
    s_secure_storage_initialized = false;
}
