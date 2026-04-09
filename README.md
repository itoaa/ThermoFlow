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

### Hardware Detection & Simulation Mode (v1.1.0) 🔌
- **Auto-detect**: SHT40 sensors, OLED display, and PWM fans at boot
- **Simulation Mode**: Runs without hardware for testing/onboarding
- **"Bare" ESP32-S3**: Flash to unconfigured device, configure WiFi via web
- **Pin Config API**: Shows GPIO connections for missing hardware
- **Seamless Switch**: Connect sensors and reboot → automatic hardware mode

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

### Pin Configuration

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA | GPIO 8 | SHT40 sensors, OLED |
| I2C SCL | GPIO 9 | SHT40 sensors, OLED |
| Fan 1 PWM | GPIO 10 | PWM output |
| Fan 2 PWM | GPIO 11 | PWM output |

## Quick Start

### Flash Pre-built Binary (No Build Required)

```bash
# Flash pre-built binary to ESP32-S3
cd ThermoFlow
./flash.sh /dev/ttyUSB0  # Replace with your port

# Or use esptool directly
esptool.py -p /dev/ttyUSB0 -b 460800 write_flash 0x0 binaries/bootloader.bin 0x8000 binaries/partition-table.bin 0x10000 binaries/ThermoFlow.bin
```

After flashing:
1. Device starts in AP mode: `ThermoFlow-XXXX` (last 4 hex of MAC)
2. Connect to AP, open http://192.168.4.1
3. Configure WiFi credentials
4. Device restarts and connects to your network

### Build from Source

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

## API Endpoints

### Hardware Detection

```bash
# Get hardware status and pin config
GET /api/hardware

# Response example (simulation mode)
{
  "simulation_mode": true,
  "status": "SIMULATION - No hardware detected",
  "detected": {
    "sensor_1": false,
    "sensor_2": false,
    "sensor_3": false,
    "sensor_4": false,
    "display": false,
    "fan_1": false,
    "fan_2": false
  },
  "sensor_count": 0,
  "fan_count": 0,
  "pin_config": {
    "i2c": {"sda_gpio": 8, "scl_gpio": 9, "frequency_hz": 100000},
    "fans": {"fan_1_gpio": 10, "fan_2_gpio": 11, "pwm_freq_hz": 25000}
  },
  "instructions": [
    "SHT40 Sensors: Connect to GPIO 8 (SDA) and GPIO 9 (SCL), 3.3V, GND",
    "OLED Display: Connect to same I2C bus, address 0x3C or 0x3D",
    "Fan 1: Connect PWM to GPIO 10",
    "Fan 2: Connect PWM to GPIO 11"
  ]
}
```

### Device Info

```bash
GET /api/device/info

# Response
{
  "device_name": "ThermoFlow",
  "mac_address": "XX:XX:XX:XX:XX:XX",
  "firmware_version": "1.1.0",
  "platform": "ESP32-S3",
  "simulation_mode": true,
  "mode_description": "Running with simulated sensor data"
}
```

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
✅ **Hardware Detection** - Auto-detect + simulation mode (v1.1.0)  
✅ **Mini-FTX** - Full FTX support (v1.4.0)  
✅ **Modern Web GUI** - SPA with PWA (v1.5.0)

See [docs/IMPLEMENTATION_STATUS.md](docs/IMPLEMENTATION_STATUS.md) for detailed status.

## License

MIT License - See LICENSE file

## Author

Ola Andersson - https://github.com/itoaa/ThermoFlow
