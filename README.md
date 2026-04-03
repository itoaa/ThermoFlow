# ThermoFlow

ESP32-S3 Climate Monitoring and Control System

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
| **Mini-FTX** | Full ventilation with heat recovery | 4 sensors + 2 fans |

## Features

### Core Features
- **Multi-sensor**: Up to 4x SHT40 sensors (I2C)
- **Fan Control**: PWM control for 2 fans
- **Anti-condensation**: Automatic protection at >90% RH
- **MQTT**: Secure TLS connection to Home Assistant
- **Web Interface**: Modern responsive UI (HTTPS)
- **OTA Updates**: Signed firmware updates (Ed25519)
- **Security**: IEC 62443 SL-2 compliant

### Mini-FTX Extension (NEW)
- **Heat Recovery**: Calculate efficiency (up to 95%)
- **Energy Tracking**: Monitor saved energy in kWh/day
- **Frost Protection**: Automatic detection and prevention
- **Filter Monitoring**: Track filter status
- **Smart Control**: Auto-adjust fan speed based on conditions

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

# Flash and monitor
./flash.sh /dev/ttyUSB0
```

See [BUILD.md](BUILD.md) for detailed instructions.

## Architecture

See `docs/architecture/` for system design documents.

## Documentation

- [BUILD.md](BUILD.md) - Build instructions
- [FTX_EXTENSION.md](docs/FTX_EXTENSION.md) - Mini-FTX guide
- [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) - Security framework
- [CHANGELOG.md](CHANGELOG.md) - Version history

## Security

See [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) for security requirements and compliance.

## Status

✅ **Complete** - All core components implemented
🆕 **FTX Extension** - Mini-FTX support added (v1.1.0)

See [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for detailed status.

## License

MIT License - See LICENSE file
