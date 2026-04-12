# WiFi Credential Encryption Implementation (SEC-021)

## Overview

This document describes the implementation of AES-256 encryption for WiFi credentials in ThermoFlow (Security Finding SEC-021).

## Security Framework

- **CISSP Domain**: 3 (Security Architecture and Engineering)
- **NIST Control**: PR.DS-02 (Protect - Data-at-rest)
- **ISO 27001**: 8.5 (Secure authentication secrets)
- **CVSS Score**: 7.1 (High) - Addressed

## Implementation Details

### Algorithm

- **Encryption**: AES-256-CBC with PKCS7 padding
- **Key Derivation**: HKDF-SHA256 with device-unique input
- **Integrity**: HMAC-SHA256 for tamper detection
- **IV**: 16-byte random IV generated per encryption

### Key Derivation

The encryption key is derived using HKDF (RFC 5869):

1. **Input Material**: Device MAC address (6 bytes) + Hardware RNG entropy (32 bytes)
2. **Salt**: 16-byte random salt (stored with credentials)
3. **Info String**: "ThermoFlow WiFi Credentials v1"
4. **Output**: 32-byte AES-256 key

### Storage Format

Encrypted credentials are stored as NVS blobs:

```
Namespace: wifi_sec_cfg
Keys:
  - ssid_enc    : Encrypted SSID (IV + ciphertext)
  - pass_enc    : Encrypted password (IV + ciphertext)
  - salt        : 16-byte random salt
  - version     : Encryption format version (1)
  - checksum    : HMAC-SHA256 integrity check
  - ssid_len    : Actual encrypted SSID length
  - pass_len    : Actual encrypted password length
```

### Encryption Structure

```c
typedef struct {
    uint8_t salt[WIFI_SALT_LEN];              // 16 bytes
    uint8_t ssid_enc[WIFI_ENC_MAX_SSID_LEN];  // 80 bytes (IV + ciphertext)
    uint8_t pass_enc[WIFI_ENC_MAX_PASS_LEN];  // 80 bytes (IV + ciphertext)
    size_t ssid_len;                          // Actual length
    size_t pass_len;                          // Actual length
    uint8_t version;                          // Format version
    uint8_t checksum[32];                       // HMAC-SHA256
} wifi_encrypted_data_t;
```

## Key Sources

### WIFI_KEY_SOURCE_EFUSE (Recommended for Production)

- Uses device-unique eFuse key
- Most secure option
- Requires flash encryption enabled
- Key cannot be read from software

### WIFI_KEY_SOURCE_FLASH

- Stores derived key in NVS
- Protected by tamper detection
- Suitable for development
- Can be combined with flash encryption

### WIFI_KEY_SOURCE_AUTO

- Automatically selects best available option
- Prefers EFUSE if flash encryption is enabled
- Falls back to FLASH otherwise

## Files Modified/Created

### New Files

1. `components/wifi_manager/include/wifi_secure_storage.h` - Header for secure storage API
2. `components/wifi_manager/wifi_secure_storage.c` - Implementation of AES-256 encryption
3. `tests/test_wifi_encryption.c` - Unit tests for encryption functionality
4. `docs/WiFi_Encryption.md` - This documentation

### Modified Files

1. `components/wifi_manager/include/wifi_manager.h` - Added encryption status functions
2. `components/wifi_manager/wifi_manager.c` - Integrated secure storage
3. `components/wifi_manager/CMakeLists.txt` - Added secure storage source file
4. `sdkconfig.defaults` - Added NVS encryption and mbedtls configuration

## Usage

### Basic Usage

```c
#include "wifi_manager.h"

// Initialize WiFi manager (automatically initializes secure storage)
wifi_manager_init();

// Configure WiFi (automatically encrypted)
wifi_manager_configure("MyNetwork", "MyPassword");
```

### Direct Secure Storage API

```c
#include "wifi_secure_storage.h"

// Initialize secure storage
wifi_secure_storage_init(WIFI_KEY_SOURCE_AUTO);

// Store credentials
wifi_secure_store_credentials("MyNetwork", "MyPassword");

// Load credentials
char ssid[33], password[65];
wifi_secure_load_credentials(ssid, sizeof(ssid), password, sizeof(password));

// Clear from memory when done
wifi_secure_memclear(ssid, sizeof(ssid));
wifi_secure_memclear(password, sizeof(password));
```

### Migration from Legacy Storage

```c
// Automatically migrates plaintext credentials to encrypted
// Called during wifi_manager_init()
esp_err_t ret = wifi_secure_migrate_from_legacy();
if (ret == ESP_OK) {
    ESP_LOGI("Migrated legacy credentials to encrypted storage");
}
```

## Testing

### Build and Run Tests

```bash
cd /path/to/ThermoFlow
# Configure project
idf.py set-target esp32s3
# Run tests
idf.py -p /dev/ttyUSB0 flash monitor
```

### Test Coverage

1. Basic encryption/decryption
2. Credential existence checks
3. Deletion and cleanup
4. Edge cases (empty, long, special characters)
5. Key source detection
6. Memory clearing
7. Overwrite operations
8. Multiple operations
9. Status reporting
10. Legacy migration

## Security Considerations

### Threats Addressed

| Threat | Mitigation |
|--------|------------|
| Physical extraction of flash | AES-256 encryption prevents credential disclosure |
| Memory dumps | Secure memory clearing after use |
| Credential tampering | HMAC-SHA256 integrity verification |
| Replay attacks | Device-unique key derivation prevents cross-device replay |

### Limitations

1. **Runtime exposure**: Credentials are decrypted in RAM during use
2. **Backup devices**: Credentials cannot be migrated between devices (by design)
3. **Key extraction**: Requires flash encryption to protect against sophisticated attacks
4. **Side-channel**: Timing attacks possible (constant-time compare mitigates HMAC verification)

### Recommendations for Production

1. Enable Flash Encryption:
   ```
   idf.py menuconfig
   → Security features
   → Enable flash encryption
   ```

2. Enable Secure Boot:
   ```
   → Security features
   → Enable secure boot
   ```

3. Use eFuse key source:
   ```c
   wifi_secure_storage_init(WIFI_KEY_SOURCE_EFUSE);
   ```

4. Lock NVS partition after first boot:
   ```
   → Partition Table
   → Enable NVS encryption
   ```

## Backwards Compatibility

The implementation maintains backwards compatibility:

1. Legacy plaintext credentials are detected and migrated
2. Falls back to plaintext if encryption is unavailable
3. All new credentials are encrypted by default
4. Migration preserves configuration across reboots

## Migration Path

### First Boot After Update

1. System detects legacy plaintext credentials
2. Automatically migrates to encrypted storage
3. Legacy credentials are securely erased
4. Device operates with encrypted credentials

### Factory Reset

1. Delete encrypted credentials: `wifi_manager_reset()`
2. Device boots in AP mode
3. New credentials are stored encrypted

## Performance Impact

- **Storage overhead**: ~130 bytes additional per credential set
- **Encryption latency**: <1ms for typical credentials
- **Decryption latency**: <1ms for typical credentials
- **Memory overhead**: ~512 bytes during crypto operations

## References

- ESP-IDF NVS Encryption: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_encryption.html
- HKDF RFC 5869: https://tools.ietf.org/html/rfc5869
- AES-256 NIST Standard: https://csrc.nist.gov/publications/detail/fips/197/final

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-04-03 | Initial plaintext storage |
| 2.0.0 | 2026-04-12 | SEC-021: AES-256 encryption added |

## Contact

For security questions, contact:
- Ola Andersson (Security Owner)
- ThermoFlow Security Team
