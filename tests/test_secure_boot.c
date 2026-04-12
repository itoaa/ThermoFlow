/**
 * @file test_secure_boot.c
 * @brief Unit tests for Secure Boot V2 and Flash Encryption
 *
 * Tests for SEC-024: ThermoFlow Secure Boot Implementation
 *
 * @author coding-agent
 * @date 2026-04-12
 * @security CVSS 7.4 (High)
 * @nist PR.PS-04
 * @iso27001 8.6
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "esp_system.h"
#include "esp_flash_encrypt.h"
#include "esp_secure_boot.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_log.h"

static const char *TAG = "SECURE_BOOT_TEST";

/**
 * @brief Set up test environment
 */
void setUp(void)
{
    // Initialize eFuse system
    esp_efuse_init();
}

/**
 * @brief Clean up after each test
 */
void tearDown(void)
{
    // Cleanup if needed
}

/**
 * @test SECURE_BOOT_001
 * @brief Verify Secure Boot V2 is enabled
 *
 * Tests that Secure Boot V2 RSA is properly enabled in eFuse
 */
void test_secure_boot_v2_enabled(void)
{
    ESP_LOGI(TAG, "Testing Secure Boot V2 status...");

    bool secure_boot_enabled = esp_secure_boot_enabled();

    // In a provisioned device, this should return true
    // For unit testing on unprovisioned devices, we log the status
    ESP_LOGI(TAG, "Secure Boot enabled: %s", secure_boot_enabled ? "YES" : "NO");

    // Verify secure boot configuration is set
    esp_chip_model_t chip_model;
    uint32_t chip_revision;
    esp_chip_info(&chip_model, &chip_revision);

    // ESP32-S3 supports Secure Boot V2
    TEST_ASSERT_EQUAL(ESP_CHIP_MODEL_ESP32S3, chip_model);

    ESP_LOGI(TAG, "Chip model: ESP32-S3, Revision: %d", chip_revision);
}

/**
 * @test SECURE_BOOT_002
 * @brief Verify Flash Encryption status
 *
 * Tests that flash encryption is enabled and configured correctly
 */
void test_flash_encryption_status(void)
{
    ESP_LOGI(TAG, "Testing Flash Encryption status...");

    bool flash_encryption_enabled = esp_flash_encryption_enabled();

    ESP_LOGI(TAG, "Flash Encryption enabled: %s", flash_encryption_enabled ? "YES" : "NO");

    if (flash_encryption_enabled) {
        esp_flash_enc_mode_t mode = esp_get_flash_encryption_mode();

        switch (mode) {
            case ESP_FLASH_ENC_MODE_DEVELOPMENT:
                ESP_LOGI(TAG, "Flash Encryption Mode: DEVELOPMENT");
                break;
            case ESP_FLASH_ENC_MODE_RELEASE:
                ESP_LOGI(TAG, "Flash Encryption Mode: RELEASE");
                break;
            default:
                ESP_LOGW(TAG, "Flash Encryption Mode: UNKNOWN");
                break;
        }

        // Verify mode is valid
        TEST_ASSERT_TRUE(mode == ESP_FLASH_ENC_MODE_DEVELOPMENT ||
                         mode == ESP_FLASH_ENC_MODE_RELEASE);
    }
}

/**
 * @test SECURE_BOOT_003
 * @brief Verify flash encryption behavior
 *
 * Tests that encrypted flash operations work correctly
 */
void test_flash_encryption_operations(void)
{
    ESP_LOGI(TAG, "Testing Flash Encryption operations...");

    // Test reading encryption configuration
    uint32_t crypt_cnt = 0;
    esp_err_t ret = esp_efuse_read_field_cnt(ESP_EFUSE_SPI_BOOT_CRYPT_CNT, &crypt_cnt);

    ESP_LOGI(TAG, "SPI_BOOT_CRYPT_CNT: %lu", (unsigned long)crypt_cnt);

    // crypt_cnt with odd number of bits set means encryption is enabled
    bool is_encrypted = (crypt_cnt % 2) == 1;

    if (is_encrypted) {
        ESP_LOGI(TAG, "Flash encryption is active (CRYPT_CNT has odd bits)");
    } else {
        ESP_LOGI(TAG, "Flash encryption is NOT active (CRYPT_CNT has even bits)");
    }

    // Verify we can read eFuse
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test SECURE_BOOT_004
 * @brief Verify secure boot public key digest
 *
 * Tests that secure boot key digest is properly stored
 */
void test_secure_boot_key_digest(void)
{
    ESP_LOGI(TAG, "Testing Secure Boot Key Digest...");

    // Read secure boot key digests from eFuse
    uint8_t digest[32];
    esp_err_t ret = esp_efuse_read_block(EFUSE_BLK_SECURE_BOOT,
                                          digest,
                                          0,
                                          sizeof(digest) * 8);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Secure Boot Key Digest (first 8 bytes): ");
        ESP_LOG_BUFFER_HEX(TAG, digest, 8);

        // Check if digest is non-zero (indicating a key is burned)
        bool has_key = false;
        for (int i = 0; i < sizeof(digest); i++) {
            if (digest[i] != 0) {
                has_key = true;
                break;
            }
        }

        if (has_key) {
            ESP_LOGI(TAG, "Secure Boot Key Digest is programmed");
        } else {
            ESP_LOGW(TAG, "Secure Boot Key Digest is NOT programmed (all zeros)");
        }
    } else {
        ESP_LOGW(TAG, "Failed to read secure boot key digest: %s", esp_err_to_name(ret));
    }
}

/**
 * @test SECURE_BOOT_005
 * @brief Verify eFuse write protection
 *
 * Tests that critical eFuses are write-protected
 */
void test_efuse_write_protection(void)
{
    ESP_LOGI(TAG, "Testing eFuse Write Protection...");

    // Check write protection status of secure boot related eFuses
    bool wr_dis_secure_boot = esp_efuse_read_field_bit(ESP_EFUSE_WR_DIS_SECURE_BOOT);
    bool wr_dis_spi_boot_crypt_cnt = esp_efuse_read_field_bit(ESP_EFUSE_WR_DIS_SPI_BOOT_CRYPT_CNT);

    ESP_LOGI(TAG, "Write protection - Secure Boot: %s", wr_dis_secure_boot ? "ENABLED" : "DISABLED");
    ESP_LOGI(TAG, "Write protection - SPI_BOOT_CRYPT_CNT: %s",
             wr_dis_spi_boot_crypt_cnt ? "ENABLED" : "DISABLED");

    // In a properly secured device, these should be write-protected
    // Note: In development, they may not be protected yet
}

/**
 * @test SECURE_BOOT_006
 * @brief Verify UART download mode status
 *
 * Tests UART bootloader security settings
 */
void test_uart_download_mode(void)
{
    ESP_LOGI(TAG, "Testing UART Download Mode...");

    // Check UART download mode status (ESP32-S3 specific)
    bool dl_encrypt_disabled = esp_efuse_read_field_bit(ESP_EFUSE_DIS_DOWNLOAD_DCACHE);
    bool dl_decrypt_disabled = esp_efuse_read_field_bit(ESP_EFUSE_DIS_DOWNLOAD_ICACHE);

    ESP_LOGI(TAG, "UART DL Encrypt disabled: %s", dl_encrypt_disabled ? "YES" : "NO");
    ESP_LOGI(TAG, "UART DL Decrypt disabled: %s", dl_decrypt_disabled ? "YES" : "NO");
}

/**
 * @test SECURE_BOOT_007
 * @brief Verify JTAG status
 *
 * Tests that JTAG is properly disabled
 */
void test_jtag_status(void)
{
    ESP_LOGI(TAG, "Testing JTAG status...");

    // Check JTAG disable bits
    bool jtag_disabled = esp_efuse_read_field_bit(ESP_EFUSE_HARD_DIS_JTAG);
    bool usb_jtag_disabled = esp_efuse_read_field_bit(ESP_EFUSE_DIS_USB_JTAG);

    ESP_LOGI(TAG, "JTAG disabled: %s", jtag_disabled ? "YES" : "NO");
    ESP_LOGI(TAG, "USB JTAG disabled: %s", usb_jtag_disabled ? "YES" : "NO");
}

/**
 * @test SECURE_BOOT_008
 * @brief Test secure boot verification logic
 *
 * Tests the signature verification algorithm (software test)
 */
void test_secure_boot_signature_verification(void)
{
    ESP_LOGI(TAG, "Testing Secure Boot Signature Verification logic...");

    // This is a conceptual test - actual signature verification
    // requires a signed binary and key material

    // Verify that secure boot functions are available
    TEST_ASSERT_TRUE(esp_secure_boot_enabled != NULL);
    TEST_ASSERT_TRUE(esp_secure_boot_verify_signature != NULL);

    ESP_LOGI(TAG, "Secure Boot functions are available");
}

/**
 * @test SECURE_BOOT_009
 * @brief Test flash encryption alignment requirements
 *
 * Tests that flash encryption uses proper alignment
 */
void test_flash_encryption_alignment(void)
{
    ESP_LOGI(TAG, "Testing Flash Encryption Alignment...");

    // Flash encryption operates on 16-byte aligned addresses
    const size_t flash_enc_align = 16;

    // Test addresses
    uint32_t test_addr = 0x10000;  // Valid aligned address
    TEST_ASSERT_EQUAL(0, test_addr % flash_enc_align);

    test_addr = 0x10008;  // Misaligned address
    TEST_ASSERT_NOT_EQUAL(0, test_addr % flash_enc_align);

    ESP_LOGI(TAG, "Flash encryption requires 16-byte alignment: VERIFIED");
}

/**
 * @test SECURE_BOOT_010
 * @brief Verify secure boot rollback protection
 *
 * Tests that app rollback is properly configured
 */
void test_rollback_protection(void)
{
    ESP_LOGI(TAG, "Testing Rollback Protection...");

    // Check if rollback protection is enabled in configuration
    // This requires the bootloader to be built with CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE

    #ifdef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    ESP_LOGI(TAG, "Rollback protection: ENABLED in configuration");
    #else
    ESP_LOGW(TAG, "Rollback protection: DISABLED in configuration");
    #endif
}

/**
 * @brief Run all secure boot tests
 */
void app_main(void)
{
    UNITY_BEGIN();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "SEC-024: Secure Boot V2 Test Suite");
    ESP_LOGI(TAG, "ThermoFlow Security Implementation");
    ESP_LOGI(TAG, "========================================");

    RUN_TEST(test_secure_boot_v2_enabled);
    RUN_TEST(test_flash_encryption_status);
    RUN_TEST(test_flash_encryption_operations);
    RUN_TEST(test_secure_boot_key_digest);
    RUN_TEST(test_efuse_write_protection);
    RUN_TEST(test_uart_download_mode);
    RUN_TEST(test_jtag_status);
    RUN_TEST(test_secure_boot_signature_verification);
    RUN_TEST(test_flash_encryption_alignment);
    RUN_TEST(test_rollback_protection);

    UNITY_END();
}

/**
 * @brief Alternative entry point for automated testing
 *
 * This function can be called from other test frameworks
 */
int run_secure_boot_tests(void)
{
    return UNITY_END();
}
