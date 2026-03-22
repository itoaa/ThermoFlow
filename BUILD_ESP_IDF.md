# ThermoFlow - Pure ESP-IDF Build

## Project Structure

```
ThermoFlow/
├── CMakeLists.txt              # Project CMakeLists
├── sdkconfig.defaults         # Default configuration
├── main/
│   ├── CMakeLists.txt         # Main component
│   └── main.c                 # Application entry
└── components/                 # ESP-IDF components
    ├── sht4x_sensor/
    ├── fan_control/
    ├── mqtt_client/
    ├── web_server/
    ├── security_utils/
    ├── display_driver/
    ├── anti_condensation/
    ├── sensor_manager/
    ├── rate_limiter/
    └── audit_log/
```

## Build Instructions

### Step 1: Export ESP-IDF Environment

```bash
# Adjust path if ESP-IDF is installed elsewhere
export IDF_PATH="$HOME/esp/esp-idf"
. $IDF_PATH/export.sh
```

### Step 2: Set Target

```bash
cd /home/ola/.openclaw/workspace/ThermoFlow
idf.py set-target esp32s3
```

### Step 3: Configure (Optional)

```bash
idf.py menuconfig
# Or use defaults:
cp sdkconfig.defaults sdkconfig
```

### Step 4: Build

```bash
idf.py build
```

### Step 5: Flash and Monitor

```bash
# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor

# Or both at once
idf.py -p /dev/ttyUSB0 flash monitor
```

## Build Outputs

After successful build:
- `build/thermoflow.bin` - Main application binary
- `build/bootloader/bootloader.bin` - Bootloader
- `build/partition_table/partition-table.bin` - Partition table

## Troubleshooting

### Missing dependencies
```bash
idf.py fullclean
idf.py build
```

### sdkconfig conflicts
```bash
rm sdkconfig
idf.py set-target esp32s3
idf.py build
```

### Component not found
Check that all components have `CMakeLists.txt` with `idf_component_register()`