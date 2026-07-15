# ThermoFlow Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Note:** Firmware version uses CalVer `YYYY.WW.BUILD` (see [docs/VERSIONING.md](docs/VERSIONING.md)).  
> Generated in `include/thermoflow_version.h` by `scripts/generate_version.py`.

---

## [Unreleased]

### Fixed
- HTTP server failed to start after application profiles: URI handler count exceeded `max_uri_handlers` (32) — raised to 48
- Stack overflow in `sys_evt` when logging from WiFi event handler — sinks now dispatch async
- Reduced serial log noise (default serial level WARN); NVS persist throttled to WARN+ / audit

### Added
- **Optional PSRAM** with stability-first policy (capability allocator only, not default `malloc`)
- `tf_mem` helpers: prefer PSRAM for bulk non-DMA buffers with internal fallback
- Live heap/PSRAM stats in `GET /api/device/info` and Inställningar UI
- Larger log ring (400 entries) when PSRAM is available; 100 without
- Docs: [PSRAM.md](docs/PSRAM.md)
- **log_manager** — unified multi-sink logging (serial, web, NVS, MQTT, SD stub)
- Structured JSON v1 log entries with `boot_id`, `correlation_id`, category, component
- `GET/PUT /api/logs/config`, `GET /api/logs/export` (NDJSON/JSON)
- Web Logg UI: category/source columns, export, runtime sink configuration
- NVS log persistence (32 entries across reboot)
- MQTT remote log sink (`thermoflow/logs`) when broker connected
- `TF_LOG_*` macros and [docs/LOGGING.md](docs/LOGGING.md)
- CalVer versioning scheme `YYYY.WW.BUILD` with CI build number and local `+gitsha` suffix
- `build-local.ps1` / `flash-local.ps1` for Windows development
- Web UI **Logg** tab with `GET/DELETE /api/logs` (audit log viewer)
- `wifi_reconnect.html` — wait page during AP+STA fallback (saved credentials)
- API fields: `wifi_ap_fallback`, `wifi_saved_ssid`, `device_id`, `has_custom_name`
- `wifi_manager_is_ap_fallback_mode()`, `wifi_manager_get_saved_ssid()`
- Docs: [WIFI_AND_FLASH.md](docs/WIFI_AND_FLASH.md), [VERSIONING.md](docs/VERSIONING.md), [PSRAM.md](docs/PSRAM.md)

### Changed
- IRAM headroom: disable WiFi IRAM opts; place FreeRTOS/ringbuf helpers in flash (`sdkconfig.ci.defaults`)
- `audit_log` refactored as SR-005 facade over `log_manager`
- Ring buffer capacity: 100 default; 400 when optional PSRAM is available
- Implemented `audit_log_export_json`, `audit_log_verify`, `audit_log_set_filter`
- **Flash default is app-flash** (`flash.sh`, `flash-local.ps1`) — NVS/WiFi preserved
- WiFi credentials dual-written to encrypted + legacy NVS; migration keeps legacy backup
- AP+STA fallback: 15 retries, 90 s timeout; credentials never cleared on timeout
- Root web page (`/`): onboarding only when no saved credentials; reconnect page in fallback
- Device ID derived from factory MAC (`esp_read_mac`) before WiFi init — immutable `ThermoFlow-XXXX`
- Separate editable display name (NVS `device_config`) from device ID
- `wifi_init_nvs()` no longer erases NVS partition on second init

### Fixed
- WiFi SSID lost after flash when using `erase-flash` — documented; app-flash is default
- False onboarding after flash when device was in AP+STA fallback with saved credentials
- Stale device names mimicking old MAC-based ID format auto-cleared from NVS

### Documentation
- README, BUILD.md, IMPLEMENTATION_STATUS, TODO, WiFi_Encryption synced to current firmware
- Documentation maintenance policy added to README

---

## [1.3.0] - 2026-07-12

### Added
- Full `main.c` integration of web_server, MQTT, FTX, audit_log, rate_limiter, display
- SHT40 hardware reading in sensor_manager
- LEDC PWM fan output
- WiFi credential AES-256-CBC encryption
- Ed25519 signatures via Monocypher
- OTA via esp_https_ota
- SR-010 operating modes (AC / heat exchanger / FTX)
- SSD1306 display driver
- heat_recovery Unity tests, GitHub Actions CI
- SBOM and threat model documents

### Security
- Removed private signing key from repository
- Extended `.gitignore` for secrets

### Removed
- PlatformIO `library.json` files
- Duplicate stub headers in `include/`

---

## [2.0.0] - 2026-04-12

### Added - HTTPS Web Server Security Enhancement (SEC-018) 🔒

- **HTTPS Server Implementation** (`components/web_server/`)
  - TLS 1.3 with fallback to TLS 1.2
  - Secure cipher suites only (AEAD: AES-GCM, ChaCha20-Poly1305)
  - HTTP-to-HTTPS redirect on port 80 → 443
  - Certificate management integration with `security_utils`
  - Auto-provisioning from EJBCA-PKI
  - Certificate renewal 30 days before expiry
  - `web_server_start_https()`, `web_server_restart_https()`
  - `web_server_provision_certificate()`, `web_server_check_cert_renewal()`

- **Security Headers** (all HTTPS responses)
  - `Strict-Transport-Security` (HSTS) with configurable max-age
  - `Content-Security-Policy` (CSP) - XSS protection
  - `X-Frame-Options: DENY` - Clickjacking prevention
  - `X-Content-Type-Options: nosniff` - MIME sniffing protection
  - `X-XSS-Protection: 1; mode=block`
  - `Referrer-Policy: strict-origin-when-cross-origin`
  - `Permissions-Policy` - Feature restrictions

- **Certificate Management API**
  - `GET /api/cert/status` - Certificate info and expiry
  - Returns: has_server_cert, has_server_key, days_until_expiry, fingerprint
  - Integration with existing `security_manager` component

- **Configuration via menuconfig**
  - `WEB_SERVER_HTTPS_ENABLED` - Enable/disable HTTPS
  - `WEB_SERVER_HTTPS_PORT` - HTTPS port (default 443)
  - `WEB_SERVER_HTTP_REDIRECT` - HTTP → HTTPS redirect
  - `WEB_SERVER_TLS_VERSION` - TLS 1.2/1.3 selection
  - `WEB_SERVER_ENABLE_MTLS` - Mutual TLS authentication
  - `WEB_SERVER_ENABLE_HSTS` - HTTP Strict Transport Security
  - `WEB_SERVER_ENABLE_CSP` - Content Security Policy
  - `WEB_SERVER_AUTO_PROVISION` - Auto-request certs from EJBCA
  - `WEB_SERVER_EJBCA_URL` - EJBCA server URL

- **New API Endpoints**
  - `GET /api/cert/status` - Certificate status and health
  - All existing endpoints now include `https_enabled` flag

- **Documentation**
  - `docs/HTTPS-SETUP.md` - Complete setup and configuration guide
  - Security best practices and troubleshooting
  - SSL Labs-style testing instructions

### Security Fixes (1 Critical)

1. **SEC-018: HTTPS Web Server Implementation** (CVSS 7.5 → Fixed)
   - **Finding THF-003**: Web server operated over unencrypted HTTP only
   - **Risk**: Credential theft, session hijacking, configuration tampering
   - **Fix**: Full HTTPS/TLS 1.3 implementation with security headers

### Technical Details
- Uses `esp_https_server` component from ESP-IDF
- Certificate storage: Encrypted NVS partition
- Key algorithm: ECDSA secp256r1
- Cipher suites: Only AEAD ciphers (no CBC, no RC4, no SHA1)
- Memory overhead: ~8KB additional RAM for TLS
- Fallback: Can start HTTP-only if certificates unavailable

### API Changes
- Added `https_config_t` configuration structure
- Added `web_server_cert_info_t` certificate info structure
- New functions: `web_server_set_https_config()`, `web_server_get_https_config()`
- New functions: `web_server_get_cert_info()`, `web_server_delete_certificates()`
- New functions: `web_server_is_https_running()`, `web_server_get_status()`
- All responses now include security headers when HTTPS enabled

---

## [1.6.0] - 2026-04-09

### Added - Hardware Detection & Simulation Mode 🔌

- **Hardware Manager Component** (`components/hardware_manager/`)
  - Automatic hardware detection at boot (I2C scan, GPIO check)
  - Detects SHT40 sensors (up to 4 units), OLED display, PWM fans
  - Auto-fallback to simulation mode if no hardware detected
  - Runtime re-detection for hot-plug support
  - Thread-safe mutex protection for hardware state
  - `hardware_manager_init()`, `hardware_is_simulation_mode()`, `hardware_get_summary()`
  - `hardware_is_detected()`, `hardware_redetect()`, `hardware_get_sensor_count()`

- **Sensor Manager Simulation** (`components/sensor_manager/`)
  - Generates realistic simulated sensor data (temp 15-25°C, RH 30-80%)
  - Small random variation using hardware RNG for realistic demo
  - Automatic mode switching based on hardware detection
  - Validated per IEC 62443 SR-001 (input validation)

- **Hardware API Endpoints**
  - `GET /api/hardware` - Full hardware status and pin configuration
  - Returns: simulation mode, detected components, sensor/fan counts, pin mapping
  - Includes instructions for connecting missing hardware
  - JSON response with i2c_sda/scl_gpio, fan_1/2_gpio

- **"Bare" ESP32-S3 Operation**
  - Flash to unconfigured ESP32-S3 without any connected hardware
  - Device starts AP mode (`ThermoFlow-XXXX`), serves web config
  - Simulated sensor data for testing and onboarding
  - Connect hardware later → reboot → automatic hardware mode

### Added - Web Server Updates (v1.6.0)
  - `GET /api/hardware` endpoint for hardware status
  - Simulation mode flag in all API responses
  - Pin configuration exposed in JSON API
  - Simplified device info (no hard WiFi manager dependency)

### Changed
- Updated `main.c` with hardware detection initialization
- Updated `sensor_manager.c` for simulation mode support
- Web server returns simulation status in all endpoints
- Removed wifi_manager dependency from web_server.c (avoid circular deps)

### API Changes
- Added `GET /api/hardware` - Hardware detection and pin config
- All endpoints now include `simulation_mode` boolean
- `/api/ftx/sensors` includes `pin_config` object

### Technical Details
- I2C probing at 100kHz with 100ms timeout
- SHT40 addresses scanned: 0x44, 0x45, 0x46, 0x47
- OLED detection at 0x3C, 0x3D
- Simulation uses esp_random() for realistic variation
- No impact on flash usage (~2KB additional code)

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

### [SEC-2026-04-12-001] HTTPS Web Server Implementation
**Severity**: High (CVSS 7.5) ✅ RESOLVED
**Description**: Web server operated over unencrypted HTTP only (Finding THF-003)
**Risk**: Credential theft, session hijacking, configuration tampering
**Fix**: Full HTTPS/TLS 1.3 implementation with security headers
**Status**: ✅ Fixed in v2.0.0 (SEC-018)

### [SEC-2026-04-09-002] Hardware Detection & Simulation Mode
**Severity**: Info
**Description**: Hardware auto-detection with simulation fallback for "bare" ESP32 operation.
**Status**: ✅ Implemented in v1.6.0

### [SEC-2026-04-03-001] Mini-FTX Security Hardening
**Severity**: Critical (resolved)
**Description**: 5 security weaknesses identified and fixed in Mini-FTX implementation.
**Status**: ✅ Fixed in v1.4.0

### [SEC-2026-04-03-002] WiFi Manager
**Severity**: Info
**Description**: New WiFi Manager component with AP mode and web configuration.
**Status**: ✅ Implemented in v1.5.0

### [SEC-2026-04-09-001] Hardware Detection & Simulation Mode
**Severity**: Info
**Description**: Hardware auto-detection with simulation fallback for "bare" ESP32 operation.
**Status**: ✅ Implemented in v1.6.0

### Security Components
- SR-001: Input Validation (sensor_manager) ✅
- SR-002: Authentication (security_utils) ✅
- SR-003: Secure Communication (mqtt_client, web_server) ✅
- SR-004: Fail-Safe Defaults (fan_control) ✅
- SR-005: Audit Logging (audit_log) ✅
- SR-006: Resource Limits (rate_limiter) ✅
- SR-009: Actuator Fail-Safe (fan_control) ✅
- SR-010: Environmental Limits (anti_condensation) ✅
- SR-011: OTA Security (security_utils) ✅
- SEC-018: HTTPS Web Server (web_server) ✅

---

## References

- [Project Framework](PROJECT_FRAMEWORK.md)
- [Implementation Status](docs/IMPLEMENTATION_STATUS.md)
- [Build Instructions](BUILD.md)
- [Mini-FTX Extension](docs/FTX_EXTENSION.md)
- [HTTPS Setup Guide](docs/HTTPS-SETUP.md)
- [Web GUI](components/web_server/web/)
