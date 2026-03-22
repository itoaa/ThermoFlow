# ThermoFlow - ESP-IDF Build Instructions

## Prerequisites

ESP-IDF must be installed at `$HOME/esp-idf`.

If not installed:
```bash
cd ~
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
./esp-idf/install.sh esp32s3
```

## Quick Start

```bash
cd /home/ola/.openclaw/workspace/ThermoFlow

# Build only
./build.sh

# Build and flash
./build.sh && ./flash.sh /dev/ttyUSB0
```

## Manual Build

If scripts don't work, use ESP-IDF directly:

```bash
export IDF_PATH="$HOME/esp-idf"
. $IDF_PATH/export.sh

cd /home/ola/.openclaw/workspace/ThermoFlow

# First time only - set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash

# Monitor
idf.py -p /dev/ttyUSB0 monitor

# Or all at once
idf.py -p /dev/ttyUSB0 flash monitor
```

## Project Structure

- `main/` - Main application entry point
- `components/` - ESP-IDF components (10 total)
  - `sht4x_sensor/` - SHT40 temperature/humidity driver
  - `fan_control/` - PWM fan control with fail-safe
  - `mqtt_client/` - MQTT over TLS
  - `web_server/` - HTTPS web interface
  - `security_utils/` - Ed25519 signing and auth
  - `display_driver/` - OLED display support
  - `anti_condensation/` - Condensation protection
  - `sensor_manager/` - Multi-sensor orchestration
  - `rate_limiter/` - Token bucket rate limiting
  - `audit_log/` - Security audit logging
- `build.sh` - Build script
- `flash.sh` - Flash and monitor script
- `tests/` - Unity framework unit tests

## Configuration

Default configuration in `sdkconfig.defaults`. To customize:

```bash
idf.py menuconfig
```

Common settings:
- **WiFi**: Component config → WiFi
- **MQTT**: Component config → MQTT
- **Partition table**: Partition Table

## Build Outputs

After successful build:
- `build/ThermoFlow.bin` - Main application binary
- `build/bootloader/bootloader.bin` - Bootloader
- `build/partition_table/partition-table.bin` - Partition table

## Troubleshooting

### Missing sdkconfig
```bash
cp sdkconfig.defaults sdkconfig
```

### Clean build
```bash
idf.py fullclean
idf.py build
```

### Component not found
Check that all components have `CMakeLists.txt` with `idf_component_register()`.

### Permission denied on /dev/ttyUSB0
```bash
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```
