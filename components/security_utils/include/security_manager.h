/**
 * @file security_manager.h
 * @brief Security Manager Interface
 * 
 * Handles certificates, authentication, and secure operations
 * Implements IEC 62443 SR-002: Authentication & Authorization
 * 
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-016: MQTT-TLS Certificate Management
 */

#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "ed25519_impl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEC_MAX_USERNAME_LEN    32
#define SEC_MAX_PASSWORD_LEN    64
#define SEC_HASH_LEN            32
#define SEC_MAX_CERT_LEN        4096
#define SEC_MAX_KEY_LEN         2048

/* ============================================
 * Core Security Functions
 * ============================================ */

/**
 * @brief Security manager initialization
 * 
 * @return ESP_OK on success
 */
esp_err_t security_manager_init(void);

/**
 * @brief Generate secure random bytes
 * 
 * @param[out] buffer Output buffer
 * @param len Number of bytes to generate
 * @return ESP_OK on success
 */
esp_err_t security_random_bytes(uint8_t *buffer, size_t len);

/**
 * @brief Hash password using PBKDF2
 * 
 * @param password Plain text password
 * @param salt Salt bytes
 * @param salt_len Salt length
 * @param[out] hash Output hash buffer (32 bytes)
 * @return ESP_OK on success
 */
esp_err_t security_hash_password(const char *password, const uint8_t *salt, 
                                   size_t salt_len, uint8_t *hash);

/**
 * @brief Verify password against stored hash
 * 
 * @param password Plain text password
 * @param stored_hash Stored hash
 * @param salt Stored salt
 * @param salt_len Salt length
 * @return true if password matches
 */
bool security_verify_password(const char *password, const uint8_t *stored_hash,
                              const uint8_t *salt, size_t salt_len);

/**
 * @brief Store user credentials securely
 * 
 * @param username Username
 * @param password Password
 * @return ESP_OK on success
 */
esp_err_t security_store_credentials(const char *username, const char *password);

/**
 * @brief Load user credentials
 * 
 * @param[out] username Buffer for username
 * @param username_len Buffer length
 * @param[out] hash Buffer for password hash
 * @param[out] salt Buffer for salt
 * @param[out] salt_len Salt length
 * @return ESP_OK on success
 */
esp_err_t security_load_credentials(char *username, size_t username_len,
                                    uint8_t *hash, uint8_t *salt, size_t *salt_len);

/**
 * @brief Verify user credentials
 * 
 * @param username Username
 * @param password Password
 * @return true if valid
 */
bool security_validate_credentials(const char *username, const char *password);

/**
 * @brief Check if credentials are configured
 * 
 * @return true if credentials exist
 */
bool security_has_credentials(void);

/* ============================================
 * Certificate Management (SEC-016)
 * ============================================ */

/**
 * @brief Certificate types for MQTT-TLS
 */
typedef enum {
    SEC_CERT_TYPE_CA = 0,           /*!< CA certificate */
    SEC_CERT_TYPE_CLIENT,           /*!< Client certificate (mTLS) */
    SEC_CERT_TYPE_CLIENT_KEY,       /*!< Client private key */
    SEC_CERT_TYPE_SERVER            /*!< Server certificate (for web server) */
} sec_cert_type_t;

/**
 * @brief Store certificate in secure NVS storage
 * 
 * @param type Certificate type
 * @param cert Certificate data (PEM)
 * @param cert_len Certificate length
 * @return ESP_OK on success
 */
esp_err_t security_store_certificate(sec_cert_type_t type, const char *cert, size_t cert_len);

/**
 * @brief Load certificate from NVS
 * 
 * @param type Certificate type
 * @param[out] cert Buffer for certificate (must be freed by caller)
 * @param[out] cert_len Certificate length
 * @return ESP_OK on success
 */
esp_err_t security_load_certificate(sec_cert_type_t type, char **cert, size_t *cert_len);

/**
 * @brief Delete certificate from NVS
 * 
 * @param type Certificate type
 * @return ESP_OK on success
 */
esp_err_t security_delete_certificate(sec_cert_type_t type);

/**
 * @brief Check if certificate exists
 * 
 * @param type Certificate type
 * @return true if certificate exists
 */
bool security_has_certificate(sec_cert_type_t type);

/**
 * @brief Calculate certificate fingerprint (SHA-256)
 * 
 * @param cert Certificate in PEM format
 * @param[out] fingerprint Output buffer (32 bytes)
 * @return ESP_OK on success
 */
esp_err_t security_calc_cert_fingerprint(const char *cert, uint8_t *fingerprint);

/**
 * @brief Validate certificate format
 * 
 * @param cert Certificate in PEM format
 * @param expected_type Expected certificate type (0 for any)
 * @return true if valid
 */
bool security_validate_cert_format(const char *cert, int expected_type);

/**
 * @brief Check if certificate is expired
 * 
 * @param cert Certificate in PEM format
 * @param[out] days_remaining Days until expiry (can be NULL)
 * @return true if expired or invalid
 */
bool security_is_cert_expired(const char *cert, int *days_remaining);

/**
 * @brief Generate certificate signing request (CSR)
 * 
 * @param[out] csr Buffer for CSR (PEM)
 * @param csr_len CSR buffer size
 * @param cn Common name
 * @param org Organization (can be NULL)
 * @param country Country code (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t security_generate_csr(char *csr, size_t csr_len, 
                                const char *cn, const char *org, const char *country);

/**
 * @brief Store certificate pinning hash
 * 
 * @param hash SHA-256 hash (32 bytes)
 * @return ESP_OK on success
 */
esp_err_t security_store_pinning_hash(const uint8_t *hash);

/**
 * @brief Load certificate pinning hash
 * 
 * @param[out] hash Buffer (32 bytes)
 * @return ESP_OK on success
 */
esp_err_t security_load_pinning_hash(uint8_t *hash);

/* ============================================
 * Legacy Certificate Functions
 * ============================================ */

/**
 * @brief Generate self-signed certificate (legacy)
 * 
 * @param[out] cert Buffer for certificate (PEM)
 * @param cert_len Certificate buffer size
 * @param[out] key Buffer for private key (PEM)
 * @param key_len Key buffer size
 * @param cn Common name for certificate
 * @param days_valid Validity period in days
 * @return ESP_OK on success
 */
esp_err_t security_generate_certificate(char *cert, size_t cert_len,
                                        char *key, size_t key_len,
                                        const char *cn, int days_valid);

/**
 * @brief Verify certificate chain (legacy)
 * 
 * @param cert Certificate to verify
 * @param ca_cert CA certificate
 * @return true if valid
 */
bool security_verify_certificate(const uint8_t *cert, const uint8_t *ca_cert);

/* ============================================
 * Cryptographic Functions
 * ============================================ */

/**
 * @brief Sign data with Ed25519
 * 
 * @param data Data to sign
 * @param data_len Data length
 * @param private_key Private key (32 bytes)
 * @param[out] signature Output signature (64 bytes)
 * @return ESP_OK on success
 */
esp_err_t security_sign_data(const uint8_t *data, size_t data_len,
                              const uint8_t *private_key, uint8_t *signature);

/**
 * @brief Verify Ed25519 signature
 * 
 * @param data Data that was signed
 * @param data_len Data length
 * @param signature Signature (64 bytes)
 * @param public_key Public key (32 bytes)
 * @return true if signature is valid
 */
bool security_verify_signature(const uint8_t *data, size_t data_len,
                               const uint8_t *signature, const uint8_t *public_key);

/**
 * @brief Store OTA signing keys
 * 
 * @param public_key Ed25519 public key (32 bytes)
 * @param private_key Ed25519 private key (32 bytes, optional)
 * @return ESP_OK on success
 */
esp_err_t security_store_ota_keys(const uint8_t *public_key, const uint8_t *private_key);

/**
 * @brief Load OTA signing keys
 * 
 * @param[out] public_key Ed25519 public key buffer (32 bytes)
 * @param[out] private_key Ed25519 private key buffer (32 bytes, optional)
 * @param[out] has_private Set to true if private key exists
 * @return ESP_OK on success
 */
esp_err_t security_load_ota_keys(uint8_t *public_key, uint8_t *private_key, bool *has_private);

/* ============================================
 * Status Functions
 * ============================================ */

/**
 * @brief Get security status
 * 
 * @return true if security subsystem is healthy
 */
bool security_is_healthy(void);

/**
 * @brief Get detailed security status
 * 
 * @param[out] certs_configured Number of configured certificates
 * @param[out] tls_ready true if TLS is ready
 * @return ESP_OK on success
 */
esp_err_t security_get_status(uint8_t *certs_configured, bool *tls_ready);

/**
 * @brief Deinitialize security manager
 * 
 * @return ESP_OK on success
 */
esp_err_t security_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* SECURITY_MANAGER_H */
