# ThermoFlow - ESP32-S3 Climate Monitoring and Control System

**Firmware version:** `2026.29.BUILD` (CalVer) — se [docs/VERSIONING.md](docs/VERSIONING.md)  
**Repository:** https://github.com/itoaa/ThermoFlow

## Overview

ThermoFlow is an ESP32-S3 based system for monitoring temperature and humidity, with fan control and optional heat-recovery (FTX) calculations. Target use cases:

- **Mobile AC units** — cold and hot air monitoring
- **DIY heat exchangers** — fan control based on conditions
- **Mini-FTX** — ventilation with heat recovery calculations

> Alla kärnkomponenter körs från `main.c`. HTTP-webbserver startar vid boot. HTTPS faller tillbaka till HTTP tills certifikat är konfigurerade. Se [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) och [docs/TODO.md](docs/TODO.md).

---

## What works today

| Feature | Status |
|---------|--------|
| Hardware detection + simulation mode | ✅ |
| SHT40 sensor reading (hardware mode) | ✅ |
| PWM fan control (LEDC GPIO 10/11) | ✅ |
| WiFi manager + encrypted credentials + NVS persistence | ✅ |
| AP+STA fallback (sparade uppgifter, ingen falsk onboarding) | ✅ |
| Enhets-ID från MAC + redigerbart visningsnamn | ✅ |
| HTTP web server + SPA (dashboard, FTX, inställningar, logg) | ✅ |
| Audit log i webben (`GET /api/logs`) | ✅ |
| MQTT / FTX (when broker configured) | ✅ |
| Heat recovery / FTX calculations | ✅ |
| Anti-condensation (per operating mode) | ✅ |
| OTA via esp_https_ota + Ed25519 | ✅ |
| CalVer versioning (`YYYY.WW.BUILD`) | ✅ |
| SSD1306 OLED (when detected) | ✅ |
| HTTPS | ⚠️ Falls back to HTTP |
| MicroSD logging | ❌ Not implemented |

---

## Quick Start

### Windows (lokal utveckling)

```powershell
powershell -ExecutionPolicy Bypass -File build-local.ps1
powershell -ExecutionPolicy Bypass -File flash-local.ps1 COM4
```

`flash-local.ps1` använder **app-flash** — WiFi och inställningar i NVS bevaras.

### Linux / macOS

```bash
cd ThermoFlow
./build.sh
./flash.sh /dev/ttyUSB0    # app-flash (NVS bevaras)
```

Full erase (raderar WiFi): `ERASE_FLASH=1 ./flash.sh /dev/ttyUSB0`

### Efter flash

1. Enheten försöker ansluta till sparat WiFi (om konfigurerat)
2. Om setup-AP **ThermoFlow-XXXX** syns: vänta 1–2 minuter — kan vara AP+STA-fallback
3. Första gången (ingen sparad WiFi): anslut till AP, öppna http://192.168.4.1, ange nätverk

Se [docs/WIFI_AND_FLASH.md](docs/WIFI_AND_FLASH.md) för fullständig guide.

---

## WiFi och onboarding

| Läge | Vad du ser |
|------|------------|
| Ingen sparad WiFi | Onboarding-formulär |
| Sparad WiFi, ansluter inte än | Sidan **"Ansluter till WiFi"** (inte onboarding) |
| Ansluten | Dashboard på hemmanätverkets IP |

Enhets-ID: `ThermoFlow-XXXX` (XXXX = sista 4 hex av MAC). Visningsnamn ändras under Inställningar.

---

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/device/info` | GET | Enhets-ID, namn, WiFi-status, version |
| `/api/wifi/config` | POST | Spara WiFi-uppgifter |
| `/api/wifi/config` | DELETE | Återställ WiFi |
| `/api/logs` | GET | Audit/systemlogg (senaste 50 händelser) |
| `/api/logs` | DELETE | Rensa logg |
| `/api/hardware` | GET | Hårdvarustatus och pin-konfiguration |
| `/api/sensors` | GET | Sensorvärden |
| `/api/fans` | GET/POST | Fläktstatus och styrning |

Exempel `GET /api/device/info`:

```json
{
  "device_id": "ThermoFlow-1440",
  "device_name": "ThermoFlow-1440",
  "wifi_credentials_saved": true,
  "wifi_ap_fallback": false,
  "wifi_saved_ssid": "S22",
  "wifi_state": "connected",
  "firmware_version": "2026.29.42",
  "version_full": "2026.29.42+040f567"
}
```

---

## Hardware

- **MCU:** ESP32-S3 (240 MHz, WiFi + BLE)
- **Sensors:** Sensirion SHT40 (I2C)
- **Display:** OLED 0.96" I2C (optional)
- **Fans:** 2× PWM (GPIO 10/11)
- **Power:** 5 V USB or 5 V/2 A adapter

| Function | GPIO |
|----------|------|
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |
| Fan 1 PWM | GPIO 10 |
| Fan 2 PWM | GPIO 11 |

---

## Documentation

| Document | Purpose |
|----------|---------|
| [WIFI_AND_FLASH.md](docs/WIFI_AND_FLASH.md) | WiFi, onboarding, app-flash, felsökning |
| [VERSIONING.md](docs/VERSIONING.md) | CalVer `YYYY.WW.BUILD` |
| [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) | Komponent- och firmwarestatus |
| [TODO.md](docs/TODO.md) | Kvarvarande arbete |
| [BUILD.md](BUILD.md) | Bygginstruktioner |
| [BUILD_ESP_IDF.md](BUILD_ESP_IDF.md) | Detaljerad ESP-IDF-guide |
| [WiFi_Encryption.md](docs/WiFi_Encryption.md) | SEC-021 krypterad lagring |
| [CHANGELOG.md](CHANGELOG.md) | Versionshistorik |
| [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) | Säkerhetskrav och styrning |

### Dokumentationsunderhåll

**Vid varje kodändring som påverkar beteende, API eller flash:** uppdatera relevant dokumentation i samma commit/PR. Minst:

- [CHANGELOG.md](CHANGELOG.md) — vad som ändrats
- Berörd guide (t.ex. [WIFI_AND_FLASH.md](docs/WIFI_AND_FLASH.md))
- [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) om komponentstatus ändras

---

## Security

ThermoFlow targets IEC 62443 SL-2. See [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md).

**Known issues (see [TODO.md](docs/TODO.md)):**
- Private signing keys must not be stored in the repository
- HTTPS is not active by default in the current web server build
- Git history may contain old exposed keys — manual cleanup required

---

## License

MIT License — see [LICENSE](LICENSE)

## Author

Ola Andersson — https://github.com/itoaa/ThermoFlow