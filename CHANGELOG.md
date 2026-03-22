# ThermoFlow Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2026-03-22

### Added (Initial Release)
- ESP32-S3 support with WiFi and BLE
- SHT40 sensor driver (I2C)
- Multi-sensor support (2-4 sensors)
- MQTT integration with Home Assistant
- Local OLED display support
- Web configuration interface
- Secure OTA updates (Ed25519)
- Fan PWM control (1-2 fans)
- Anti-condensation protection (>90% RH)
- Security framework (IEC 62443 SL-2)

### Security
- TLS 1.3 for MQTT and Web
- Certificate pinning
- Signed OTA updates
- Fail-safe fan control
- Authentication for control operations

---

## Security Advisory

### [SEC-2026-03-22-001] Initial Security Framework
**Severity:** Informational  
**Description:** Security framework established per IEC 62443 SL-2.  
**Status:** Implemented

---

## References

- [Project Framework](PROJECT_FRAMEWORK.md)
