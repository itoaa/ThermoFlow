# ThermoFlow - ESP-IDF Build Instructions

## Quick Start

```bash
cd /home/ola/.openclaw/workspace/ThermoFlow

# Build only
./build.sh

# Build and flash
./build.sh && ./flash.sh
```

## Manual Build (if scripts don't work)

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
```

## Project Structure

- `main/` - Main application
- `components/` - ESP-IDF components (10 total)
- `build.sh` - Build script
- `flash.sh` - Flash and monitor script

## ESP-IDF Location

Expected at: `$HOME/esp-idf`

If installed elsewhere, edit `build.sh` and `flash.sh` to change `IDF_PATH`