# ThermoFlow - ESP-IDF Build Instructions

**Senast uppdaterad:** 2026-07-14  
**Firmware-version:** CalVer `YYYY.WW.BUILD` — se [docs/VERSIONING.md](docs/VERSIONING.md)

## Prerequisites

ESP-IDF v5.1.2+ (ESP32-S3 target).

### Installation (Linux/macOS)
```bash
cd ~
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

### Windows

ESP-IDF installerat lokalt, t.ex. `C:\Users\<user>\termoflow-test\esp-idf`.

## Quick Start

### Linux / macOS

```bash
cd ThermoFlow
./build.sh
./flash.sh /dev/ttyUSB0          # app-flash — NVS bevaras
```

### Windows

```powershell
powershell -ExecutionPolicy Bypass -File build-local.ps1
powershell -ExecutionPolicy Bypass -File flash-local.ps1 COM4
```

`build-local.ps1` genererar version via `scripts/generate_version.py` och bygger med `sdkconfig.ci.defaults`.

## Flash: app-flash vs full erase

| Kommando | NVS (WiFi, namn) | Användning |
|----------|------------------|------------|
| `app-flash` (standard) | **Bevaras** | Normal uppdatering |
| `erase-flash` + `flash` | Raderas | Fabriksåterställning |

```bash
# Standard — bevarar WiFi
./flash.sh /dev/ttyUSB0

# Full wipe
ERASE_FLASH=1 ./flash.sh /dev/ttyUSB0
```

Se [docs/WIFI_AND_FLASH.md](docs/WIFI_AND_FLASH.md) för onboarding och felsökning.

## Project Components

| Component | Purpose |
|-----------|---------|
| `wifi_manager` | WiFi STA/AP, encrypted credentials, AP+STA fallback |
| `web_server` | HTTP API + SPA (dashboard, logg, inställningar) |
| `sht4x_sensor` | Temperature/humidity sensing |
| `fan_control` | PWM fan control with fail-safe |
| `sensor_manager` | Multi-sensor orchestration |
| `hardware_manager` | Detection + simulation mode |
| `heat_recovery` | Mini-FTX calculations |
| `mqtt_client` | MQTT over TLS |
| `ota_manager` | Signed OTA updates |
| `security_utils` | Certificates, Ed25519 |
| `audit_log` | In-memory event log |
| `rate_limiter` | Token bucket rate limiting |
| `anti_condensation` | Condensation protection |
| `display_driver` | OLED display support |

## Manual Build

```bash
export IDF_PATH="$HOME/esp-idf"
. $IDF_PATH/export.sh

cd ThermoFlow
idf.py set-target esp32s3   # first time only
idf.py build
idf.py -p /dev/ttyUSB0 app-flash
idf.py -p /dev/ttyUSB0 monitor
```

## WiFi Configuration

### First boot (no saved credentials)

1. Enheten startar som `ThermoFlow-XXXX` (sista 4 hex av MAC)
2. Anslut till AP från telefon/dator
3. Öppna http://192.168.4.1
4. Ange WiFi SSID och lösenord
5. Enheten startar om och ansluter

### Efter firmware-uppdatering (app-flash)

WiFi-uppgifter ska finnas kvar. Om setup-AP syns kort:
- Vänta 1–2 minuter
- Öppna http://192.168.4.1 — ska visa **"Ansluter till WiFi"**, inte onboarding

### API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/device/info` | GET | Enhets-ID, WiFi-status, version |
| `/api/wifi/config` | POST | Spara WiFi |
| `/api/wifi/config` | DELETE | Återställ WiFi |
| `/api/logs` | GET/DELETE | Auditlogg |

```json
POST /api/wifi/config
{ "ssid": "DittWiFi", "password": "lösenord" }
```

## Web Interface

- **Dashboard** — gauges, charts, realtidsdata
- **Sensorer** — detaljerade läsningar
- **FTX** — värmeväxlare
- **Inställningar** — enhets-ID, visningsnamn, WiFi, version
- **Logg** — boot, WiFi, config, OTA-händelser
- **PWA** — kan installeras på telefon

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+1–4 | Navigera mellan vyer |
| Ctrl+R | Uppdatera data |

## Version Generation

```bash
python3 scripts/generate_version.py
```

Miljövariabler: `USE_BUILD_VERSION`, `BUILD_NUMBER`, `CHANNEL`, `REVISION`, `GIT_SHA`.  
CI sätter `BUILD_NUMBER` från `GITHUB_RUN_NUMBER`.

## Build Outputs

- `build/ThermoFlow.bin` — application
- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`

## Troubleshooting

### Clean build
```bash
idf.py fullclean
./build.sh
```

### Permission denied on serial port (Linux)
```bash
sudo usermod -a -G dialout $USER
```

### WiFi lost after flash
- Kontrollera att du använder **app-flash**, inte erase-flash
- Efter erase-flash: gör onboarding en gång; därefter app-flash bevarar uppgifter

### Component not found
Each component needs `CMakeLists.txt` with `idf_component_register()`.

## References

- [BUILD_ESP_IDF.md](BUILD_ESP_IDF.md) — detailed ESP-IDF guide
- [docs/WIFI_AND_FLASH.md](docs/WIFI_AND_FLASH.md) — WiFi persistence
- [docs/VERSIONING.md](docs/VERSIONING.md) — CalVer scheme