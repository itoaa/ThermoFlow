/**
 * @file security_manager.h
 * @brief Security Manager Interface
 * 
 * Handles certificates, authentication, and secure operations
 * Implements IEC 62443 SR-002: Authentication & Authorization
 * 
 * @version 1.0.0
 * @date 2026-03-22
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
#define SEC_MAX_CERT_LEN        2048

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

/**
 * @brief Generate self-signed certificate
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
 * @brief Verify certificate chain
 * 
 * @param cert Certificate to verify
 * @param ca_cert CA certificate
 * @return true if valid
 */
bool security_verify_certificate(const uint8_t *cert, const uint8_t *ca_cert);

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

/**
 * @brief Get security status
 * 
 * @return true if security subsystem is healthy
 */
bool security_is_healthy(void);

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