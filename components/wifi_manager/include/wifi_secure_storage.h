/**
 * @file wifi_secure_storage.h
 * @brief Secure encrypted NVS storage for WiFi credentials
 * 
 * Implements AES-256 encryption for WiFi credentials using ESP-IDF NVS encryption.
 * Derives encryption key from device-specific unique key (eFuse/eFuse_key).
 * 
 * Security: SEC-021 - WiFi Credential Encryption
 * Framework: CISSP Domain 3 (Security Architecture), NIST PR.DS-02
 * 
 * @version 2.0.0
 * @date 2026-04-12
 * @security SEC-021
 */

#ifndef WIFI_SECURE_STORAGE_H
#define WIFI_SECURE_STORAGE_H

#include "esp_err.h"
#include "wifi_manager.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Configuration
 * ============================================ */

#define WIFI_ENC_NVS_NAMESPACE    "wifi_sec_cfg"   /*!< Encrypted NVS namespace */
#define WIFI_ENC_KEY_NAMESPACE    "wifi_key"       /*!< Key storage namespace */
#define WIFI_ENC_KEY_SSID         "ssid_enc"       /*!< Encrypted SSID key */
#define WIFI_ENC_KEY_PASSWORD     "pass_enc"       /*!< Encrypted password key */
#define WIFI_ENC_KEY_SALT         "salt"           /*!< Salt for key derivation */
#define WIFI_ENC_KEY_VERSION      "enc_ver"        /*!< Encryption version marker */

#define WIFI_ENC_VERSION          1                /*!< Current encryption version */
#define WIFI_SALT_LEN             16               /*!< Salt length in bytes */
#define WIFI_ENC_MAX_SSID_LEN     33               /*!< Max encrypted SSID storage */
#define WIFI_ENC_MAX_PASS_LEN     65               /*!< Max encrypted password storage */
#define WIFI_DERIVED_KEY_LEN      32               /*!< AES-256 key length */

/* ============================================
 * Encryption Context
 * ============================================ */

/**
 * @brief Encrypted WiFi credential storage structure
 */
typedef struct {
    uint8_t salt[WIFI_SALT_LEN];              /*!< Salt for key derivation */
    uint8_t ssid_enc[WIFI_ENC_MAX_SSID_LEN];  /*!< Encrypted SSID */
    uint8_t pass_enc[WIFI_ENC_MAX_PASS_LEN];  /*!< Encrypted password */
    size_t ssid_len;                          /*!< Actual SSID length */
    size_t pass_len;                          /*!< Actual password length */
    uint8_t version;                          /*!< Encryption version */
    uint8_t checksum[32];                       /*!< HMAC-SHA256 for integrity */
} wifi_encrypted_data_t;

/* ============================================
 * Core Functions
 * ============================================ */

/**
 * @brief Initialize secure WiFi storage
 * 
 * Initializes encrypted NVS partition and checks for existing encrypted credentials.
 * Must be called before any other secure storage functions.
 * 
 * @param[in] key_source Source for encryption key (EFUSE recommended)
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if encryption unavailable
 */
esp_err_t wifi_secure_storage_init(wifi_key_source_t key_source);

/**
 * @brief Store WiFi credentials securely with encryption
 * 
 * Encrypts and stores WiFi credentials in NVS using AES-256.
 * Uses device-unique key derivation to ensure credentials
 * cannot be decrypted on another device.
 * 
 * @param[in] ssid WiFi SSID (plaintext)
 * @param[in] password WiFi password (plaintext)
 * @return ESP_OK on success
 */
esp_err_t wifi_secure_store_credentials(const char *ssid, const char *password);

/**
 * @brief Load WiFi credentials from secure storage
 * 
 * Decrypts and returns WiFi credentials from encrypted NVS.
 * 
 * @param[out] ssid Buffer for SSID (min 33 bytes)
 * @param[in] ssid_len SSID buffer size
 * @param[out] password Buffer for password (min 65 bytes)
 * @param[in] pass_len Password buffer size
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no credentials
 */
esp_err_t wifi_secure_load_credentials(char *ssid, size_t ssid_len,
                                          char *password, size_t pass_len);

/**
 * @brief Check if encrypted credentials exist
 * 
 * @return true if encrypted credentials are stored
 */
bool wifi_secure_has_credentials(void);

/**
 * @brief Delete encrypted WiFi credentials
 * 
 * Securely erases encrypted credentials from NVS.
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_secure_delete_credentials(void);

/**
 * @brief Migrate plaintext credentials to encrypted storage
 * 
 * Checks for legacy plaintext credentials and migrates them
 * to encrypted storage. Original plaintext is securely erased.
 * 
 * @return ESP_OK if migration successful or no legacy data
 * @return ESP_ERR_NOT_FOUND if no legacy credentials exist
 * @return Other error codes on failure
 */
esp_err_t wifi_secure_migrate_from_legacy(void);

/**
 * @brief Verify credential integrity
 * 
 * Validates HMAC checksum of stored credentials.
 * 
 * @return ESP_OK if credentials are valid
 * @return ESP_ERR_INVALID_CRC if integrity check fails
 */
esp_err_t wifi_secure_verify_integrity(void);

/**
 * @brief Get encryption status
 * 
 * @param[out] encrypted true if credentials are encrypted
 * @param[out] key_source Current key source type
 * @return ESP_OK on success
 */
esp_err_t wifi_secure_get_status(bool *encrypted, wifi_key_source_t *key_source);

/**
 * @brief Generate and store new encryption key
 * 
 * Generates a fresh encryption key using hardware RNG.
 * Only needed for FLASH key source; EFUSE uses factory key.
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_secure_generate_key(void);

/**
 * @brief Re-encrypt credentials with new key
 * 
 * Decrypts and re-encrypts credentials (for key rotation).
 * 
 * @return ESP_OK on success
 */
esp_err_t wifi_secure_reencrypt(void);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Check if NVS encryption is available
 * 
 * @return true if encryption is supported and configured
 */
bool wifi_secure_encryption_available(void);

/**
 * @brief Get recommended key source
 * 
 * @return Recommended key source for this device
 */
wifi_key_source_t wifi_secure_recommended_key_source(void);

/**
 * @brief Secure clear memory buffer
 * 
 * Securely erases sensitive data from memory.
 * 
 * @param[in,out] buffer Buffer to clear
 * @param len Length of buffer
 */
void wifi_secure_memclear(void *buffer, size_t len);

/**
 * @brief Deinitialize secure storage
 * 
 * Clears sensitive data from memory.
 */
void wifi_secure_storage_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SECURE_STORAGE_H */
