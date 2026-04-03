/**
 * @file wifi_manager.c
 * @brief WiFi Manager Implementation - AP Mode Setup with MAC-based naming
 */

#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
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

/* Forward declarations */
static esp_err_t wifi_init_nvs(void);
static esp_err_t wifi_load_config(void);
static esp_err_t wifi_save_config(void);
static void wifi_generate_ap_name(void);
static esp_err_t wifi_start_ap(void);
static esp_err_t wifi_connect(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

esp_err_t wifi_manager_init(void) {
    ESP_LOGI(TAG, "Initializing WiFi manager");
    
    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize NVS
    esp_err_t ret = wifi_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
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
    
    // Generate AP name with MAC suffix
    wifi_generate_ap_name();
    strncpy(s_status.ap_name, s_ap_name, sizeof(s_status.ap_name) - 1);
    
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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static void wifi_generate_ap_name(void) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);  // Get STA MAC (same as AP MAC)
    
    // Format: ThermoFlow-XXXX (last 4 hex digits of MAC)
    snprintf(s_ap_name, sizeof(s_ap_name), "%s%02X%02X",
             WIFI_AP_SSID_PREFIX, mac[4], mac[5]);
    
    ESP_LOGI(TAG, "AP name: %s", s_ap_name);
}

static esp_err_t wifi_load_config(void) {
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
    
    ESP_LOGI(TAG, "Loaded WiFi config: SSID=%s", s_wifi_config.ssid);
    return ESP_OK;
}

static esp_err_t wifi_save_config(void) {
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
    
    return ESP_OK;
}

static esp_err_t wifi_connect(void) {
    if (!s_wifi_config.configured) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Connecting to %s", s_wifi_config.ssid);
    
    // Create STA interface
    s_wifi_netif = esp_netif_create_default_wifi_sta();
    
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

esp_err_t wifi_manager_configure(const char *ssid, const char *password) {
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "Configuring WiFi: SSID=%s", ssid);
    
    strncpy(s_wifi_config.ssid, ssid, sizeof(s_wifi_config.ssid) - 1);
    strncpy(s_wifi_config.password, password, sizeof(s_wifi_config.password) - 1);
    s_wifi_config.configured = true;
    
    esp_err_t ret = wifi_save_config();
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
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;
    
    nvs_erase_key(nvs_handle, WIFI_NVS_KEY_SSID);
    nvs_erase_key(nvs_handle, WIFI_NVS_KEY_PASSWORD);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
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

void wifi_manager_deinit(void) {
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }
}
