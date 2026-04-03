# ThermoFlow - ESP32-S3 Climate Monitoring and Control System

## Overview

ThermoFlow monitors temperature and humidity for:
- **Mobile AC units** (cold and hot air monitoring)
- **DIY heat exchangers** with fan control
- **Mini-FTX systems** (Frånluftsventilation med värmeåtervinning) ⭐ NEW!

## Use Cases

| Mode | Description | Hardware |
|------|-------------|----------|
| **AC Monitor** | Track mobile AC efficiency | 2-4 sensors |
| **Heat Exchanger** | Control DIY air-to-air HX | 2 fans + sensors |
| **Mini-FTX** | Full ventilation with heat recovery | 4 sensors + 2 fans + web UI |

## Features

### Core Features
- **Multi-sensor**: Up to 4x SHT40 sensors (I2C)
- **Fan Control**: PWM control for 2 fans
- **Anti-condensation**: Automatic protection at >90% RH
- **MQTT**: Secure TLS connection to Home Assistant
- **Web Interface**: Modern responsive UI (HTTPS)
- **OTA Updates**: Signed firmware updates (Ed25519)
- **Security**: IEC 62443 SL-2 compliant

### Mini-FTX Extension (v1.4.0) ⭐
- **Heat Recovery**: Calculate efficiency (up to 95%)
- **Energy Tracking**: Monitor saved energy in kWh/day
- **Frost Protection**: Automatic detection and prevention with hysteresis
- **Filter Monitoring**: Track filter status with pressure drop
- **Smart Control**: Auto-adjust fan speed based on conditions
- **Airflow Balance**: Monitor supply/exhaust balance
- **Web Dashboard**: Real-time FTX visualization

See [FTX_EXTENSION.md](docs/FTX_EXTENSION.md) for detailed FTX documentation.

## Hardware

- **MCU**: ESP32-S3 (240MHz, WiFi + BLE)
- **Sensors**: Sensirion SHT40 (I2C, ±0.2°C, ±1.8% RH)
- **Display**: OLED 0.96" I2C (optional)
- **Fans**: 2x PWM controlled (3-pin or 4-pin)
- **Storage**: MicroSD for local logging
- **Power**: 5V USB or 5V/2A adapter

## Quick Start

Requires ESP-IDF installed at `$HOME/esp-idf`.

```bash
cd ThermoFlow

# Build
./build.sh

# Or with clean
./build.sh clean

# Flash and monitor
./flash.sh /dev/ttyUSB0

# Quick build (incremental, faster)
./quick_build.sh
```

See [BUILD.md](BUILD.md) and [BUILD_ESP_IDF.md](BUILD_ESP_IDF.md) for detailed instructions.

## Documentation

- [BUILD.md](BUILD.md) - Build instructions
- [BUILD_ESP_IDF.md](BUILD_ESP_IDF.md) - Detailed ESP-IDF guide
- [FTX_EXTENSION.md](docs/FTX_EXTENSION.md) - Mini-FTX build guide
- [MQTT_FTX_API.md](docs/MQTT_FTX_API.md) - MQTT API for FTX
- [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) - Security framework
- [IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) - Component status
- [CHANGELOG.md](CHANGELOG.md) - Version history

## Security

See [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) for security requirements and compliance.

## Status

✅ **Complete** - All core components implemented  
✅ **Mini-FTX** - Full FTX support with heat recovery (v1.4.0)  

See [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for detailed status.

## License

MIT License - See LICENSE file

## Author

Ola Andersson - https://github.com/itoaa/ThermoFlow
