# MQTT-TLS Implementation for ThermoFlow

**Security Remediation: SEC-016**  
**Finding: THF-CRIT-001**  
**CVSS Score: 9.2 → Remediated**

---

## Overview

This component implements MQTT over TLS (MQTTS) for the ThermoFlow ESP32 IoT project, addressing the critical security vulnerability where MQTT communications were transmitted in plaintext.

## Security Improvements

| Aspect | Before | After |
|--------|--------|-------|
| **Encryption** | ❌ Plaintext MQTT (port 1883) | ✅ MQTTS (port 8883) |
| **TLS Version** | ❌ None | ✅ TLS 1.3 with fallback to 1.2 |
| **Certificate Validation** | ❌ None | ✅ Server certificate verification |
| **Certificate Pinning** | ❌ None | ✅ Optional certificate pinning |
| **mTLS** | ❌ None | ✅ Client certificate authentication |
| **Cipher Suites** | ❌ Weak ciphers allowed | ✅ Strong AEAD ciphers only |

## Architecture

```
┌─────────────────┐
│   Application   │
│  (ThermoFlow)   │
└────────┬────────┘
         │
┌────────▼────────┐
│   mqtt_client   │  MQTT protocol handling
│    (new API)    │
└────────┬────────┘
         │
┌────────▼────────┐
│    esp-tls      │  TLS 1.3/1.2 handshake
└────────┬────────┘
         │
┌────────▼────────┐
│    mbedtls    │  Cryptographic operations
└────────┬────────┘
         │
┌────────▼────────┐
│  WiFi/Ethernet  │  Network layer
└─────────────────┘
```

## Files Created/Modified

### New/Updated Files

1. **mqtt_client.h** - Complete header rewrite with TLS structures
   - `mqtt_tls_config_t` - TLS configuration structure
   - `mqtt_client_create()` - Instance-based client creation
   - `mqtt_tls_config_from_nvs()` - Load certs from secure storage
   - `mqtt_tls_set_pinning()` - Certificate pinning

2. **mqtt_client.c** - Full implementation
   - TLS handshake via ESP-TLS
   - Certificate validation
   - Optional certificate pinning
   - Thread-safe operations with mutex

3. **Kconfig** - Configuration options
   - `MQTT_TLS_ENABLED` - Enable/disable TLS
   - `MQTT_TLS_VERSION` - TLS 1.2 or 1.3
   - `MQTT_TLS_CERT_PINNING` - Enable pinning
   - `MQTT_TLS_MTLS` - Mutual TLS authentication

4. **CMakeLists.txt** - Build system integration
   - Links to esp-tls, mbedtls, esp_crt_bundle
   - Conditional compilation based on Kconfig

5. **security_manager.h/c** - Certificate management
   - `security_store_certificate()` - Store certs in NVS
   - `security_load_certificate()` - Load certs from NVS
   - `security_is_cert_expired()` - Check certificate expiry
   - `security_generate_csr()` - Generate certificate signing requests

## Configuration

### Via menuconfig

```bash
idf.py menuconfig
# Navigate to: Component config → MQTT-TLS Configuration
```

### Programmatic Configuration

```c
#include "mqtt_client.h"
#include "security_manager.h"

// Configure TLS
mqtt_tls_config_t tls_config = {
    .use_tls = true,
    .verify_server_cert = true,
    .skip_common_name_check = false,
    .use_client_cert = false,
    .certificate_pinning = false
};

// Load certificates from NVS (recommended)
mqtt_tls_config_from_nvs(&tls_config);

// Or set certificates manually (not recommended)
tls_config.ca_cert = ca_cert_pem;
tls_config.ca_cert_len = strlen(ca_cert_pem);

// MQTT configuration
mqtt_config_t config = {
    .broker_host = "mqtt.example.com",
    .port = 8883,
    .client_id = "thermoflow-001",
    .keepalive_sec = 60,
    .tls = tls_config
};

// Create and start client
mqtt_client_handle_t client;
mqtt_client_create(&client);
mqtt_client_configure(client, &config);
mqtt_client_start(client);
```

## Certificate Provisioning

### Method 1: NVS Flash (Recommended)

```c
// Store CA certificate
const char* ca_cert = "-----BEGIN CERTIFICATE-----\n...";
security_store_certificate(SEC_CERT_TYPE_CA, ca_cert, strlen(ca_cert));

// Store client certificate (for mTLS)
const char* client_cert = "-----BEGIN CERTIFICATE-----\n...";
security_store_certificate(SEC_CERT_TYPE_CLIENT, client_cert, strlen(client_cert));

// Store client key
const char* client_key = "-----BEGIN PRIVATE KEY-----\n...";
security_store_certificate(SEC_CERT_TYPE_CLIENT_KEY, client_key, strlen(client_key));
```

### Method 2: Embedded in Firmware

Enable `CONFIG_MQTT_TLS_CA_CERT_EMBED` and set `CONFIG_MQTT_TLS_CA_CERT_PATH` to include the certificate at compile time.

### Method 3: EJBCA-PKI Integration

The security manager supports certificate lifecycle management with EJBCA-PKI:

```c
// Generate CSR for EJBCA
char csr[2048];
security_generate_csr(csr, sizeof(csr), "thermoflow-001", "ThermoFlow", "SE");

// Submit CSR to EJBCA and receive signed certificate
// Store certificate from EJBCA response
security_store_certificate(SEC_CERT_TYPE_CLIENT, cert_pem, cert_len);
```

## Certificate Pinning

For additional security, implement certificate pinning:

```c
// Calculate hash of expected certificate
uint8_t pin_hash[32];
security_calc_cert_fingerprint(expected_cert_pem, pin_hash);

// Store pinning hash
mqtt_tls_set_pinning(&tls_config, pin_hash);
// OR
security_store_pinning_hash(pin_hash);
```

## Testing

### Unit Tests

```c
// Test TLS connection
void test_mqtt_tls_connection() {
    mqtt_client_handle_t client;
    mqtt_client_create(&client);
    
    mqtt_config_t config = {
        .broker_host = "test.mosquitto.org",
        .port = 8883,
        .tls = { .use_tls = true }
    };
    
    mqtt_client_configure(client, &config);
    esp_err_t ret = mqtt_client_start(client);
    
    assert(ret == ESP_OK);
    assert(mqtt_client_get_status(client) == MQTT_STATUS_CONNECTED);
    
    mqtt_client_destroy(client);
}
```

### Manual Testing

1. **Verify TLS version:**
   ```bash
   openssl s_client -connect <broker>:8883 -tls1_3
   ```

2. **Check cipher suites:**
   ```bash
   nmap --script ssl-enum-ciphers -p 8883 <broker>
   ```

3. **Wireshark verification:**
   - Capture port 8883 traffic
   - Verify no plaintext MQTT packets
   - Confirm TLS 1.3 handshake

## Security Considerations

### Do

- ✅ Always use TLS 1.3 when available
- ✅ Verify server certificates
- ✅ Enable certificate pinning for known brokers
- ✅ Store certificates in encrypted NVS
- ✅ Use mTLS for mutual authentication
- ✅ Implement certificate rotation

### Don't

- ❌ Disable TLS in production
- ❌ Skip certificate verification
- ❌ Allow plaintext fallback
- ❌ Store private keys unencrypted
- ❌ Use self-signed certificates in production

## Integration with EJBCA-PKI

The implementation integrates with the existing EJBCA-PKI project for certificate lifecycle management:

1. **Certificate Enrollment:**
   - Generate CSR on device
   - Submit to EJBCA via secure channel
   - Receive and store signed certificate

2. **Certificate Renewal:**
   - Monitor certificate expiry
   - Auto-renew before expiration
   - Update NVS with new certificate

3. **Revocation:**
   - Support CRL checking
   - OCSP stapling (future enhancement)

## Compliance

This implementation addresses:

- **IEC 62443-3-3** SR-2.1: Cryptographic authentication
- **IEC 62443-3-3** SR-3.1: Communication integrity
- **IEC 62443-3-3** SR-3.2: Communication confidentiality
- **IEC 62443-4-2** CR-2.1: Authentication
- **NIST SP 800-82** - Industrial Control System Security

## References

- [ESP-IDF ESP-TLS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_tls.html)
- [MQTT Specification v3.1.1](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html)
- [TLS 1.3 RFC 8446](https://tools.ietf.org/html/rfc8446)
- EJBCA-PKI: `/home/ola/.openclaw/workspace/projects/EJBCA-PKI/`

## Change Log

| Version | Date | Changes |
|---------|------|---------|
| 2.0.0 | 2026-04-12 | SEC-016: Implemented MQTT-TLS |
| 1.0.0 | 2026-03-22 | Initial MQTT stub implementation |

## Authors

- **Security Assessment:** Ola Andersson
- **Implementation:** coding-agent (orchestrated)
- **Review:** security-agent-daily

---

**Status:** ✅ COMPLETE  
**Next Task:** SEC-017 - CA Audit Logging System
