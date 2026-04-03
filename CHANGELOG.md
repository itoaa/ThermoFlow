# ThermoFlow Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.5.0] - 2026-04-03

### Added - WiFi Manager & Modern Web GUI 🌐

- **WiFi Manager Component** (`components/wifi_manager/`)
  - AP mode with MAC-based naming (`ThermoFlow-XXXX`)
  - Web-based WiFi configuration
  - Credentials saved to NVS (flash)
  - Automatic reconnection on boot
  - Fallback to AP mode if connection fails
  - `wifi_manager_get_status()`, `wifi_manager_configure()`, `wifi_manager_reset()`

- **Modern Web Interface** (`components/web_server/web/`)
  - Single Page Application (SPA) - no page reloads
  - **Chart.js integration** for temperature history
  - **Animated gauges** for real-time sensor values
  - **Dark/Light/Auto theme** with localStorage persistence
  - **PWA support**: Service worker, offline capability, manifest
  - **Toast notifications** for user feedback
  - **Keyboard shortcuts**: Ctrl+1-4 for views, Ctrl+R to refresh
  - **Glassmorphism design** with smooth animations
  - **Responsive layout** with mobile bottom navigation
  - New endpoints: `/api/wifi/config`, `/api/device/info`

- **Build Automation**
  - Git pre-commit hook for automatic binary copying
  - `binaries/` folder with latest compiled firmware

### Changed
- Updated `main.c` to integrate WiFi manager initialization
- Updated `web_server.c` with new WiFi config endpoints
- Modernized all CSS with CSS variables for theming
- Replaced static HTML with dynamic SPA architecture

### API Changes
- Added `POST /api/wifi/config` - Configure WiFi credentials
- Added `GET /api/device/info` - Device info (MAC, name, version)
- Added `tf_wifi_config_t` struct (avoids ESP-IDF naming conflict)

---

## [1.4.0] - 2026-04-03

### Added - Mini-FTX Extension 🏠
- **Heat Recovery Component** (`components/heat_recovery/`)
  - Värmeåtervinningsberäkningar (effektivitet, energibesparing)
  - Frostskydd med hysteresis (min 60s aktiveringstid)
  - Fläktstyrning med hysteresis (förhindrar fladder)
  - Luftflödesbalans-övervakning
  - Rate limiting för MQTT (max 1 publikation per 5-60s)
  - Sensorvalidering (NaN, infinity, rimliga värden)

- **MQTT FTX Integration** (`components/mqtt_client/mqtt_ftx.c`)
  - Publikation till `thermoflow/ftx/*` topics
  - Rate limiting per topic-typ
  - Alert-meddelanden (ej rate-limited)
  - Home Assistant discovery

- **REST API for FTX** (`components/web_server/web_server.c`)
  - `GET /api/ftx` - Komplett FTX data
  - `GET /api/ftx/sensors` - Sensorläsningar
  - `GET /api/ftx/efficiency` - Beräkningar
  - `GET /api/ftx/status` - Statusflaggor
  - `POST /api/ftx/control` - Kommandon

- **Web UI Dashboard** (`components/web_server/web/`)
  - `ftx.html` - FTX-vy med SVG gauge
  - `ftx-style.css` - Responsiv design
  - `ftx-script.js` - Live-data via MQTT/REST
  - Temperaturflödesdiagram (visuellt)
  - Fläktstyrning med sliders
  - Energidiagram (7 dagar)
  - Filterstatus med progress bar

- **Documentation**
  - `docs/FTX_EXTENSION.md` - Byggguide för Mini-FTX
  - `docs/MQTT_FTX_API.md` - MQTT API specifikation
  - Uppdaterad `README.md` med FTX-funktioner

- **Build Scripts**
  - `build.sh` - Förbättrad med komponentkontroll
  - `flash.sh` - Med port-detektering och erase-flash
  - `quick_build.sh` - Snabb inkrementell bygg

### Security Fixes (5 Critical)
1. **Frost Protection Actions** - Tidigare bara detektion, nu faktiska åtgärder
2. **Fan Speed Hysteresis** - Förhindrar fladder vid gränsvärden
3. **MQTT Rate Limiting** - Max 1 publikation per intervall
4. **Sensor Validation** - Kollar NaN, infinity, rimliga värden
5. **Airflow Balance Monitoring** - Detekterar obalans mellan tilluft/frånluft

### Technical
- Target: ESP32-S3
- ESP-IDF: v5.1.2
- Components: 12 (11 original + 1 ny heat_recovery + wifi_manager)

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

### [SEC-2026-04-03-001] Mini-FTX Security Hardening
**Severity**: Critical (resolved)
**Description**: 5 security weaknesses identified and fixed in Mini-FTX implementation.
**Status**: ✅ Fixed in v1.4.0

### [SEC-2026-04-03-002] WiFi Manager
**Severity**: Info
**Description**: New WiFi Manager component with AP mode and web configuration.
**Status**: ✅ Implemented in v1.5.0

### Components
- SR-001: Input Validation (sensor_manager) ✅
- SR-002: Authentication (security_utils) ✅
- SR-003: Secure Communication (mqtt_client, web_server) ✅
- SR-004: Fail-Safe Defaults (fan_control) ✅
- SR-005: Audit Logging (audit_log) ✅
- SR-006: Resource Limits (rate_limiter) ✅
- SR-009: Actuator Fail-Safe (fan_control) ✅
- SR-010: Environmental Limits (anti_condensation) ✅
- SR-011: OTA Security (security_utils) ✅

---

## References

- [Project Framework](PROJECT_FRAMEWORK.md)
- [Implementation Status](docs/IMPLEMENTATION_STATUS.md)
- [Build Instructions](BUILD.md)
- [Mini-FTX Extension](docs/FTX_EXTENSION.md)
- [Web GUI](components/web_server/web/)
