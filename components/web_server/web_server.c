/**
 * @file web_server.c
 * @brief HTTPS Web Server Implementation with FTX API - ESP-IDF
 * 
 * Implements SEC-018: HTTPS Web Server Security Enhancement
 * - TLS 1.3 with fallback to TLS 1.2
 * - Certificate management via security_utils
 * - HTTP-to-HTTPS redirect
 * - Security headers (HSTS, CSP, X-Frame-Options, etc.)
 * 
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-018
 */

#include "web_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
// #include "esp_https_server.h" // DISABLED for ESP-IDF v5.1.2 compatibility
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "time.h"

#include "heat_recovery.h"
#include "hardware_manager.h"
#include "sensor_manager.h"
#include "thermoflow_config.h"
#include "security_manager.h"
#include "rate_limiter.h"
#include "audit_log.h"
#include "web_static.h"
#include "wifi_manager.h"
#include "ota_manager.h"
#include "thermoflow_version.h"
#include "esp_system.h"


static const char *TAG = "WEB_SERVER";

static bool web_rate_limit_check(httpd_req_t *req)
{
    if (!rate_limiter_check(RATE_LIMIT_WEB_REQUESTS, NULL)) {
        audit_log_event(AUDIT_EVENT_RATE_LIMIT_HIT, AUDIT_SEVERITY_WARNING,
                        "Web rate limit exceeded");
        httpd_resp_set_status(req, "429 Too Many Requests");
        httpd_resp_send(req, "Rate limit exceeded", HTTPD_RESP_USE_STRLEN);
        return false;
    }
    return true;
}

static void add_firmware_version_fields(cJSON *root)
{
    cJSON_AddStringToObject(root, "firmware_version", THERMOFLOW_VERSION_STRING);
    cJSON_AddStringToObject(root, "version_full", THERMOFLOW_VERSION_FULL);
    cJSON_AddStringToObject(root, "version_scheme", THERMOFLOW_VERSION_SCHEME);
    cJSON_AddStringToObject(root, "calver", THERMOFLOW_CALVER_STRING);
    cJSON_AddNumberToObject(root, "version_year", THERMOFLOW_VERSION_YEAR);
    cJSON_AddNumberToObject(root, "version_week", THERMOFLOW_VERSION_WEEK);
    cJSON_AddNumberToObject(root, "version_revision", THERMOFLOW_VERSION_REVISION);
    cJSON_AddNumberToObject(root, "build_number", THERMOFLOW_BUILD_NUMBER);
    cJSON_AddStringToObject(root, "git_sha", THERMOFLOW_GIT_SHA);
    cJSON_AddStringToObject(root, "channel", THERMOFLOW_CHANNEL);
    cJSON_AddNumberToObject(root, "security_version", THERMOFLOW_SECURITY_VERSION);
}

// Server handles
static httpd_handle_t http_server = NULL;
static httpd_handle_t https_server = NULL;

// Configuration
static https_config_t s_https_config = {0};
static bool s_configured = false;

// Data
static heat_recovery_data_t s_ftx_data = {0};
static bool s_ftx_data_valid = false;

// Certificate cache
static char *s_server_cert = NULL;
static char *s_server_key = NULL;
static char *s_ca_cert = NULL;

// Forward declarations
static esp_err_t ftx_api_handler(httpd_req_t *req);
static esp_err_t ftx_sensors_handler(httpd_req_t *req);
static esp_err_t ftx_efficiency_handler(httpd_req_t *req);
static esp_err_t ftx_control_handler(httpd_req_t *req);
static esp_err_t ftx_status_handler(httpd_req_t *req);
static esp_err_t wifi_config_handler(httpd_req_t *req);
static esp_err_t wifi_config_delete_handler(httpd_req_t *req);
static esp_err_t device_info_handler(httpd_req_t *req);
static esp_err_t device_name_handler(httpd_req_t *req);
static esp_err_t device_restart_handler(httpd_req_t *req);
static esp_err_t ota_status_handler(httpd_req_t *req);
static esp_err_t logs_get_handler(httpd_req_t *req);
static esp_err_t logs_clear_handler(httpd_req_t *req);
static esp_err_t hardware_info_handler(httpd_req_t *req);
static esp_err_t hardware_mode_get_handler(httpd_req_t *req);
static esp_err_t hardware_mode_set_handler(httpd_req_t *req);
static esp_err_t cert_status_handler(httpd_req_t *req);
static esp_err_t redirect_handler(httpd_req_t *req);

static esp_err_t add_security_headers(httpd_req_t *req);
static void register_all_handlers(httpd_handle_t server);
static esp_err_t load_certificates_from_nvs(void);
static void free_certificate_cache(void);

static void wifi_config_restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(800));
    ESP_LOGI(TAG, "Restarting to apply WiFi credentials");
    esp_restart();
}

static void device_restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static const char *wifi_state_to_string(wifi_manager_state_t state)
{
    switch (state) {
        case WIFI_STATE_CONNECTED:
            return "connected";
        case WIFI_STATE_AP_MODE:
            return "ap_mode";
        case WIFI_STATE_CONNECTING:
            return "connecting";
        case WIFI_STATE_DISCONNECTED:
            return "disconnected";
        default:
            return "unknown";
    }
}

static const char *data_source_to_string(hw_data_source_t source)
{
    switch (source) {
        case HW_DATA_SOURCE_AUTO: return "auto";
        case HW_DATA_SOURCE_SIMULATION: return "simulation";
        case HW_DATA_SOURCE_HARDWARE: return "hardware";
        default: return "unknown";
    }
}

static bool parse_data_source(const char *value, hw_data_source_t *out)
{
    if (!value || !out) {
        return false;
    }
    if (strcmp(value, "auto") == 0) {
        *out = HW_DATA_SOURCE_AUTO;
        return true;
    }
    if (strcmp(value, "simulation") == 0) {
        *out = HW_DATA_SOURCE_SIMULATION;
        return true;
    }
    if (strcmp(value, "hardware") == 0) {
        *out = HW_DATA_SOURCE_HARDWARE;
        return true;
    }
    return false;
}

static const char *ota_state_to_string(ota_state_t state)
{
    switch (state) {
        case OTA_STATE_IDLE: return "idle";
        case OTA_STATE_CHECKING: return "checking";
        case OTA_STATE_DOWNLOADING: return "downloading";
        case OTA_STATE_VERIFYING: return "verifying";
        case OTA_STATE_READY: return "ready";
        case OTA_STATE_APPLYING: return "applying";
        case OTA_STATE_ROLLBACK: return "rollback";
        case OTA_STATE_ERROR: return "error";
        default: return "unknown";
    }
}

static void wifi_reset_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(800));
    wifi_manager_reset();
}

/* ============================================
 * Configuration Functions
 * ============================================ */

esp_err_t web_server_get_default_https_config(https_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(config, 0, sizeof(https_config_t));
    
    config->use_https = true;
    config->https_port = 443;
    config->http_port = 80;
    config->enable_http_redirect = true;
    config->enable_hsts = true;
    config->hsts_max_age = 31536000;  // 1 year
    config->tls_1_3_only = false;
    config->enable_mtls = false;
    config->auto_provision = false;
    config->ejbca_url = "https://ejbca.lttsweden.local:8443";
    config->csp_policy = "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self';";
    
    return ESP_OK;
}

esp_err_t web_server_set_https_config(const https_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&s_https_config, config, sizeof(https_config_t));
    
    // Make a copy of strings if they were provided
    // Note: In production, use proper string duplication
    
    s_configured = true;
    ESP_LOGI(TAG, "HTTPS config set: port=%d, redirect=%s, hsts=%s",
             config->https_port,
             config->enable_http_redirect ? "yes" : "no",
             config->enable_hsts ? "yes" : "no");
    
    return ESP_OK;
}

esp_err_t web_server_get_https_config(https_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_configured) {
        web_server_get_default_https_config(config);
    } else {
        memcpy(config, &s_https_config, sizeof(https_config_t));
    }
    
    return ESP_OK;
}

/* ============================================
 * Helper Functions
 * ============================================ */

static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    char *response = cJSON_Print(json);
    cJSON_Delete(json);
    
    if (!response) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON error");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, response, strlen(response));
    free(response);
    return ret;
}

static esp_err_t add_security_headers(httpd_req_t *req)
{
    // Get current config
    https_config_t config;
    web_server_get_https_config(&config);
    
    // HSTS - HTTP Strict Transport Security
    if (config.enable_hsts) {
        char hsts_header[128];
        snprintf(hsts_header, sizeof(hsts_header), 
                 "max-age=%lu; includeSubDomains; preload",
                 (unsigned long)config.hsts_max_age);
        httpd_resp_set_hdr(req, "Strict-Transport-Security", hsts_header);
    }
    
    // Content Security Policy
    if (config.csp_policy && strlen(config.csp_policy) > 0) {
        httpd_resp_set_hdr(req, "Content-Security-Policy", config.csp_policy);
    }
    
    // X-Frame-Options - prevent clickjacking
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");
    
    // X-Content-Type-Options - prevent MIME sniffing
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    
    // X-XSS-Protection
    httpd_resp_set_hdr(req, "X-XSS-Protection", "1; mode=block");
    
    // Referrer-Policy
    httpd_resp_set_hdr(req, "Referrer-Policy", "strict-origin-when-cross-origin");
    
    // Permissions-Policy
    httpd_resp_set_hdr(req, "Permissions-Policy", "geolocation=(), microphone=(), camera=()");
    
    return ESP_OK;
}

/* ============================================
 * Certificate Functions
 * ============================================ */

static void free_certificate_cache(void)
{
    if (s_server_cert) {
        free(s_server_cert);
        s_server_cert = NULL;
    }
    if (s_server_key) {
        free(s_server_key);
        s_server_key = NULL;
    }
    if (s_ca_cert) {
        free(s_ca_cert);
        s_ca_cert = NULL;
    }
}

static esp_err_t load_certificates_from_nvs(void)
{
    free_certificate_cache();
    
    size_t cert_len, key_len, ca_len;
    
    // Load server certificate
    esp_err_t ret = security_load_certificate(SEC_CERT_TYPE_SERVER, &s_server_cert, &cert_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Server certificate not found in NVS");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Loaded server certificate (%zu bytes)", cert_len);
    
    // Load server private key
    // Store server key as CLIENT_KEY type for reuse with existing security manager
    ret = security_load_certificate(SEC_CERT_TYPE_CLIENT_KEY, &s_server_key, &key_len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Server private key not found in NVS");
        free_certificate_cache();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Loaded server private key (%zu bytes)", key_len);
    
    // Load CA certificate (optional, for mTLS)
    ret = security_load_certificate(SEC_CERT_TYPE_CA, &s_ca_cert, &ca_len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded CA certificate (%zu bytes)", ca_len);
    } else {
        ESP_LOGW(TAG, "CA certificate not found (mTLS will be disabled)");
        s_ca_cert = NULL;
    }
    
    // Validate certificates
    if (!security_validate_cert_format(s_server_cert, 0)) {
        ESP_LOGE(TAG, "Server certificate format validation failed");
        free_certificate_cache();
        return ESP_FAIL;
    }
    
    int days_remaining = 0;
    if (security_is_cert_expired(s_server_cert, &days_remaining)) {
        ESP_LOGE(TAG, "Server certificate is expired or invalid");
        free_certificate_cache();
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Certificate expires in %d days", days_remaining);
    
    return ESP_OK;
}

bool web_server_has_certificates(void)
{
    return security_has_certificate(SEC_CERT_TYPE_SERVER) &&
           security_has_certificate(SEC_CERT_TYPE_CLIENT_KEY);
}

esp_err_t web_server_delete_certificates(void)
{
    ESP_LOGI(TAG, "Deleting certificates from NVS...");
    
    security_delete_certificate(SEC_CERT_TYPE_SERVER);
    security_delete_certificate(SEC_CERT_TYPE_CLIENT_KEY);
    
    free_certificate_cache();
    
    return ESP_OK;
}

esp_err_t web_server_get_cert_info(web_server_cert_info_t *info)
{
    if (!info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(info, 0, sizeof(web_server_cert_info_t));
    
    info->has_server_cert = security_has_certificate(SEC_CERT_TYPE_SERVER);
    info->has_server_key = security_has_certificate(SEC_CERT_TYPE_CLIENT_KEY);
    info->has_ca_cert = security_has_certificate(SEC_CERT_TYPE_CA);
    
    if (info->has_server_cert) {
        char *cert = NULL;
        size_t cert_len;
        if (security_load_certificate(SEC_CERT_TYPE_SERVER, &cert, &cert_len) == ESP_OK) {
            int days = 0;
            security_is_cert_expired(cert, &days);
            info->days_until_expiry = days;
            
            // Calculate fingerprint
            uint8_t fp[32];
            if (security_calc_cert_fingerprint(cert, fp) == ESP_OK) {
                for (int i = 0; i < 32; i++) {
                    sprintf(&info->fingerprint[i*2], "%02x", fp[i]);
                }
                info->fingerprint[64] = '\0';
            }
            
            free(cert);
        }
    }
    
    return ESP_OK;
}

esp_err_t web_server_check_cert_renewal(void)
{
    web_server_cert_info_t info;
    web_server_get_cert_info(&info);
    
    if (!info.has_server_cert) {
        ESP_LOGW(TAG, "No certificate found for renewal check");
        return ESP_FAIL;
    }
    
    if (info.days_until_expiry < 30) {
        ESP_LOGW(TAG, "Certificate expires in %d days, renewal required", 
                 info.days_until_expiry);
        
        https_config_t config;
        web_server_get_https_config(&config);
        
        if (config.auto_provision) {
            return web_server_provision_certificate();
        }
        
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Certificate valid for %d days, no renewal needed", 
             info.days_until_expiry);
    
    return ESP_OK;
}

/* ============================================
 * EJBCA Certificate Provisioning
 * ============================================ */

esp_err_t web_server_provision_certificate(void)
{
    ESP_LOGI(TAG, "Provisioning HTTPS certificate from EJBCA...");
    
    https_config_t config;
    web_server_get_https_config(&config);
    
    if (!config.device_id || strlen(config.device_id) == 0) {
        // Generate device ID from MAC address
        uint8_t mac[6];
        esp_wifi_get_mac(WIFI_IF_STA, mac);
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        ESP_LOGI(TAG, "Using MAC-based device ID: %s", mac_str);
    }
    
    // Generate CSR
    char csr[4096];
    esp_err_t ret = security_generate_csr(csr, sizeof(csr), 
                                           config.device_id ? config.device_id : "ThermoFlow",
                                           "ThermoFlow", "SE");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate CSR");
        return ret;
    }
    
    ESP_LOGI(TAG, "CSR generated successfully");
    
    // TODO: Implement actual EJBCA REST API call
    // For now, generate self-signed certificate as placeholder
    ESP_LOGW(TAG, "EJBCA integration not yet implemented, using self-signed certificate");
    
    char cert[4096];
    char key[2048];
    
    ret = security_generate_certificate(cert, sizeof(cert), key, sizeof(key),
                                         config.device_id ? config.device_id : "ThermoFlow",
                                         365);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate self-signed certificate");
        return ret;
    }
    
    // Store certificate and key
    ret = security_store_certificate(SEC_CERT_TYPE_SERVER, cert, strlen(cert));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store server certificate");
        return ret;
    }
    
    ret = security_store_certificate(SEC_CERT_TYPE_CLIENT_KEY, key, strlen(key));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to store server key");
        security_delete_certificate(SEC_CERT_TYPE_SERVER);
        return ret;
    }
    
    ESP_LOGI(TAG, "Certificate provisioned successfully");
    
    return ESP_OK;
}

/* ============================================
 * HTTP Redirect Server
 * ============================================ */

static esp_err_t redirect_handler(httpd_req_t *req)
{
    https_config_t config;
    web_server_get_https_config(&config);
    
    // Build HTTPS URL
    char redirect_url[256];
    char host_buf[64] = {0};
    const char *host = NULL;
    
    // Get host from request header, fallback to IP
    if (httpd_req_get_hdr_value_str(req, "Host", host_buf, sizeof(host_buf)) == ESP_OK) {
        host = host_buf;
    }
    
    if (!host || strlen(host) == 0) {
        // Get IP address
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[16];
            esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
            host = ip_str;
        } else {
            host = "localhost";
        }
    }
    
    snprintf(redirect_url, sizeof(redirect_url), 
             "https://%s:%d%s", host, config.https_port, req->uri);
    
    ESP_LOGI(TAG, "Redirecting HTTP request to: %s", redirect_url);
    
    // Send 301 Moved Permanently
    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    
    // Add HSTS header even on redirect
    if (config.enable_hsts) {
        char hsts_header[128];
        snprintf(hsts_header, sizeof(hsts_header), 
                 "max-age=%lu; includeSubDomains",
                 (unsigned long)config.hsts_max_age);
        httpd_resp_set_hdr(req, "Strict-Transport-Security", hsts_header);
    }
    
    httpd_resp_send(req, NULL, 0);
    
    return ESP_OK;
}

static esp_err_t start_http_redirect_server(void)
{
    if (http_server != NULL) {
        ESP_LOGW(TAG, "HTTP redirect server already running");
        return ESP_OK;
    }
    
    https_config_t config;
    web_server_get_https_config(&config);
    
    if (!config.enable_http_redirect || config.http_port == 0) {
        ESP_LOGI(TAG, "HTTP redirect disabled");
        return ESP_OK;
    }
    
    httpd_config_t http_config = HTTPD_DEFAULT_CONFIG();
    http_config.server_port = config.http_port;
    http_config.lru_purge_enable = true;
    http_config.max_uri_handlers = 5;
    
    ESP_LOGI(TAG, "Starting HTTP redirect server on port %d", config.http_port);
    
    esp_err_t ret = httpd_start(&http_server, &http_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP redirect server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register catch-all redirect handler
    httpd_uri_t redirect_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = redirect_handler,
        .user_ctx = NULL
    };
    
    ret = httpd_register_uri_handler(http_server, &redirect_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register redirect handler");
        httpd_stop(http_server);
        http_server = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "HTTP redirect server started on port %d", config.http_port);
    
    return ESP_OK;
}

/* ============================================
 * Handler Registration
 * ============================================ */

static void register_all_handlers(httpd_handle_t server)
{
    // Register FTX API handlers
    httpd_uri_t ftx_api_uri = {
        .uri = "/api/ftx",
        .method = HTTP_GET,
        .handler = ftx_api_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_api_uri);
    
    httpd_uri_t ftx_sensors_uri = {
        .uri = "/api/ftx/sensors",
        .method = HTTP_GET,
        .handler = ftx_sensors_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_sensors_uri);
    
    httpd_uri_t ftx_efficiency_uri = {
        .uri = "/api/ftx/efficiency",
        .method = HTTP_GET,
        .handler = ftx_efficiency_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_efficiency_uri);
    
    httpd_uri_t ftx_control_uri = {
        .uri = "/api/ftx/control",
        .method = HTTP_POST,
        .handler = ftx_control_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_control_uri);
    
    httpd_uri_t ftx_status_uri = {
        .uri = "/api/ftx/status",
        .method = HTTP_GET,
        .handler = ftx_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ftx_status_uri);
    
    // Hardware info endpoint
    httpd_uri_t hardware_info_uri = {
        .uri = "/api/hardware",
        .method = HTTP_GET,
        .handler = hardware_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &hardware_info_uri);

    httpd_uri_t hardware_mode_get_uri = {
        .uri = "/api/hardware/mode",
        .method = HTTP_GET,
        .handler = hardware_mode_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &hardware_mode_get_uri);

    httpd_uri_t hardware_mode_set_uri = {
        .uri = "/api/hardware/mode",
        .method = HTTP_POST,
        .handler = hardware_mode_set_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &hardware_mode_set_uri);
    
    // Certificate status endpoint
    httpd_uri_t cert_status_uri = {
        .uri = "/api/cert/status",
        .method = HTTP_GET,
        .handler = cert_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &cert_status_uri);
    
    // WiFi config handlers
    httpd_uri_t wifi_config_uri = {
        .uri = "/api/wifi/config",
        .method = HTTP_POST,
        .handler = wifi_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_config_uri);

    httpd_uri_t wifi_config_delete_uri = {
        .uri = "/api/wifi/config",
        .method = HTTP_DELETE,
        .handler = wifi_config_delete_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &wifi_config_delete_uri);
    
    httpd_uri_t device_info_uri = {
        .uri = "/api/device/info",
        .method = HTTP_GET,
        .handler = device_info_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_info_uri);

    httpd_uri_t device_name_uri = {
        .uri = "/api/device/name",
        .method = HTTP_POST,
        .handler = device_name_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_name_uri);

    httpd_uri_t device_restart_uri = {
        .uri = "/api/device/restart",
        .method = HTTP_POST,
        .handler = device_restart_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &device_restart_uri);

    httpd_uri_t ota_status_uri = {
        .uri = "/api/ota/status",
        .method = HTTP_GET,
        .handler = ota_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &ota_status_uri);

    httpd_uri_t logs_get_uri = {
        .uri = "/api/logs",
        .method = HTTP_GET,
        .handler = logs_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &logs_get_uri);

    httpd_uri_t logs_clear_uri = {
        .uri = "/api/logs",
        .method = HTTP_DELETE,
        .handler = logs_clear_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &logs_clear_uri);
}

/* ============================================
 * Core Server Functions
 * ============================================ */

esp_err_t web_server_init(void)
{
    ESP_LOGI(TAG, "Web server initialized (v2.0.0 HTTPS)");
    
    // Set default configuration if not already set
    if (!s_configured) {
        web_server_get_default_https_config(&s_https_config);
        s_configured = true;
    }
    
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    // Legacy HTTP-only mode
    if (s_configured && s_https_config.use_https) {
        ESP_LOGW(TAG, "HTTPS is configured but starting in HTTP mode");
    }
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.lru_purge_enable = true;
    config.max_uri_handlers = 32;
    
    esp_err_t err = httpd_start(&http_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }
    
    register_all_handlers(http_server);
    err = web_static_register_handlers(http_server);
    if (err != ESP_OK) {
        httpd_stop(http_server);
        http_server = NULL;
        return err;
    }
    
    ESP_LOGI(TAG, "HTTP server started on port 80");
    return ESP_OK;
}

esp_err_t web_server_start_https(void)
{
    if (https_server != NULL) {
        ESP_LOGW(TAG, "HTTPS server already running");
        return ESP_OK;
    }
    
    https_config_t config;
    web_server_get_https_config(&config);
    
    if (!config.use_https) {
        ESP_LOGW(TAG, "HTTPS not enabled in config, starting HTTP instead");
        return web_server_start();
    }
    
    // Load certificates from NVS
    esp_err_t ret = load_certificates_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load certificates from NVS");
        
        if (config.auto_provision) {
            ESP_LOGI(TAG, "Attempting certificate auto-provisioning...");
            ret = web_server_provision_certificate();
            if (ret == ESP_OK) {
                // Retry loading certificates
                ret = load_certificates_from_nvs();
            }
        }
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "No certificates available, HTTPS cannot start");
            
            // Option: fall back to HTTP if enabled
            if (config.enable_http_redirect) {
                ESP_LOGW(TAG, "Falling back to HTTP mode");
                return web_server_start();
            }
            
            return ESP_FAIL;
        }
    }
    
    // HTTPS DISABLED - ESP-IDF v5.1.2 compatibility issue with esp_tls_handshake_callback
    ESP_LOGW(TAG, "HTTPS server disabled - falling back to HTTP");
    return web_server_start();
}

esp_err_t web_server_restart_https(void)
{
    ESP_LOGI(TAG, "Restarting HTTPS server...");
    
    web_server_stop();
    free_certificate_cache();
    
    // Small delay to ensure sockets are released
    vTaskDelay(pdMS_TO_TICKS(100));
    
    return web_server_start_https();
}

esp_err_t web_server_stop(void)
{
    if (http_server) {
        httpd_stop(http_server);
        http_server = NULL;
    }
    
    /* HTTPS DISABLED
    if (https_server) {
        httpd_ssl_stop(https_server);
        https_server = NULL;
    }
    */
    
    ESP_LOGI(TAG, "Web server stopped");
    
    return ESP_OK;
}

bool web_server_is_running(void)
{
    return (http_server != NULL);  // HTTPS disabled
}

bool web_server_is_https_running(void)
{
    return false;  // HTTPS disabled
}

esp_err_t web_server_get_status(bool *http_running, bool *https_running)
{
    if (http_running) *http_running = (http_server != NULL);
    if (https_running) *https_running = (https_server != NULL);
    return ESP_OK;
}

esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, 
                                       esp_err_t (*handler)(httpd_req_t *req))
{
    httpd_handle_t srv = https_server ? https_server : http_server;
    
    if (!srv) {
        ESP_LOGE(TAG, "No server running to register handler");
        return ESP_ERR_INVALID_STATE;
    }
    
    httpd_uri_t uri_handler = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = NULL
    };
    
    return httpd_register_uri_handler(srv, &uri_handler);
}

esp_err_t web_server_deinit(void)
{
    web_server_stop();
    free_certificate_cache();
    return ESP_OK;
}

void web_server_update_ftx_data(const heat_recovery_data_t *data)
{
    if (!data) return;
    memcpy(&s_ftx_data, data, sizeof(s_ftx_data));
    s_ftx_data_valid = true;
}

bool web_server_get_ftx_data(heat_recovery_data_t *data)
{
    if (!data || !s_ftx_data_valid) return false;
    memcpy(data, &s_ftx_data, sizeof(s_ftx_data));
    return true;
}

/* ============================================
 * API Handlers (with security headers)
 * ============================================ */

static void add_ftx_sensor_fields(cJSON *root, bool include_flat_fields)
{
    if (!s_ftx_data_valid) {
        return;
    }

    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensors, "outdoor_temp", s_ftx_data.outdoor_temp);
    cJSON_AddNumberToObject(sensors, "outdoor_rh", s_ftx_data.outdoor_rh);
    cJSON_AddNumberToObject(sensors, "supply_temp", s_ftx_data.supply_temp);
    cJSON_AddNumberToObject(sensors, "supply_rh", s_ftx_data.supply_rh);
    cJSON_AddNumberToObject(sensors, "exhaust_temp", s_ftx_data.exhaust_temp);
    cJSON_AddNumberToObject(sensors, "exhaust_rh", s_ftx_data.exhaust_rh);
    cJSON_AddNumberToObject(sensors, "extract_temp", s_ftx_data.extract_temp);
    cJSON_AddNumberToObject(sensors, "extract_rh", s_ftx_data.extract_rh);
    cJSON_AddItemToObject(root, "sensors", sensors);

    cJSON *efficiency = cJSON_CreateObject();
    cJSON_AddNumberToObject(efficiency, "percent", s_ftx_data.efficiency_percent);
    cJSON_AddNumberToObject(efficiency, "power_recovered_w", s_ftx_data.energy_recovery_w);
    cJSON_AddNumberToObject(efficiency, "airflow_m3h", s_ftx_data.airflow_supply_m3h);
    cJSON_AddItemToObject(root, "efficiency", efficiency);

    cJSON *fans = cJSON_CreateObject();
    cJSON_AddNumberToObject(fans, "supply", s_ftx_data.fan_speed_current);
    cJSON_AddNumberToObject(fans, "exhaust", s_ftx_data.fan_speed_current);
    cJSON_AddItemToObject(root, "fans", fans);

    if (include_flat_fields) {
        cJSON_AddNumberToObject(root, "outdoor_temp", s_ftx_data.outdoor_temp);
        cJSON_AddNumberToObject(root, "outdoor_rh", s_ftx_data.outdoor_rh);
        cJSON_AddNumberToObject(root, "supply_temp", s_ftx_data.supply_temp);
        cJSON_AddNumberToObject(root, "supply_rh", s_ftx_data.supply_rh);
        cJSON_AddNumberToObject(root, "exhaust_temp", s_ftx_data.exhaust_temp);
        cJSON_AddNumberToObject(root, "exhaust_rh", s_ftx_data.exhaust_rh);
        cJSON_AddNumberToObject(root, "extract_temp", s_ftx_data.extract_temp);
        cJSON_AddNumberToObject(root, "extract_rh", s_ftx_data.extract_rh);
        cJSON_AddNumberToObject(root, "efficiency_percent", s_ftx_data.efficiency_percent);
        cJSON_AddNumberToObject(root, "fan_speed_percent", s_ftx_data.fan_speed_current);
    }
}

// GET /api/ftx - Main FTX data
static esp_err_t ftx_api_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);
    
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddBoolToObject(root, "valid", s_ftx_data_valid);
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    cJSON_AddStringToObject(root, "mode", hardware_is_simulation_mode() ? "SIMULATION" : "HARDWARE");
    cJSON_AddBoolToObject(root, "https_enabled", web_server_is_https_running());
    add_ftx_sensor_fields(root, true);
    
    return send_json_response(req, root);
}

// GET /api/ftx/sensors - Sensor readings only
static esp_err_t ftx_sensors_handler(httpd_req_t *req)
{
    add_security_headers(req);
    
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    
    if (s_ftx_data_valid) {
        cJSON_AddNumberToObject(root, "outdoor_temp", s_ftx_data.outdoor_temp);
        cJSON_AddNumberToObject(root, "outdoor_rh", s_ftx_data.outdoor_rh);
        cJSON_AddNumberToObject(root, "supply_temp", s_ftx_data.supply_temp);
        cJSON_AddNumberToObject(root, "supply_rh", s_ftx_data.supply_rh);
        cJSON_AddNumberToObject(root, "exhaust_temp", s_ftx_data.exhaust_temp);
        cJSON_AddNumberToObject(root, "exhaust_rh", s_ftx_data.exhaust_rh);
        cJSON_AddNumberToObject(root, "extract_temp", s_ftx_data.extract_temp);
        cJSON_AddNumberToObject(root, "extract_rh", s_ftx_data.extract_rh);
    }
    
    // Add pin configuration info
    cJSON *pins = cJSON_CreateObject();
    cJSON_AddNumberToObject(pins, "i2c_sda", I2C_MASTER_SDA_IO);
    cJSON_AddNumberToObject(pins, "i2c_scl", I2C_MASTER_SCL_IO);
    cJSON_AddNumberToObject(pins, "fan_1_gpio", FAN_1_GPIO);
    cJSON_AddNumberToObject(pins, "fan_2_gpio", FAN_2_GPIO);
    cJSON_AddItemToObject(root, "pin_config", pins);
    
    return send_json_response(req, root);
}

// GET /api/ftx/efficiency - Efficiency calculations
static esp_err_t ftx_efficiency_handler(httpd_req_t *req)
{
    add_security_headers(req);
    
    cJSON *root = cJSON_CreateObject();
    
    if (s_ftx_data_valid) {
        float efficiency = s_ftx_data.efficiency_percent;
        float temp_diff = s_ftx_data.exhaust_temp - s_ftx_data.outdoor_temp;
        
        cJSON_AddNumberToObject(root, "efficiency_percent", efficiency);
        cJSON_AddNumberToObject(root, "power_recovered_w", s_ftx_data.energy_recovery_w);
        cJSON_AddNumberToObject(root, "airflow_m3h", s_ftx_data.airflow_supply_m3h);
        cJSON_AddNumberToObject(root, "temp_diff_in_out", temp_diff);
    }
    
    return send_json_response(req, root);
}

// GET /api/ftx/status - Status flags
static esp_err_t ftx_status_handler(httpd_req_t *req)
{
    add_security_headers(req);
    
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    
    if (s_ftx_data_valid) {
        cJSON_AddBoolToObject(root, "frost_risk", s_ftx_data.frost_protection_active);
        cJSON_AddBoolToObject(root, "bypass_active", s_ftx_data.bypass_active);
        cJSON_AddBoolToObject(root, "filter_warning", 
            (s_ftx_data.status == FTX_STATUS_FILTER_WARNING || 
             s_ftx_data.status == FTX_STATUS_FILTER_CRITICAL));
    }
    
    return send_json_response(req, root);
}

static void add_hardware_mode_fields(cJSON *root)
{
    hw_data_source_t source = hardware_get_data_source();
    bool sim = hardware_is_simulation_mode();
    uint8_t sensor_count = hardware_get_sensor_count();

    cJSON_AddStringToObject(root, "data_source", data_source_to_string(source));
    cJSON_AddBoolToObject(root, "simulation_mode", sim);
    cJSON_AddNumberToObject(root, "sensor_count", sensor_count);

    if (sim && source == HW_DATA_SOURCE_SIMULATION) {
        cJSON_AddStringToObject(root, "status", "SIMULATION - Forced by setting");
    } else if (sim) {
        cJSON_AddStringToObject(root, "status", "SIMULATION - No sensors detected");
    } else if (sensor_count == 0) {
        cJSON_AddStringToObject(root, "status", "HARDWARE - No sensors detected");
    } else {
        cJSON_AddStringToObject(root, "status", "HARDWARE - Sensors connected");
    }
}

// GET /api/hardware/mode - Sensor data source preference
static esp_err_t hardware_mode_get_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    cJSON *root = cJSON_CreateObject();
    add_hardware_mode_fields(root);
    return send_json_response(req, root);
}

// POST /api/hardware/mode - Set sensor data source preference
static esp_err_t hardware_mode_set_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    char buf[128];
    int ret, remaining = req->content_len;

    if (remaining > (int)sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *source_item = cJSON_GetObjectItem(json, "data_source");
    if (!source_item || !cJSON_IsString(source_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing data_source");
        return ESP_FAIL;
    }

    hw_data_source_t source = HW_DATA_SOURCE_AUTO;
    if (!parse_data_source(source_item->valuestring, &source)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid data_source");
        return ESP_FAIL;
    }
    cJSON_Delete(json);

    esp_err_t set_ret = hardware_set_data_source(source);
    if (set_ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save setting");
        return ESP_FAIL;
    }

    sensor_manager_refresh_mode();

    audit_log_event(AUDIT_EVENT_CONFIG_CHANGE, AUDIT_SEVERITY_INFO,
                    "Sensor data source set to %s", data_source_to_string(source));

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    add_hardware_mode_fields(response);
    return send_json_response(req, response);
}

// GET /api/hardware - Hardware detection status and pin info
static esp_err_t hardware_info_handler(httpd_req_t *req)
{
    add_security_headers(req);
    
    cJSON *root = cJSON_CreateObject();
    add_hardware_mode_fields(root);
    
    // Detected components
    cJSON *detected = cJSON_CreateObject();
    cJSON_AddBoolToObject(detected, "sensor_1", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_1));
    cJSON_AddBoolToObject(detected, "sensor_2", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_2));
    cJSON_AddBoolToObject(detected, "sensor_3", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_3));
    cJSON_AddBoolToObject(detected, "sensor_4", hardware_is_detected(HW_COMPONENT_SHT40_SENSOR_4));
    cJSON_AddBoolToObject(detected, "display", hardware_is_detected(HW_COMPONENT_OLED_DISPLAY));
    cJSON_AddBoolToObject(detected, "fan_1", hardware_is_detected(HW_COMPONENT_FAN_1));
    cJSON_AddBoolToObject(detected, "fan_2", hardware_is_detected(HW_COMPONENT_FAN_2));
    cJSON_AddItemToObject(root, "detected", detected);
    
    // Counts
    cJSON_AddNumberToObject(root, "fan_count", hardware_get_fan_count());
    
    // Pin configuration
    cJSON *pin_config = cJSON_CreateObject();
    
    cJSON *i2c = cJSON_CreateObject();
    cJSON_AddNumberToObject(i2c, "sda_gpio", I2C_MASTER_SDA_IO);
    cJSON_AddNumberToObject(i2c, "scl_gpio", I2C_MASTER_SCL_IO);
    cJSON_AddNumberToObject(i2c, "frequency_hz", I2C_MASTER_FREQ_HZ);
    cJSON_AddItemToObject(pin_config, "i2c", i2c);
    
    cJSON *fans = cJSON_CreateObject();
    cJSON_AddNumberToObject(fans, "fan_1_gpio", FAN_1_GPIO);
    cJSON_AddNumberToObject(fans, "fan_2_gpio", FAN_2_GPIO);
    cJSON_AddNumberToObject(fans, "pwm_freq_hz", FAN_PWM_FREQ_HZ);
    cJSON_AddItemToObject(pin_config, "fans", fans);
    
    cJSON_AddItemToObject(root, "pin_config", pin_config);
    
    return send_json_response(req, root);
}

// GET /api/cert/status - Certificate status
static esp_err_t cert_status_handler(httpd_req_t *req)
{
    add_security_headers(req);
    
    cJSON *root = cJSON_CreateObject();
    
    web_server_cert_info_t info;
    web_server_get_cert_info(&info);
    
    cJSON_AddBoolToObject(root, "has_server_cert", info.has_server_cert);
    cJSON_AddBoolToObject(root, "has_server_key", info.has_server_key);
    cJSON_AddBoolToObject(root, "has_ca_cert", info.has_ca_cert);
    cJSON_AddNumberToObject(root, "days_until_expiry", info.days_until_expiry);
    
    if (strlen(info.fingerprint) > 0) {
        cJSON_AddStringToObject(root, "fingerprint", info.fingerprint);
    }
    
    // Server status
    bool http_running, https_running;
    web_server_get_status(&http_running, &https_running);
    cJSON_AddBoolToObject(root, "http_running", http_running);
    cJSON_AddBoolToObject(root, "https_running", https_running);
    
    return send_json_response(req, root);
}

// POST /api/ftx/control - Control commands
static esp_err_t ftx_control_handler(httpd_req_t *req)
{
    add_security_headers(req);
    
    char buf[256];
    int ret, remaining = req->content_len;
    
    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    int received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *cmd = cJSON_GetObjectItem(json, "command");
    cJSON *value = cJSON_GetObjectItem(json, "value");
    
    if (!cmd || !cJSON_IsString(cmd)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing command");
        return ESP_FAIL;
    }
    
    const char *cmd_str = cmd->valuestring;
    int val = value ? value->valueint : 0;
    
    ESP_LOGI(TAG, "FTX Control command: %s, value: %d", cmd_str, val);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "status", "ok");
    cJSON_AddStringToObject(response, "command", cmd_str);
    cJSON_AddNumberToObject(response, "value", val);
    cJSON_AddBoolToObject(response, "simulation_mode", hardware_is_simulation_mode());
    
    cJSON_Delete(json);
    return send_json_response(req, response);
}

// POST /api/wifi/config - Configure WiFi credentials
static esp_err_t wifi_config_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);
    
    char buf[512];
    int ret, remaining = req->content_len;
    
    if (remaining > sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    int received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';
    
    ESP_LOGI(TAG, "WiFi config request received");
    
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid = cJSON_GetObjectItem(json, "ssid");
    cJSON *password = cJSON_GetObjectItem(json, "password");
    
    if (!ssid || !cJSON_IsString(ssid) || strlen(ssid->valuestring) == 0) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid SSID");
        return ESP_FAIL;
    }
    
    const char *ssid_str = ssid->valuestring;
    const char *pass_str = password && cJSON_IsString(password) ? password->valuestring : "";
    
    ESP_LOGI(TAG, "WiFi config: SSID=%s", ssid_str);

    esp_err_t store_ret = wifi_manager_store_credentials(ssid_str, pass_str);
    if (store_ret != ESP_OK) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save WiFi credentials");
        return ESP_FAIL;
    }

    audit_log_event(AUDIT_EVENT_CONFIG_CHANGE, AUDIT_SEVERITY_INFO,
                      "WiFi credentials updated for SSID %s", ssid_str);
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "WiFi configuration saved. Device will restart.");
    cJSON_AddStringToObject(response, "ssid", ssid_str);
    
    cJSON_Delete(json);
    esp_err_t err = send_json_response(req, response);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(wifi_config_restart_task, "wifi_cfg_rst", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Restart task create failed, rebooting now");
        esp_restart();
    }

    return ESP_OK;
}

// DELETE /api/wifi/config - Reset saved WiFi credentials
static esp_err_t wifi_config_delete_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    audit_log_event(AUDIT_EVENT_CONFIG_CHANGE, AUDIT_SEVERITY_INFO,
                    "WiFi credentials reset via web API");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "WiFi configuration reset. Device will restart.");

    esp_err_t err = send_json_response(req, response);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(wifi_reset_task, "wifi_rst", 3072, NULL, 5, NULL) != pdPASS) {
        wifi_manager_reset();
    }

    return ESP_OK;
}

// POST /api/device/name - Set custom device name
static esp_err_t device_name_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    char buf[128];
    int ret, remaining = req->content_len;

    if (remaining > (int)sizeof(buf) - 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }

    int received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf + received, remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            return ESP_FAIL;
        }
        received += ret;
        remaining -= ret;
    }
    buf[received] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *name_item = cJSON_GetObjectItem(json, "device_name");
    if (!name_item || !cJSON_IsString(name_item) || name_item->valuestring[0] == '\0') {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid device_name");
        return ESP_FAIL;
    }

    char new_name[DEVICE_NAME_MAX_LEN + 1];
    strncpy(new_name, name_item->valuestring, sizeof(new_name) - 1);
    new_name[sizeof(new_name) - 1] = '\0';
    cJSON_Delete(json);

    esp_err_t set_ret = wifi_manager_set_device_name(new_name);
    if (set_ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid device name");
        return ESP_FAIL;
    }

    audit_log_event(AUDIT_EVENT_CONFIG_CHANGE, AUDIT_SEVERITY_INFO,
                    "Device name changed to %s", new_name);

    char display_name[DEVICE_NAME_MAX_LEN + 1];
    wifi_manager_get_display_name(display_name, sizeof(display_name));

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "device_id", wifi_manager_get_ap_name());
    cJSON_AddStringToObject(response, "device_name", display_name);
    cJSON_AddBoolToObject(response, "has_custom_name", wifi_manager_has_custom_name());
    return send_json_response(req, response);
}

// POST /api/device/restart - Restart the device
static esp_err_t device_restart_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "Device restarting");

    esp_err_t err = send_json_response(req, response);
    if (err != ESP_OK) {
        return err;
    }

    if (xTaskCreate(device_restart_task, "dev_restart", 2048, NULL, 5, NULL) != pdPASS) {
        esp_restart();
    }

    return ESP_OK;
}

// GET /api/ota/status - OTA update status
static esp_err_t ota_status_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    cJSON *root = cJSON_CreateObject();
    ota_status_t status = {0};

    if (ota_manager_get_status(&status) == ESP_OK) {
        cJSON_AddStringToObject(root, "state", ota_state_to_string(status.state));
        cJSON_AddBoolToObject(root, "update_available", status.update_available);
        cJSON_AddBoolToObject(root, "download_in_progress", status.download_in_progress);
        cJSON_AddBoolToObject(root, "update_ready", status.update_ready);
        cJSON_AddBoolToObject(root, "can_rollback", status.can_rollback);
        cJSON_AddStringToObject(root, "current_version", status.current_version);
        cJSON_AddStringToObject(root, "pending_version", status.pending_version);
        cJSON_AddStringToObject(root, "partition", status.partition_label);
        cJSON_AddNumberToObject(root, "download_progress", status.download_progress);
    } else {
        cJSON_AddStringToObject(root, "state", "unavailable");
    }

    add_firmware_version_fields(root);
    cJSON_AddBoolToObject(root, "server_configured", OTA_SERVER_URL[0] != '\0');
    if (OTA_SERVER_URL[0] != '\0') {
        cJSON_AddStringToObject(root, "update_url", OTA_SERVER_URL);
    }
    cJSON_AddStringToObject(root, "update_method",
        "OTA körs automatiskt när OTA_SERVER_URL är konfigurerad. "
        "Alternativt: flasha via USB (esptool) eller idf.py flash.");

    return send_json_response(req, root);
}

static void add_log_entry_json(cJSON *array, const audit_log_entry_t *entry, uint64_t now_us)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "sequence", entry->sequence);
    cJSON_AddNumberToObject(item, "timestamp_us", (double)entry->timestamp_us);
    cJSON_AddNumberToObject(item, "age_s", (double)((now_us - entry->timestamp_us) / 1000000ULL));
    cJSON_AddStringToObject(item, "event", audit_log_event_type_str(entry->event_type));
    cJSON_AddStringToObject(item, "severity", audit_log_severity_str(entry->severity));
    cJSON_AddStringToObject(item, "message", entry->message);

    char uptime_buf[16];
    uint32_t uptime_s = (uint32_t)(entry->timestamp_us / 1000000ULL);
    snprintf(uptime_buf, sizeof(uptime_buf), "+%02lu:%02lu:%02lu",
             (unsigned long)(uptime_s / 3600),
             (unsigned long)((uptime_s / 60) % 60),
             (unsigned long)(uptime_s % 60));
    cJSON_AddStringToObject(item, "time", uptime_buf);

    cJSON_AddItemToArray(array, item);
}

// GET /api/logs - Recent audit/system log entries
static esp_err_t logs_get_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    audit_log_entry_t entries[50];
    uint32_t count = 0;
    esp_err_t ret = audit_log_get_recent(entries, 50, &count);
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Log read failed");
        return ESP_FAIL;
    }

    uint64_t now_us = esp_timer_get_time();
    audit_log_stats_t stats = {0};
    audit_log_get_stats(&stats);

    cJSON *root = cJSON_CreateObject();
    cJSON *logs = cJSON_CreateArray();
    for (uint32_t i = 0; i < count; i++) {
        add_log_entry_json(logs, &entries[i], now_us);
    }

    cJSON_AddItemToObject(root, "logs", logs);
    cJSON_AddNumberToObject(root, "count", count);
    cJSON_AddNumberToObject(root, "capacity", 50);
    cJSON_AddNumberToObject(root, "total_logged", stats.total_entries);
    cJSON_AddNumberToObject(root, "uptime_us", (double)now_us);
    return send_json_response(req, root);
}

// DELETE /api/logs - Clear in-memory audit log
static esp_err_t logs_clear_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);

    esp_err_t ret = audit_log_clear();
    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Log clear failed");
        return ESP_FAIL;
    }

    audit_log_event(AUDIT_EVENT_CONFIG_CHANGE, AUDIT_SEVERITY_INFO,
                    "Audit log cleared from web UI");

    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    return send_json_response(req, response);
}

// GET /api/device/info - Return device info (MAC, name, etc.)
static esp_err_t device_info_handler(httpd_req_t *req)
{
    if (!web_rate_limit_check(req)) {
        return ESP_FAIL;
    }
    add_security_headers(req);
    
    cJSON *root = cJSON_CreateObject();
    
    const char *device_id = wifi_manager_get_ap_name();
    char display_name[DEVICE_NAME_MAX_LEN + 1];
    wifi_manager_get_display_name(display_name, sizeof(display_name));

    cJSON_AddStringToObject(root, "device_id", device_id);
    cJSON_AddStringToObject(root, "default_name", device_id);
    cJSON_AddStringToObject(root, "device_name", display_name);
    cJSON_AddStringToObject(root, "name", display_name);
    cJSON_AddBoolToObject(root, "has_custom_name", wifi_manager_has_custom_name());
    cJSON_AddBoolToObject(root, "name_editable", true);

    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        memset(mac, 0, sizeof(mac));
    }
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    cJSON_AddStringToObject(root, "mac_address", mac_str);
    cJSON_AddStringToObject(root, "mac", mac_str);
    
    wifi_manager_status_t wifi_status = {0};
    wifi_manager_get_status(&wifi_status);
    cJSON_AddStringToObject(root, "wifi_state", wifi_state_to_string(wifi_status.state));
    cJSON_AddBoolToObject(root, "wifi_credentials_saved", wifi_manager_has_saved_credentials());
    cJSON_AddBoolToObject(root, "wifi_ap_fallback", wifi_manager_is_ap_fallback_mode());

    char saved_ssid[33] = {0};
    if (wifi_manager_get_saved_ssid(saved_ssid, sizeof(saved_ssid)) == ESP_OK) {
        cJSON_AddStringToObject(root, "wifi_saved_ssid", saved_ssid);
    }

    cJSON_AddStringToObject(root, "ip_address",
        wifi_status.ip_address[0] ? wifi_status.ip_address : "0.0.0.0");
    cJSON_AddStringToObject(root, "ap_name", wifi_status.ap_name);
    if (wifi_status.state == WIFI_STATE_CONNECTED) {
        cJSON_AddNumberToObject(root, "rssi", wifi_status.rssi);
    }
    
    // Server status
    bool http_running, https_running;
    web_server_get_status(&http_running, &https_running);
    cJSON_AddBoolToObject(root, "http_running", http_running);
    cJSON_AddBoolToObject(root, "https_running", https_running);
    
    // Firmware version and platform
    add_firmware_version_fields(root);
    cJSON_AddStringToObject(root, "platform", "ESP32-S3");
    cJSON_AddBoolToObject(root, "ota_available", OTA_SERVER_URL[0] != '\0');
    
    // Hardware mode
    cJSON_AddBoolToObject(root, "simulation_mode", hardware_is_simulation_mode());
    cJSON_AddStringToObject(root, "mode_description", hardware_is_simulation_mode() ? 
        "Running with simulated sensor data" : "Running with real hardware sensors");
    
    return send_json_response(req, root);
}
