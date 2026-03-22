# ThermoFlow - Pure ESP-IDF Build

## Overview

This project uses pure ESP-IDF (no PlatformIO). ESP-IDF v5.1+ required.

## Project Structure

```
ThermoFlow/
├── CMakeLists.txt              # Project CMakeLists
├── sdkconfig.defaults         # Default configuration
├── partitions.csv             # Flash partition table
├── main/
│   ├── CMakeLists.txt         # Main component
│   └── main.c                 # Application entry
├── components/                 # ESP-IDF components (10 total)
│   ├── sht4x_sensor/          # SHT40 I2C driver
│   ├── fan_control/             # PWM fan control
│   ├── mqtt_client/            # MQTT over TLS
│   ├── web_server/             # HTTPS web UI
│   ├── security_utils/         # Ed25519 + auth
│   ├── display_driver/         # OLED display
│   ├── anti_condensation/      # RH protection
│   ├── sensor_manager/         # Multi-sensor hub
│   ├── rate_limiter/           # Rate limiting
│   └── audit_log/              # Audit logging
└── tests/                      # Unity tests
```

## Build Instructions

### Step 1: Install ESP-IDF

```bash
cd ~
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
./esp-idf/install.sh esp32s3
```

### Step 2: Export Environment

```bash
export IDF_PATH="$HOME/esp-idf"
. $IDF_PATH/export.sh
```

Add to `~/.bashrc` for persistence:
```bash
echo 'export IDF_PATH="$HOME/esp-idf"' >> ~/.bashrc
echo '. $IDF_PATH/export.sh' >> ~/.bashrc
```

### Step 3: Build Project

```bash
cd /home/ola/.openclaw/workspace/ThermoFlow

# Set target (first time only)
idf.py set-target esp32s3

# Build
idf.py build
```

### Step 4: Flash and Monitor

```bash
# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor

# Or both at once
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

```bash
# Interactive configuration
idf.py menuconfig

# Or use defaults:
cp sdkconfig.defaults sdkconfig
```

## Build Outputs

After successful build:
- `build/ThermoFlow.bin` - Main application (221 KB)
- `build/bootloader/bootloader.bin` - Bootloader (21 KB)
- `build/partition_table/partition-table.bin` - Partition table

## Testing

```bash
# Run unit tests
idf.py test

# Or specific test
idf.py test --filter test_sht4x
```

## Troubleshooting

### Full clean
```bash
idf.py fullclean
idf.py set-target esp32s3
idf.py build
```

### sdkconfig conflicts
```bash
rm sdkconfig
idf.py set-target esp32s3
idf.py build
```

### Component not found
Ensure each component has `CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "component.c"
    INCLUDE_DIRS "include"
    REQUIRES required_component
)
```

## IDE Support

### VS Code
Install "Espressif IDF" extension. Open project folder, extension auto-detects ESP-IDF structure.

### CLion
Set CMake options:
```
-DIDF_PATH=$HOME/esp-idf
-DCMAKE_TOOLCHAIN_FILE=$HOME/esp-idf/tools/cmake/toolchain-esp32s3.cmake
```
