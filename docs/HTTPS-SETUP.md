# HTTPS Web Server Setup Guide

## Overview

This guide covers the setup and configuration of HTTPS for the ThermoFlow web server (SEC-018).

**Security Features:**
- TLS 1.3 with fallback to TLS 1.2
- Secure cipher suites only (AEAD: AES-GCM, ChaCha20-Poly1305)
- HTTP-to-HTTPS redirect (port 80 → 443)
- Security headers (HSTS, CSP, X-Frame-Options)
- EJBCA-PKI certificate integration
- Certificate auto-renewal

## Configuration

### Using menuconfig

```bash
cd $IDF_PATH
idf.py menuconfig
```

Navigate to: `Component config → HTTPS Web Server Configuration`

### Key Options

| Option | Default | Description |
|--------|---------|-------------|
| Enable HTTPS | Yes | Enable/disable HTTPS entirely |
| HTTPS Port | 443 | HTTPS server port |
| HTTP Redirect | Yes | Redirect HTTP to HTTPS |
| Minimum TLS | 1.3 | TLS version requirement |
| Enable HSTS | Yes | HTTP Strict Transport Security |
| Auto-provision | Yes | Auto-request certs from EJBCA |

## Certificate Management

### Automatic Provisioning (Recommended)

1. Configure EJBCA server URL in menuconfig
2. Set device ID (or use MAC address default)
3. On first boot, device will:
   - Generate ECDSA key pair (secp256r1)
   - Create CSR with device identity
   - Submit to EJBCA REST API
   - Store returned certificate in encrypted NVS

### Manual Certificate Upload

```c
// Load certificate from string
const char* server_cert = "-----BEGIN CERTIFICATE-----\n...";
const char* server_key = "-----BEGIN PRIVATE KEY-----\n...";

// Store to NVS
security_store_certificate(SEC_CERT_TYPE_SERVER, server_cert, strlen(server_cert));
security_store_certificate(SEC_CERT_TYPE_CLIENT_KEY, server_key, strlen(server_key));
```

### Self-Signed Certificate (Development)

```c
// Generate self-signed certificate
char cert[4096], key[2048];
security_generate_certificate(cert, sizeof(cert), key, sizeof(key), "ThermoFlow-Dev", 365);
```

## API Endpoints

### Certificate Status

```
GET /api/cert/status
```

**Response:**
```json
{
  "has_server_cert": true,
  "has_server_key": true,
  "has_ca_cert": false,
  "days_until_expiry": 364,
  "fingerprint": "a1b2c3d4...",
  "http_running": true,
  "https_running": true
}
```

## Security Headers

All HTTPS responses include these headers:

```
Strict-Transport-Security: max-age=31536000; includeSubDomains
Content-Security-Policy: default-src 'self'; script-src 'self'...
X-Frame-Options: DENY
X-Content-Type-Options: nosniff
X-XSS-Protection: 1; mode=block
Referrer-Policy: strict-origin-when-cross-origin
```

## Testing

### Verify HTTPS

```bash
# Test HTTPS connection
curl -v https://thermoflow-device.local/

# Check certificate
openssl s_client -connect thermoflow-device.local:443 -tls1_3

# Test cipher suites
nmap --script ssl-enum-ciphers -p 443 thermoflow-device.local
```

### SSL Labs-style Testing

```bash
# Check certificate details
openssl x509 -in /path/to/cert.pem -text -noout

# Verify TLS 1.3 support
openssl s_client -connect thermoflow-device.local:443 -tls1_3 </dev/null

# Check for weak ciphers (should fail)
openssl s_client -connect thermoflow-device.local:443 -cipher RC4 </dev/null
```

## Troubleshooting

### Certificate Not Found

**Symptoms:** HTTPS fails to start, falls back to HTTP

**Solution:**
1. Check NVS has certificates: `security_has_certificate(SEC_CERT_TYPE_SERVER)`
2. Enable auto-provisioning in menuconfig
3. Verify EJBCA connectivity

### TLS Handshake Fails

**Symptoms:** `curl` returns SSL errors

**Solution:**
1. Check TLS version compatibility
2. Verify cipher suite support
3. Check certificate validity: `security_is_cert_expired()`

### HTTP Redirect Not Working

**Symptoms:** HTTP requests not redirected to HTTPS

**Solution:**
1. Ensure `WEB_SERVER_HTTP_REDIRECT` is enabled
2. Check port 80 is not used by other services
3. Verify firewall allows port 80

## Implementation Details

### Cipher Suites

Enabled (secure only):
- `TLS_AES_256_GCM_SHA384` (TLS 1.3)
- `TLS_CHACHA20_POLY1305_SHA256` (TLS 1.3)
- `TLS_AES_128_GCM_SHA256` (TLS 1.3)
- `ECDHE-ECDSA-AES256-GCM-SHA384` (TLS 1.2)
- `ECDHE-RSA-AES256-GCM-SHA384` (TLS 1.2)
- `ECDHE-ECDSA-CHACHA20-POLY1305` (TLS 1.2)

Disabled (weak):
- RC4, 3DES, DES
- CBC mode ciphers
- MD5, SHA1
- RSA key exchange (no forward secrecy)

### Certificate Storage

- **Location:** Encrypted NVS partition (`thermoflow_certs`)
- **Server Cert:** Key `server_cert`
- **Server Key:** Key `client_key` (reused for server)
- **CA Cert:** Key `ca_cert` (for mTLS)

### Certificate Renewal

- **Check:** Daily task checks expiry
- **Threshold:** 30 days before expiry
- **Action:** Auto-request new certificate from EJBCA
- **Restart:** HTTPS server restarted with new certificate

## Security Best Practices

1. **Always use HTTPS in production**
2. **Enable HSTS** to prevent SSL stripping
3. **Use strong device IDs** (not default MAC-based)
4. **Monitor certificate expiry** via `/api/cert/status`
5. **Enable mTLS** for additional authentication
6. **Regularly update** trusted CA certificates

## References

- **Security Finding:** THF-003 (CVSS 7.5)
- **ESP-IDF HTTPS:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_https_server.html
- **Mozilla SSL Generator:** https://ssl-config.mozilla.org/
- **EJBCA Documentation:** See `/projects/EJBCA-PKI/docs/`
