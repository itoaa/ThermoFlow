/**
 * @file web_server.h
 * @brief HTTPS Web Server Interface with FTX API - ESP-IDF
 * 
 * Implements SEC-018: HTTPS Web Server Security Enhancement
 * - TLS 1.3 with fallback to TLS 1.2
 * - Certificate management integration with EJBCA-PKI
 * - HTTP-to-HTTPS redirect
 * - Security headers (HSTS, CSP, X-Frame-Options)
 * 
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-018
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "heat_recovery.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * HTTPS Configuration
 * ============================================ */

/**
 * @brief HTTPS configuration structure
 */
typedef struct {
    bool use_https;                    /*!< Enable HTTPS mode */
    uint16_t https_port;               /*!< HTTPS server port (default 443) */
    uint16_t http_port;                /*!< HTTP redirect port (default 80, 0 to disable) */
    
    bool enable_http_redirect;         /*!< Redirect HTTP to HTTPS */
    bool enable_hsts;                  /*!< Enable HTTP Strict Transport Security */
    uint32_t hsts_max_age;             /*!< HSTS max-age in seconds (default 31536000) */
    
    bool tls_1_3_only;                 /*!< Require TLS 1.3 only (no fallback) */
    bool enable_mtls;                  /*!< Enable mutual TLS (client certificate auth) */
    
    bool auto_provision;               /*!< Auto-provision certificates from EJBCA */
    const char* ejbca_url;             /*!< EJBCA server URL */
    const char* device_id;             /*!< Device ID for certificate CN */
    
    const char* csp_policy;            /*!< Content Security Policy string */
} https_config_t;

/**
 * @brief Web server configuration structure
 */
typedef struct {
    uint16_t port;                     /*!< Deprecated: use https_config */
    bool enable_https;                 /*!< Deprecated: use https_config.use_https */
    https_config_t https_config;       /*!< HTTPS configuration */
} web_server_config_t;

/**
 * @brief Web server certificate information
 */
typedef struct {
    bool has_server_cert;              /*!< Server certificate exists */
    bool has_server_key;             /*!< Server private key exists */
    bool has_ca_cert;                /*!< CA certificate exists (for mTLS) */
    int days_until_expiry;             /*!< Days until certificate expiry (-1 if unknown) */
    char fingerprint[65];              /*!< Certificate fingerprint (hex) */
} web_server_cert_info_t;

/* ============================================
 * Core Functions
 * ============================================ */

/**
 * @brief Initialize web server subsystem
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_init(void);

/**
 * @brief Start web server (HTTP mode)
 * 
 * @note Use web_server_start_https() for HTTPS mode
 * @return ESP_OK on success
 */
esp_err_t web_server_start(void);

/**
 * @brief Start HTTPS web server
 * 
 * Loads certificates from NVS and starts HTTPS server on configured port.
 * If certificates don't exist and auto_provision is enabled, will
 * attempt to provision from EJBCA-PKI.
 * 
 * @return ESP_OK on success
 * @return ESP_FAIL if certificates unavailable and provisioning disabled/failed
 */
esp_err_t web_server_start_https(void);

/**
 * @brief Stop web server (both HTTP and HTTPS)
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_stop(void);

/**
 * @brief Check if web server is running
 * 
 * @return true if HTTP or HTTPS server is active
 */
bool web_server_is_running(void);

/**
 * @brief Check if HTTPS server is running
 * 
 * @return true if HTTPS server is active
 */
bool web_server_is_https_running(void);

/**
 * @brief Register custom URI handler
 * 
 * @param uri URI path
 * @param method HTTP method
 * @param handler Handler function
 * @return ESP_OK on success
 */
esp_err_t web_server_register_handler(const char *uri, httpd_method_t method, 
                                       esp_err_t (*handler)(httpd_req_t *req));

/**
 * @brief Deinitialize web server
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_deinit(void);

/* ============================================
 * HTTPS Configuration Functions
 * ============================================ */

/**
 * @brief Set HTTPS configuration
 * 
 * Must be called before web_server_start_https().
 * 
 * @param config HTTPS configuration
 * @return ESP_OK on success
 */
esp_err_t web_server_set_https_config(const https_config_t *config);

/**
 * @brief Get current HTTPS configuration
 * 
 * @param[out] config HTTPS configuration buffer
 * @return ESP_OK on success
 */
esp_err_t web_server_get_https_config(https_config_t *config);

/**
 * @brief Get default HTTPS configuration
 * 
 * @param[out] config HTTPS configuration buffer
 * @return ESP_OK on success
 */
esp_err_t web_server_get_default_https_config(https_config_t *config);

/* ============================================
 * Certificate Management Functions
 * ============================================ */

/**
 * @brief Load certificates from NVS and start HTTPS
 * 
 * Internal use - called by web_server_start_https()
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_load_and_start_https(void);

/**
 * @brief Provision certificate from EJBCA-PKI
 * 
 * Generates CSR, submits to EJBCA, stores returned certificate.
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_provision_certificate(void);

/**
 * @brief Get certificate information
 * 
 * @param[out] info Certificate info structure
 * @return ESP_OK on success
 */
esp_err_t web_server_get_cert_info(web_server_cert_info_t *info);

/**
 * @brief Check if certificates are available
 * 
 * @return true if server certificate and key exist
 */
bool web_server_has_certificates(void);

/**
 * @brief Delete stored certificates from NVS
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_delete_certificates(void);

/**
 * @brief Restart web server to apply new certificates
 * 
 * Stops and restarts HTTPS server with current certificates.
 * 
 * @return ESP_OK on success
 */
esp_err_t web_server_restart_https(void);

/* ============================================
 * FTX-specific Functions
 * ============================================ */

/**
 * @brief Update FTX data for API responses
 * 
 * @param data Heat recovery data
 */
void web_server_update_ftx_data(const heat_recovery_data_t *data);

/**
 * @brief Get FTX data (for internal use)
 * 
 * @param[out] data Heat recovery data buffer
 * @return true if data is valid
 */
bool web_server_get_ftx_data(heat_recovery_data_t *data);

/* ============================================
 * Status and Health
 * ============================================ */

/**
 * @brief Get web server status
 * 
 * @param[out] http_running HTTP server status
 * @param[out] https_running HTTPS server status
 * @return ESP_OK on success
 */
esp_err_t web_server_get_status(bool *http_running, bool *https_running);

/**
 * @brief Get HTTPS cipher suite information
 * 
 * @param[out] cipher_name Buffer for cipher name
 * @param name_len Buffer length
 * @return ESP_OK on success
 */
esp_err_t web_server_get_cipher_info(char *cipher_name, size_t name_len);

/**
 * @brief Trigger certificate renewal if needed
 * 
 * Checks expiry and renews if within threshold.
 * 
 * @return ESP_OK if no renewal needed or renewal succeeded
 * @return ESP_FAIL if renewal required but failed
 */
esp_err_t web_server_check_cert_renewal(void);

#ifdef __cplusplus
}
#endif

#endif /* WEB_SERVER_H */
