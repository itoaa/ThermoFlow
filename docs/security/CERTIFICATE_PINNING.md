# Certificate Pinning Implementation (SEC-034)

## Overview

This document describes the certificate pinning implementation in the ThermoFlow MQTT client, addressing security finding **SEC-034: ThermoFlow Certificate Pinning Completion** (CVSS 5.8).

## Security Framework Mapping

- **CISSP Domain 4:** Communication and Network Security
- **NIST CSF:** PR.PS-03 (Platform security)
- **ISO 27001:** A.8.24 (Use of cryptography)

## Implementation Details

### SPKI Hash Extraction

The implementation uses `mbedtls_x509_crt_info()` for certificate inspection and `mbedtls_pk_write_pubkey_der()` for Subject Public Key Info (SPKI) extraction, as specified in the task requirements.

```c
// Extract SPKI and compute SHA-256 hash
static esp_err_t extract_spki_hash(const mbedtls_x509_crt *cert, uint8_t *hash_out)
{
    // Get certificate info for logging
    char cert_info[512];
    mbedtls_x509_crt_info(cert_info, sizeof(cert_info), "  ", cert);
    
    // Extract public key in DER format
    uint8_t pk_der[1024];
    int ret = mbedtls_pk_write_pubkey_der(&cert->pk, pk_der, sizeof(pk_der));
    
    // Hash with SHA-256
    mbedtls_sha256(pk_start, pk_len, hash_out, 0);
}
```

### Multiple Pin Support

The implementation supports up to **5 pinned certificates** simultaneously, allowing for:

1. **Certificate rotation** - Add new pin before removing old
2. **Backup pins** - Secondary pins for redundancy
3. **Pin expiration** - Each pin can have an expiration timestamp

### Pin Verification Flow

```
┌─────────────────┐
│ TLS Handshake   │
│ Complete        │
└────────┬────────┘
         ▼
┌─────────────────┐
│ Get Peer Cert   │
│ via mbedtls     │
└────────┬────────┘
         ▼
┌─────────────────┐
│ Extract SPKI    │
│ SHA-256 Hash    │
└────────┬────────┘
         ▼
┌─────────────────┐     ┌─────────────────┐
│ Match Against   │────▶│ Pin Match       │
│ Pinned Certs    │     │ Continue TLS    │
└────────┬────────┘     └─────────────────┘
         │ Mismatch
         ▼
┌─────────────────┐     ┌─────────────────┐
│ Enforcement     │────▶│ CA Fallback     │
│ Enabled?        │ No  │ Allowed?        │
└────────┬────────┘     └────────┬────────┘
         │ Yes                   │ Yes
         ▼                       ▼
┌─────────────────┐     ┌─────────────────┐
│ Disconnect      │     │ Continue with   │
│ Log MITM Alert  │     │ Warning         │
└─────────────────┘     └─────────────────┘
```

## API Reference

### Core Pinning Functions

```c
// Add a pinned certificate hash
esp_err_t mqtt_client_add_pinned_cert(mqtt_client_t *client, 
                                       const uint8_t *hash, 
                                       const char *description, 
                                       uint64_t valid_until);

// Remove a pinned certificate
esp_err_t mqtt_client_remove_pinned_cert(mqtt_client_t *client, 
                                          uint8_t pin_index);

// Clear all pins
esp_err_t mqtt_client_clear_pinned_certs(mqtt_client_t *client);
```

### Configuration Functions

```c
// Enable/disable pinning enforcement
esp_err_t mqtt_client_set_pinning_enforcement(mqtt_client_t *client, 
                                               bool enforce);

// Enable/disable CA fallback
esp_err_t mqtt_client_set_ca_fallback(mqtt_client_t *client, 
                                     bool allow);
```

### Hash Calculation

```c
// Calculate SPKI hash from PEM certificate
esp_err_t mqtt_client_calc_spki_hash(const char *cert_pem, 
                                      uint8_t *hash_out);

// Calculate SPKI hash from DER certificate
esp_err_t mqtt_client_calc_spki_hash_der(const uint8_t *cert_der, 
                                          size_t cert_len, 
                                          uint8_t *hash_out);
```

### Remote Pin Updates

Pins can be updated remotely via MQTT using JSON commands:

```json
// Add a new pin
{
    "action": "add",
    "hash_hex": "a1b2c3d4e5f6...",
    "description": "New broker cert",
    "valid_until": 1767225600
}

// Remove a pin
{
    "action": "remove",
    "index": 0
}

// Clear all pins
{
    "action": "clear"
}

// Set enforcement
{
    "action": "set_enforcement",
    "enforce": true
}
```

## Security Considerations

### Timing Attack Prevention

Pin comparison uses constant-time comparison to prevent timing attacks:

```c
// Constant-time comparison
bool match = true;
for (int j = 0; j < MQTT_TLS_PIN_HASH_LEN; j++) {
    match &= (cert_hash[j] == pin->hash[j]);
}
```

### Pin Expiration

Each pinned certificate can have an expiration timestamp. Expired pins are automatically skipped during verification but remain in storage for audit purposes.

### Event Notifications

The client fires `MQTT_EVENT_PIN_MISMATCH` events when pin verification fails, allowing the application to:
- Log security alerts
- Trigger notifications
- Implement rate limiting

## Storage

Pinned certificate configurations are stored in NVS under the namespace `"mqtt_pins"`:

- `pin_config`: Binary blob containing pin configuration
- `pin_version`: Configuration version for migration

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| MQTT_MAX_PINNED_CERTS | 5 | Maximum number of pinned certificates |
| MQTT_TLS_PIN_HASH_LEN | 32 | SHA-256 hash length |
| MQTT_PIN_DESCRIPTION_LEN | 32 | Max description length |
| PIN_CONFIG_VERSION | 2 | Current config version |

## Testing

Run unit tests with:

```bash
idf.py -T components/mqtt_client/test_mqtt_pinning.c test
```

## References

- [OWASP Certificate Pinning Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Pinning_Cheat_Sheet.html)
- [RFC 7469](https://tools.ietf.org/html/rfc7469) - HTTP Public Key Pinning
- [mbedtls x509 documentation](https://tls.mbed.org/api/group__x509__module.html)
