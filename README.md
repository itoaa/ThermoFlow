# ThermoFlow - ESP32-S3 Climate Monitoring and Control System

**Firmware version:** 1.2.0 (definierad i `main/main.c`)  
**Repository:** https://github.com/itoaa/ThermoFlow

## Overview

ThermoFlow is an ESP32-S3 based system for monitoring temperature and humidity, with optional fan control. Target use cases:

- **Mobile AC units** — cold and hot air monitoring
- **DIY heat exchangers** — fan control based on conditions
- **Mini-FTX** — ventilation with heat recovery calculations

> **Important:** Many components exist as separate modules but are **not yet wired into `main.c`**. See [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for the honest status and [docs/TODO.md](docs/TODO.md) for everything still to do.

---

## What works today (firmware 1.2.0)

These features are **initialized and run** from `main.c`:

| Feature | Status |
|---------|--------|
| NVS / configuration storage | ✅ Runs |
| Hardware detection (I2C probe for SHT40/OLED) | ✅ Runs |
| Simulation mode (no hardware required) | ✅ Runs |
| WiFi manager (AP setup or connect to saved network) | ✅ Runs |
| Sensor manager | ⚠️ Runs, but returns **simulated** data |
| Fan controller | ⚠️ Runs, **software state only** (no PWM output) |
| Anti-condensation monitoring | ✅ Runs |
| OTA manager | ⚠️ Init only — no actual firmware download |

### Not started from `main.c` (code exists elsewhere)

| Feature | Status |
|---------|--------|
| Web interface (SPA/PWA) | ❌ Not started — see `components/web_server/` |
| MQTT / Home Assistant | ❌ Not started — see `components/mqtt_client/` |
| Mini-FTX / heat recovery | ❌ Not started — see `components/heat_recovery/` |
| OLED display | ❌ Stub only |
| Audit log, rate limiter | ❌ Not started |
| HTTPS | ⚠️ Falls back to HTTP if started manually |
| OTA download / rollback | ❌ Stub |
| MicroSD logging | ❌ Not implemented |

---

## Use Cases

| Mode | Description | Hardware | Firmware readiness |
|------|-------------|----------|-------------------|
| **AC Monitor** | Track mobile AC efficiency | 2–4 sensors | Simulation only |
| **Heat Exchanger** | Control DIY air-to-air HX | 2 fans + sensors | Partial (no PWM, no real sensors) |
| **Mini-FTX** | Ventilation with heat recovery | 4 sensors + 2 fans + web UI | Library only, not integrated |

---

## Features (by implementation state)

### Running in firmware
- **Hardware detection** — I2C probe for SHT40 and OLED at boot
- **Simulation mode** — runs without sensors for testing
- **WiFi Manager** — AP mode `ThermoFlow-XXXX` for initial setup
- **Anti-condensation** — RH threshold monitoring with hysteresis
- **Fan control logic** — temperature-based speed in software (no PWM yet)

### Implemented as components (not wired to main)
- **SHT40 driver** — full I2C driver with CRC (`components/sht4x_sensor/`)
- **Web server** — HTTP API + modern SPA (`components/web_server/web/`)
- **MQTT client** — TLS support (`components/mqtt_client/`)
- **Heat recovery** — FTX calculations, frost protection (`components/heat_recovery/`)
- **Audit log** — in-memory event log with checksums
- **Rate limiter** — token bucket per client
- **Security utils** — certificate management (Ed25519 is placeholder)

### Planned / documented but not complete
- PWM fan output via LEDC
- Signed OTA updates (Ed25519 stub)
- HTTPS web server (disabled for ESP-IDF v5.1.2 compatibility)
- MicroSD logging
- Full IEC 62443 SL-2 integration

---

## Hardware

- **MCU:** ESP32-S3 (240 MHz, WiFi + BLE)
- **Sensors:** Sensirion SHT40 (I2C) — driver exists, not yet used by sensor_manager
- **Display:** OLED 0.96" I2C (optional) — stub only
- **Fans:** 2× PWM (GPIO 10/11) — not yet driven in hardware
- **Power:** 5 V USB or 5 V/2 A adapter

### Pin Configuration

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA | GPIO 8 | SHT40 sensors, OLED |
| I2C SCL | GPIO 9 | SHT40 sensors, OLED |
| Fan 1 PWM | GPIO 10 | Not yet connected in code |
| Fan 2 PWM | GPIO 11 | Not yet connected in code |

---

## Quick Start

### Flash Pre-built Binary

```bash
cd ThermoFlow
./flash.sh /dev/ttyUSB0   # Replace with your port
```

After flashing:
1. Device starts in AP mode: `ThermoFlow-XXXX` (last 4 hex of MAC)
2. Connect to the AP
3. Configure WiFi (web UI requires `web_server` to be started — see [TODO.md](docs/TODO.md))

> **Note:** The pre-built binary matches `main.c` v1.2.0. The web interface is **not** started automatically in the current firmware.

### Build from Source

Requires ESP-IDF v5.1+ at `$HOME/esp-idf`.

```bash
cd ThermoFlow
./build.sh
./flash.sh /dev/ttyUSB0
```

See [BUILD.md](BUILD.md) and [BUILD_ESP_IDF.md](BUILD_ESP_IDF.md) for details.

---

## API Endpoints

Defined in `components/web_server/` — **available only when the web server is started** (not automatic in current `main.c`).

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/hardware` | GET | Hardware status and pin config |
| `/api/device/info` | GET | MAC, version, simulation mode |
| `/api/wifi/config` | POST | Save WiFi credentials |

Example response for hardware detection:

```json
{
  "simulation_mode": true,
  "detected": { "sensor_1": false, "fan_1": false },
  "pin_config": {
    "i2c": { "sda_gpio": 8, "scl_gpio": 9 },
    "fans": { "fan_1_gpio": 10, "fan_2_gpio": 11 }
  }
}
```

---

## Documentation

| Document | Purpose |
|----------|---------|
| [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) | Honest component and firmware status |
| [TODO.md](docs/TODO.md) | Unimplemented features and improvement plan |
| [BUILD.md](BUILD.md) | Build instructions |
| [BUILD_ESP_IDF.md](BUILD_ESP_IDF.md) | Detailed ESP-IDF guide |
| [FTX_EXTENSION.md](docs/FTX_EXTENSION.md) | Mini-FTX design (library, not integrated) |
| [MQTT_FTX_API.md](docs/MQTT_FTX_API.md) | MQTT API spec |
| [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) | Security requirements and governance |
| [CHANGELOG.md](CHANGELOG.md) | Version history |

---

## Security

ThermoFlow targets IEC 62443 SL-2. See [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md).

**Known issues (see [TODO.md](docs/TODO.md)):**
- Private signing keys must not be stored in the repository
- Ed25519 implementation is a placeholder
- WiFi credential encryption has stub code paths
- HTTPS is not active in the current web server build

---

## Project Status

| Area | Status |
|------|--------|
| Component architecture | ✅ Good structure |
| `main.c` integration | ⚠️ Partial — core loop only |
| Hardware I/O (sensors, PWM) | ⚠️ Detection yes, real I/O no |
| Web / MQTT / FTX | ❌ Not started from main |
| Security (production-ready) | ❌ Stubs and known issues |
| Unit tests | ⚠️ 3 suites in runner, more exist |

---

## License

MIT License — see [LICENSE](LICENSE)

## Author

Ola Andersson — https://github.com/itoaa/ThermoFlow