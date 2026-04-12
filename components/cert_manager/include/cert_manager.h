/**
 * @file cert_manager.h
 * @brief Certificate Manager Interface for ThermoFlow HTTPS Server
 * 
 * Implements SEC-026: ThermoFlow HTTPS Web Server Implementation
 * - Certificate lifecycle management
 * - Integration with EJBCA-PKI
 * - Auto-renewal support
 * 
 * @version 1.0.0
 * @date 2026-04-12
 * @security SEC-026
 */

#ifndef CERT_MANAGER_H
#define CERT_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Configuration
 * ============================================ */

#define CERT_MGR_MAX_CERT_LEN       4096
#define CERT_MGR_MAX_KEY_LEN        2048
#define CERT_MGR_MAX_CSR_LEN        4096
#define CERT_MGR_DEVICE_ID_LEN      64
#define CERT_MGR_URL_LEN            256

/* ============================================
 * Types
 * ============================================ */

/**
 * @brief Certificate manager configuration
 */
typedef struct {
    char ejbca_url[CERT_MGR_URL_LEN];           /*!< EJBCA server URL */
    char device_id[CERT_MGR_DEVICE_ID_LEN];      /*!< Device ID (CN for certificate) */
    char org[64];                               /*!< Organization (O) */
    char country[3];                            /*!< Country code (C) */
    int validity_days;                          /*!< Certificate validity period */
    int renewal_days_before;                    /*!< Days before expiry to renew */
    int key_size;                               /*!< RSA key size (2048 or 4096) */
    int signature_alg;                          /*!< Signature algorithm (MBEDTLS_MD_*) */
    bool enable_auto_renewal;                   /*!< Enable automatic renewal */
} cert_manager_config_t;

/**
 * @brief Certificate data
 */
typedef struct {
    char cert_pem[CERT_MGR_MAX_CERT_LEN];       /*!< Certificate in PEM format */
    char key_pem[CERT_MGR_MAX_KEY_LEN];         /*!< Private key in PEM format */
    size_t cert_len;                            /*!< Certificate length */
    size_t key_len;                             /*!< Key length */
    time_t expiry;                              /*!< Expiry timestamp */
    int days_remaining;                         /*!< Days until expiry */
} cert_manager_cert_t;

/**
 * @brief Certificate status
 */
typedef struct {
    bool has_certificate;                       /*!< Certificate exists */
    bool expired;                               /*!< Certificate is expired */
    bool needs_renewal;                         /*!< Renewal needed (within threshold) */
    int days_remaining;                         /*!< Days until expiry */
    char fingerprint[65];                       /*!< Certificate fingerprint (SHA-256 hex) */
} cert_manager_status_t;

/* ============================================
 * Initialization
 * ============================================ */

/**
 * @brief Initialize certificate manager
 * 
 * @param config Configuration (NULL for defaults)
 * @return ESP_OK on success
 */
esp_err_t cert_manager_init(const cert_manager_config_t *config);

/**
 * @brief Set certificate manager configuration
 * 
 * @param config New configuration
 * @return ESP_OK on success
 */
esp_err_t cert_manager_set_config(const cert_manager_config_t *config);

/**
 * @brief Get current configuration
 * 
 * @param[out] config Configuration buffer
 * @return ESP_OK on success
 */
esp_err_t cert_manager_get_config(cert_manager_config_t *config);

/* ============================================
 * Certificate Operations
 * ============================================ */

/**
 * @brief Provision new certificate
 * 
 * Generates a new key pair and certificate. If use_ejbca is true,
 * attempts to enroll with EJBCA-PKI. Otherwise generates self-signed.
 * 
 * @param use_ejbca Use EJBCA for certificate issuance
 * @return ESP_OK on success
 */
esp_err_t cert_manager_provision(bool use_ejbca);

/**
 * @brief Load certificate from storage
 * 
 * @param[out] cert Certificate buffer
 * @return ESP_OK on success
 */
esp_err_t cert_manager_load(cert_manager_cert_t *cert);

/**
 * @brief Store certificate and key
 * 
 * @param cert_pem Certificate in PEM format
 * @param key_pem Private key in PEM format
 * @return ESP_OK on success
 */
esp_err_t cert_manager_store(const char *cert_pem, const char *key_pem);

/**
 * @brief Delete stored certificate
 * 
 * @return ESP_OK on success
 */
esp_err_t cert_manager_delete(void);

/**
 * @brief Check if certificate exists
 * 
 * @return true if certificate is stored
 */
bool cert_manager_has_certificate(void);

/**
 * @brief Get certificate status
 * 
 * @param[out] status Status buffer
 * @return ESP_OK on success
 */
esp_err_t cert_manager_get_status(cert_manager_status_t *status);

/* ============================================
 * Renewal Operations
 * ============================================ */

/**
 * @brief Check and renew certificate if needed
 * 
 * Checks expiry and renews if within threshold.
 * Should be called periodically (e.g., daily).
 * 
 * @return ESP_OK if no renewal needed or renewal succeeded
 */
esp_err_t cert_manager_check_and_renew(void);

/**
 * @brief Force certificate renewal
 * 
 * @return ESP_OK on success
 */
esp_err_t cert_manager_renew(void);

/**
 * @brief Generate CSR for EJBCA enrollment
 * 
 * @param[out] csr Buffer for CSR (PEM)
 * @param csr_len Buffer size
 * @return ESP_OK on success
 */
esp_err_t cert_manager_generate_csr(char *csr, size_t csr_len);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Check if certificate is expired
 * 
 * @return true if expired or no certificate
 */
bool cert_manager_is_expired(void);

/**
 * @brief Get days remaining until expiry
 * 
 * @return Days remaining, or -1 if error
 */
int cert_manager_days_remaining(void);

#ifdef __cplusplus
}
#endif

#endif /* CERT_MANAGER_H */
