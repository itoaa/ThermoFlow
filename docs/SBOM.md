# ThermoFlow Software Bill of Materials (SBOM)

**Date:** 2026-07-12  
**Firmware:** 1.3.0

| Component | Version | License | Purpose |
|-----------|---------|---------|---------|
| ESP-IDF | 5.1.x | Apache 2.0 | SDK / FreeRTOS |
| mbedTLS | (bundled) | Apache 2.0 | TLS, AES, HKDF |
| Monocypher | 4.x | BSD-2 / CC0 | Ed25519 signatures |
| cJSON | (bundled) | MIT | JSON API |
| Unity | (bundled) | MIT | Unit tests |

## ThermoFlow Components

| Component | Status |
|-----------|--------|
| hardware_manager | Integrated |
| sensor_manager + sht4x | Integrated |
| fan_control (LEDC) | Integrated |
| wifi_manager + secure storage | Integrated |
| web_server | Integrated (HTTP) |
| mqtt_client / mqtt_ftx | Integrated (on WiFi + broker config) |
| heat_recovery | Integrated |
| ota_manager | Integrated |
| security_utils | Integrated |
| audit_log | Integrated |
| rate_limiter | Integrated |
| display_driver (SSD1306) | Integrated |
| sht3x_sensor | Legacy — not integrated |

## CVE Monitoring

Check ESP-IDF security advisories monthly: https://github.com/espressif/esp-idf/security