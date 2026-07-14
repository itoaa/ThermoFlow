# ThermoFlow Threat Model (STRIDE)

**Version:** 1.0.0  
**Date:** 2026-07-12  
**Firmware:** CalVer `2026.29.BUILD` (see [VERSIONING.md](../VERSIONING.md))

## System Boundary

- ESP32-S3 device (sensors, fans, WiFi, HTTP API)
- Home network / MQTT broker
- OTA update server

## Assets

| Asset | Impact if compromised |
|-------|----------------------|
| WiFi credentials | Network access |
| Fan control | Physical damage / condensation |
| Firmware image | Full device takeover |
| Sensor data | Privacy |

## STRIDE Summary

| Threat | Example | Mitigation (implemented) |
|--------|---------|--------------------------|
| Spoofing | Fake MQTT broker | TLS + optional cert pinning |
| Tampering | Unsigned OTA | esp_https_ota + Ed25519 (Monocypher) |
| Repudiation | Denied config change | audit_log component |
| Information disclosure | HTTP sniffing | WiFi credential encryption; HTTPS planned |
| Denial of service | API flood | rate_limiter on web_server |
| Elevation of privilege | Open OTA | Signature verification, anti-rollback |

## Residual Risks

1. HTTP used in development mode — use HTTPS in production
2. Git history may contain old signing keys — rotate and purge
3. MicroSD logging not yet implemented — audit log is in-memory only

## Review Schedule

Next review: 2026-10-12