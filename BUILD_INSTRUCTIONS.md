# ThermoFlow Build Instructions

## Prerequisites

PlatformIO must be installed with ESP32 support. This will automatically download ESP-IDF.

## Method 1: Use PlatformIO (Recommended - No sudo needed)

This uses PlatformIO's bundled ESP-IDF:

```bash
cd ThermoFlow

# First, ensure ESP-IDF is downloaded via PlatformIO
pio run -e esp32-s3 --target buildfs

# Then use the build script
./build_with_pio_idf.sh
```

## Method 2: Manual Build with PlatformIO

```bash
cd ThermoFlow

# Install dependencies
pio lib install

# Build
pio run -e esp32-s3

# Upload to device
pio run -e esp32-s3 --target upload

# Monitor serial output
pio device monitor
```

## Method 3: Native ESP-IDF (Requires manual ESP-IDF installation)

If you have ESP-IDF installed separately:

```bash
cd ThermoFlow

# Export ESP-IDF environment
export IDF_PATH="$HOME/esp/esp-idf"
. $IDF_PATH/export.sh

# Build
idf.py set-target esp32s3
idf.py build
```

## Troubleshooting

### Missing sdkconfig
If you get errors about sdkconfig, copy the defaults:
```bash
cp sdkconfig.defaults sdkconfig
```

### Clean build
If builds fail, clean and retry:
```bash
rm -rf .pio/build
pio run -e esp32-s3
```

## Build Outputs

Successful build produces:
- `.pio/build/esp32-s3/firmware.bin` (or `build/esp32s3/thermoflow.bin` for native ESP-IDF)
- Partition table
- Bootloader