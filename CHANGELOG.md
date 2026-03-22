# ThermoFlow Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.2.0] - 2026-03-22

### Changed
- **Build System**: Migrated from PlatformIO to pure ESP-IDF
- **Documentation**: Updated all build instructions for ESP-IDF
- **Code Quality**: Enhanced inline documentation across all modules
- **Cleanup**: Removed duplicate .cpp files (keeping .c only)

### Fixed
- Added missing `#include <esp_chip_info.h>` to main.c
- Added missing `#include <string.h>` and `esp_timer` to fan_controller.c
- Fixed `anti_condensation.h` with complete callback API
- Fixed struct member access in rate_limiter.c

### Build
- Binary size: 221 KB (79% flash space free)
- Target: ESP32-S3
- ESP-IDF v5.1.2

---

## [1.1.0] - 2026-03-22

### Added
- Unit tests with Unity framework (test_sht4x, test_fan_controller, test_anti_condensation)
- Full ASCII font (5x7, 96 characters) for display
- Ed25519 OTA signing framework (stubs, needs libsodium integration)
- Token bucket rate limiter for web/MQTT/login protection
- Audit logging with 50-entry circular buffer and checksum integrity

### Security
- SR-005: Audit Logging implemented
- SR-006: Resource Limits implemented

---

## [1.0.0] - 2026-03-22

### Added (Initial Release)
- ESP32-S3 support with WiFi and BLE
- SHT40 sensor driver (I2C, CRC validation)
- Multi-sensor support (up to 4 sensors)
- PWM fan control with fail-safe (SR-009)
- Anti-condensation protection (SR-010)
- MQTT integration with TLS 1.3
- Local OLED display support
- Web configuration interface (HTTPS)
- Secure OTA updates framework (Ed25519)
- Security framework (IEC 62443 SL-2)

### Security
- TLS 1.3 for MQTT and Web
- Certificate pinning
- Signed OTA updates
- Fail-safe fan control
- Authentication for control operations
- Input validation (SR-001)

---

## Security Advisory

### [SEC-2026-03-22-001] Security Framework Complete
**Severity**: Informational  
**Description**: IEC 62443 SL-2 security framework fully implemented.  
**Status**: ✅ Implemented

### Components
- SR-001: Input Validation (sensor_manager)
- SR-002: Authentication (security_utils)
- SR-003: Secure Communication (mqtt_client, web_server)
- SR-004: Fail-Safe Defaults (fan_control)
- SR-005: Audit Logging (audit_log)
- SR-006: Resource Limits (rate_limiter)
- SR-009: Actuator Fail-Safe (fan_control)
- SR-010: Environmental Limits (anti_condensation)
- SR-011: OTA Security (security_utils)

---

## References

- [Project Framework](PROJECT_FRAMEWORK.md)
- [Implementation Status](docs/IMPLEMENTATION_STATUS.md)
- [Build Instructions](BUILD.md)
