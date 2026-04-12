# ThermoFlow HTTPS Web Server

## Overview

This document describes the HTTPS web server implementation for ThermoFlow, addressing **Security Finding THF-003 (CVSS 7.5)**.

The implementation provides secure HTTPS communication with certificate management, auto-renewal, and integration with EJBCA-PKI.

## Features

### Security
- **TLS 1.3** with fallback to TLS 1.2 (strong cipher suites only)
- **HTTP-to-HTTPS redirect** on port 80
- **Security headers** (HSTS, CSP, X-Frame-Options, etc.)
- **Certificate pinning** support
- **Mutual TLS (mTLS)** support (optional)

### Certificate Management
- Automatic certificate provisioning
- Integration with EJBCA-PKI
- Self-signed certificate fallback
- Certificate auto-renewal (30 days before expiry)
- Encrypted NVS storage

## Configuration

### Kconfig Options

```
CONFIG_WEB_SERVER_HTTPS_ENABLED=y              # Enable HTTPS
CONFIG_WEB_SERVER_HTTP_REDIRECT=y              # Redirect HTTP to HTTPS
CONFIG_WEB_SERVER_TLS_VERSION_1_3=y            # Require TLS 1.3
CONFIG_WEB_SERVER_ENABLE_HSTS=y                # Enable HSTS
CONFIG_WEB_SERVER_ENABLE_CSP=y                 # Enable CSP
CONFIG_WEB_SERVER_AUTO_PROVISION=y             # Auto-provision certificates
```

### Programmatic Configuration

```c
#include "web_server.h"

// Configure HTTPS
https_config_t config;
web_server_get_default_https_config(&config);

config.use_https = true;
config.https_port = 443;
config.enable_http_redirect = true;
config.enable_hsts = true;
config.auto_provision = true;

web_server_set_https_config(&config);

// Start HTTPS server
esp_err_t ret = web_server_start_https();
if (ret != ESP_OK) {
    // Handle error
}
```

## Certificate Management

### Automatic Provisioning

The certificate manager automatically:
1. Generates RSA key pair (2048-bit)
2. Creates CSR for EJBCA enrollment
3. Stores certificate in encrypted NVS
4. Monitors expiry and renews when needed

### Manual Certificate Installation

```c
#include "cert_manager.h"

// Store existing certificate
cert_manager_store(cert_pem, key_pem);
```

### Certificate Status

```c
cert_manager_status_t status;
cert_manager_get_status(&status);

printf("Certificate valid for %d days\n", status.days_remaining);
printf("Needs renewal: %s\n", status.needs_renewal ? "yes" : "no");
printf("Fingerprint: %s\n", status.fingerprint);
```

## API Endpoints

All endpoints return JSON responses with security headers.

### Device Information
- `GET /api/device/info` - Device status, MAC, firmware version

### FTX Data
- `GET /api/ftx` - Complete heat recovery data
- `GET /api/ftx/sensors` - Sensor readings only
- `GET /api/ftx/efficiency` - Efficiency calculations
- `GET /api/ftx/status` - Status flags
- `POST /api/ftx/control` - Control commands

### Certificate Management
- `GET /api/cert/status` - Certificate status

### Hardware
- `GET /api/hardware` - Hardware detection status

### WiFi
- `POST /api/wifi/config` - Configure WiFi credentials

## Security Headers

All HTTPS responses include:

```
Strict-Transport-Security: max-age=31536000; includeSubDomains; preload
Content-Security-Policy: default-src 'self'; script-src 'self'; ...
X-Frame-Options: DENY
X-Content-Type-Options: nosniff
X-XSS-Protection: 1; mode=block
Referrer-Policy: strict-origin-when-cross-origin
Permissions-Policy: geolocation=(), microphone=(), camera=()
```

## Cipher Suites

Only AEAD cipher suites are enabled:

**TLS 1.3:**
- TLS_AES_256_GCM_SHA384
- TLS_CHACHA20_POLY1305_SHA256
- TLS_AES_128_GCM_SHA256

**TLS 1.2:**
- ECDHE_ECDSA_WITH_AES_256_GCM_SHA384
- ECDHE_RSA_WITH_AES_256_GCM_SHA384
- ECDHE_ECDSA_WITH_AES_128_GCM_SHA256
- ECDHE_RSA_WITH_AES_128_GCM_SHA256
- ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
- ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256

Weak ciphers (RC4, 3DES, CBC mode) are disabled.

## Compliance

- **CISSP Domain 4**: Communication and Network Security
- **NIST CSF**: PROTECT.PR.DS-02 (Data protection in transit)
- **ISO 27001**: 8.6 (Network security management)
- **IoT Baseline Security**: Encrypted communications

## Files

| File | Description |
|------|-------------|
| `components/web_server/web_server.c` | HTTPS server implementation |
| `components/web_server/include/web_server.h` | Web server API |
| `components/web_server/Kconfig` | Configuration options |
| `components/cert_manager/cert_manager.c` | Certificate lifecycle management |
| `components/cert_manager/include/cert_manager.h` | Certificate manager API |
| `sdkconfig.defaults` | Default ESP-IDF configuration |

## Testing

### Build
```bash
idf.py build
```

### Flash
```bash
idf.py flash
```

### Monitor
```bash
idf.py monitor
```

### Verify HTTPS
```bash
curl -k https://<device_ip>/api/device/info
curl -v --insecure https://<device_ip>/api/device/info
```

## Troubleshooting

### Certificate Issues

**No certificate found:**
```
I (1234) CERT_MGR: No certificate found, provisioning new...
```
The system will auto-generate a self-signed certificate.

**Certificate expired:**
```
E (1234) CERT_MGR: Certificate EXPIRED! Immediate renewal required.
```
Check NTP time sync. Certificates may appear expired if time is wrong.

### Connection Issues

**Check ports:**
- Port 80: HTTP redirect server
- Port 443: HTTPS server

**Check TLS version:**
```bash
openssl s_client -connect <device_ip>:443 -tls1_3
```

## References

- [ESP-IDF HTTPS Server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_https_server.html)
- [mbedTLS Documentation](https://tls.mbed.org/)
- [Security Assessment Report](../docs/security/reports/Security-Assessment-Report-2026-04-12.md) (THF-003)
- [SEC-026 Task Specification](../tasks/SEC-026.md)

## Changelog

### v2.0.0 (2026-04-12)
- Initial HTTPS implementation
- Certificate manager with auto-renewal
- Security headers (HSTS, CSP, etc.)
- TLS 1.3 support
- EJBCA-PKI integration framework
