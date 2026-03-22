# ThermoFlow

ESP32-S3 Climate Monitoring and Control System

## Overview

ThermoFlow monitors temperature and humidity for:
- Mobile AC units (cold and hot air)
- DIY heat exchangers with fan control

## Features

- **Multi-sensor**: Up to 4x SHT40 sensors (I2C)
- **Fan Control**: PWM control for 2 fans (heat exchanger mode)
- **Anti-condensation**: Automatic protection at >90% RH
- **MQTT**: Secure TLS connection to Home Assistant
- **Web Interface**: Modern responsive UI (HTTPS)
- **OTA Updates**: Signed firmware updates (Ed25519)
- **Security**: IEC 62443 SL-2 compliant

## Hardware

- **MCU**: ESP32-S3 (240MHz, WiFi + BLE)
- **Sensors**: Sensirion SHT40 (I2C, ±0.2°C, ±1.8% RH)
- **Display**: OLED 0.96" I2C (optional)
- **Fans**: 2x PWM controlled
- **Storage**: MicroSD for local logging
- **Power**: 5V USB

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

## Security

See [PROJECT_FRAMEWORK.md](PROJECT_FRAMEWORK.md) for security requirements and compliance.

## Status

✅ **Complete** - All core components implemented

See [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for detailed status.

## License

MIT License - See LICENSE file
