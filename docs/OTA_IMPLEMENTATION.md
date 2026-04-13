# ThermoFlow OTA Implementation

**Document ID:** DEV-001  
**Version:** 1.0.0  
**Date:** 2026-04-13  
**Security Classification:** HIGH (CVSS 8.1)  
**Compliance:** CISSP Domain 8, NIST CSF PR.PS-01, ISO 27001 A.8.19

---

## Executive Summary

This document describes the secure Over-The-Air (OTA) firmware update implementation for ThermoFlow. The implementation addresses the critical security gap identified in SEC-028 where the OTA mechanism was completely stubbed.

### Security Framework Mapping

| Framework | Reference | Implementation |
|-----------|-----------|----------------|
| CISSP CBK | Domain 8: Software Development Security | Secure OTA code signing |
| NIST CSF 2.0 | PR.PS-01 (Platform security) | Firmware integrity verification |
| ISO 27001:2022 | A.8.19 (Installation of software) | Controlled software deployment |

---

## Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    OTA Manager Component                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │   HTTPS     │  │  Signature  │  │  Rollback   │           │
│  │   Client    │  │ Verification│  │  Protection │           │
│  └─────────────┘  └─────────────┘  └─────────────┘           │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │   State     │  │  Partition  │  │    NVS      │           │
│  │   Machine   │  │  Management │  │   Storage   │           │
│  └─────────────┘  └─────────────┘  └─────────────┘           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      ESP32-S3 Platform                          │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │ Secure Boot  │  │   Flash      │  │    OTA       │          │
│  │     V2       │  │  Encryption  │  │  Partitions  │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

---

## Implementation Details

### 1. HTTPS Download (`ota_manager.c`)

The OTA manager uses ESP-IDF's HTTPS client with the following security features:

- **TLS 1.3** with fallback to TLS 1.2
- **Certificate validation** using the ESP certificate bundle or custom CA
- **Certificate pinning** support (optional, configure via `ota_config_t`)
- **Strict HTTPS enforcement** - HTTP URLs are rejected in production mode

```c
// Example: Start OTA download
ota_manager_start_download(
    "https://ota.example.com/firmware-v1.2.3.bin",
    expected_size,
    expected_hash
);
```

### 2. Signature Verification

Two layers of signature verification:

#### Layer 1: Ed25519 Application-Level Signatures
- Firmware signed with Ed25519 private key
- Signature verified before booting
- Public key stored in NVS or embedded in firmware

#### Layer 2: Secure Boot V2 (RSA-3072)
- Bootloader verifies RSA-3072 signature on boot
- Hardware-enforced trust chain
- Prevents execution of unsigned firmware

```c
// Load public key for signature verification
uint8_t public_key[ED25519_PUBLIC_KEY_LEN];
// ... load key ...
ota_manager_load_public_key(public_key);
```

### 3. Rollback Protection (Anti-Rollback)

Prevents downgrade attacks to vulnerable firmware versions:

```c
// Set minimum security version
ota_manager_set_min_security_version(5);

// Security version embedded in firmware version string
// e.g., "1.2.3-sec5" where 5 is the security version
```

**Mechanism:**
1. Security version stored in NVS
2. New firmware must have security_version >= stored version
3. Automatic rollback if new firmware fails to boot

### 4. Partition Management

The secure partition table (`partitions_secure.csv`) includes:

```csv
# OTA partition for updates
ota_0,    app,  ota_0,   0x210000, 0x200000,

# OTA data partition (stores OTA metadata)
otadata,  data, ota,     0x410000, 0x2000,
```

**Dual Partition Safety:**
- Factory partition is always preserved
- OTA_0 partition for updates
- Automatic rollback on boot failure

---

## API Reference

### Initialization

```c
// Configure OTA manager
ota_config_t config = {
    .use_https = true,
    .verify_signature = true,
    .verify_hash = true,
    .enable_anti_rollback = true,
    .min_security_version = 1,
    .event_cb = ota_event_callback,
    .ca_cert = custom_ca_cert_pem,  // NULL for default bundle
};

// Initialize
esp_err_t ret = ota_manager_init(&config);
```

### Download and Update

```c
// Start download
esp_err_t ret = ota_manager_start_download(
    "https://ota.example.com/firmware.bin",
    expected_size,      // 0 if unknown
    expected_hash       // SHA-256 hash (NULL to skip)
);

// Or with full update info
ota_update_info_t update = {
    .version = "1.2.3-sec2",
    .firmware_size = 1048576,
    // ... hash and signature ...
    .url = "https://ota.example.com/firmware.bin",
};
ota_manager_start_update(&update);

// Apply update
ota_manager_apply_update(true);  // true = restart immediately
```

### Post-Update Validation

```c
void app_main(void) {
    // ... initialization ...
    
    // Check if this is first boot after OTA
    ota_status_t status;
    ota_manager_get_status(&status);
    
    if (status.can_rollback) {
        // Verify subsystems
        bool all_ok = verify_subsystems();
        
        if (all_ok) {
            // Mark as valid (prevents rollback)
            ota_manager_mark_valid();
        }
    }
}
```

---

## Security Considerations

### Threat Model

| Threat | Mitigation |
|--------|------------|
| **Man-in-the-Middle** | TLS 1.3 with certificate validation |
| **Malicious Firmware** | Ed25519 + RSA-3072 signature verification |
| **Rollback Attacks** | Security version anti-rollback |
| **Network Interruption** | Resume capability, timeout handling |
| **Flash Corruption** | SHA-256 hash verification |

### Certificate Management

1. **Development**: Use ESP certificate bundle or self-signed CA
2. **Production**: Use properly signed CA certificate
3. **Pinning**: Consider certificate pinning for production

```c
// Enable certificate pinning
ota_config_t config = {
    .use_certificate_pinning = true,
    .pinned_cert_hash = { /* SHA-256 hash of expected cert */ },
};
```

### Key Storage

**OTA Signing Key:**
- Store in HSM (YubiHSM, NitroKey) for production signing
- Use secure key ceremony per SEC-029
- Never expose private key in source control

**Public Key on Device:**
- Can be embedded in firmware or stored in NVS
- Does not need protection (public key)
- Verify key integrity on load

---

## Integration with MQTT

The OTA manager integrates with the MQTT client for remote update triggers:

```json
{
  "command": "update",
  "version": "1.2.3",
  "url": "https://ota.example.com/firmware-v1.2.3.bin",
  "size": 1048576,
  "hash": "base64_encoded_sha256_hash",
  "signature": "base64_encoded_ed25519_signature",
  "force": false
}
```

**Topic Convention:**
- Command: `thermoflow/ota/command`
- Status: `thermoflow/ota/status`
- Progress: `thermoflow/ota/progress`

---

## Testing

### Unit Tests

```bash
# Build test firmware
cd ${IDF_PATH}/tools/unit-test-app
idf.py build

# Run OTA tests
idf.py -p /dev/ttyUSB0 flash monitor
```

### Integration Tests

1. **Happy Path:**
   ```
   - Start OTA download
   - Verify download completes
   - Verify signature
   - Apply update
   - Verify boot success
   - Mark valid
   ```

2. **Signature Failure:**
   ```
   - Download firmware with invalid signature
   - Verify rejection
   - Verify rollback
   ```

3. **Rollback Test:**
   ```
   - Apply update
   - Simulate boot failure (don't call mark_valid)
   - Reboot
   - Verify automatic rollback
   ```

4. **Anti-Rollback:**
   ```
   - Set security_version to 5
   - Attempt to install version with sec_version 3
   - Verify rejection
   ```

### Manual Testing

```bash
# 1. Sign firmware
./scripts/sign_firmware.sh sign build/ThermoFlow.bin build/ThermoFlow-signed.bin

# 2. Host firmware on HTTPS server
python3 -m http.server 8443 --bind 127.0.0.1 --cert server.pem --key server-key.pem

# 3. Trigger OTA via MQTT or API
mosquitto_pub -t "thermoflow/ota/command" -m '{"url":"https://192.168.1.100:8443/ThermoFlow-signed.bin","version":"1.2.3"}'

# 4. Monitor progress
mosquitto_sub -t "thermoflow/ota/status"
```

---

## Build Configuration

### sdkconfig.defaults

Ensure these options are enabled:

```
# OTA support
CONFIG_ESP_HTTPS_OTA=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_secure.csv"

# Secure Boot V2
CONFIG_SECURE_BOOT_V2_RSA=y
CONFIG_SECURE_SIGNED_ON_UPDATE=y

# Flash Encryption
CONFIG_SECURE_FLASH_ENC_ENABLED=y
```

### Component Dependencies

Add to `CMakeLists.txt`:

```cmake
idf_component_register(
    ...
    REQUIRES ota_manager
)
```

---

## Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| "No OTA partition" | Partition table missing OTA | Use `partitions_secure.csv` |
| "Signature verify failed" | Wrong public key | Load correct public key |
| "TLS handshake failed" | Certificate issue | Check CA certificate |
| "Version too old" | Anti-rollback triggered | Increment security version |
| "Flash write error" | Insufficient space | Erase partition first |

### Debug Logging

Enable verbose logging:

```
CONFIG_LOG_DEFAULT_LEVEL_VERBOSE=y
```

Monitor OTA logs:
```
I (12345) OTA_MANAGER: OTA manager initialized
I (12346) OTA_MANAGER:   HTTPS: enabled
I (12347) OTA_MANAGER:   Signature verification: enabled
I (12348) OTA_MANAGER:   Anti-rollback: enabled
```

---

## References

- **SEC-028:** OTA Audit Report (Source of DEV-001)
- **SEC-024:** Secure Boot V2 Implementation
- **SEC-015:** MQTT Certificate Pinning
- **ESP-IDF OTA:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/ota.html
- **ESP-IDF Secure Boot:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0.0 | 2026-04-13 | coding-agent | Initial implementation |

---

**Security Notice:** This document contains security-critical information. Handle according to organizational security policies.