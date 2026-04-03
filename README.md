# ThermoFlow - ESP32-S3 Climate Monitoring and Control System

## Overview

ThermoFlow monitors temperature and humidity for:
- **Mobile AC units** (cold and hot air monitoring)
- **DIY heat exchangers** with fan control
- **Mini-FTX systems** (Frånluftsventilation med värmeåtervinning)

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
- **Web Interface**: Modern SPA with charts (HTTPS)
- **OTA Updates**: Signed firmware updates (Ed25519)
- **Security**: IEC 62443 SL-2 compliant
- **WiFi Manager**: AP mode for easy configuration

### Mini-FTX Extension (v1.5.0)
- **Heat Recovery**: Calculate efficiency (up to 95%)
- **Energy Tracking**: Monitor saved energy in kWh/day
- **Frost Protection**: Automatic detection and prevention
- **Filter Monitoring**: Track filter status
- **Smart Control**: Auto-adjust fan speed based on conditions
- **Airflow Balance**: Monitor supply/exhaust balance

### Web GUI (v1.5.0) 🎨
- **Single Page Application** - No page reloads
- **Real-time Charts** - Chart.js for temperature history
- **Animated Gauges** - Visual temperature/humidity displays
- **Dark/Light Theme** - Auto theme switching
- **PWA Support** - Install as app, offline capable
- **Toast Notifications** - Non-intrusive feedback
- **Keyboard Shortcuts** - Ctrl+1-4 for views

See [docs/FTX_EXTENSION.md](docs/FTX_EXTENSION.md) for FTX documentation.

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

See [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) for security requirements.

## Status

✅ **Complete** - All core components implemented  
✅ **Mini-FTX** - Full FTX support (v1.4.0)  
✅ **Modern Web GUI** - SPA with PWA (v1.5.0)

See [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for detailed status.

## License

MIT License - See LICENSE file

## Author

Ola Andersson - https://github.com/itoaa/ThermoFlow
