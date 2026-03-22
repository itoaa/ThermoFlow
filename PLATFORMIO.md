# ThermoFlow PlatformIO Project

## Structure

```
ThermoFlow/
├── lib/                    # Component libraries
│   ├── sht4x_sensor/
│   ├── fan_control/
│   ├── mqtt_client/
│   ├── web_server/
│   ├── security_utils/
│   ├── display_driver/
│   ├── anti_condensation/
│   ├── sensor_manager/
│   ├── rate_limiter/
│   └── audit_log/
├── src/                    # Main application
│   └── main.cpp
├── include/                # Global headers
├── tests/                  # Unit tests
├── platformio.ini          # PlatformIO configuration
└── partitions.csv          # Flash partition table
```

## Build Commands

```bash
# Build for ESP32-S3
pio run -e esp32-s3

# Build release version
pio run -e esp32-s3-release

# Build debug version
pio run -e esp32-s3-debug

# Upload to device
pio run -e esp32-s3 --target upload

# Monitor serial output
pio device monitor

# Run unit tests
pio test -e native-test
```

## Components

All components are in `lib/` directory with `library.json` metadata files.

## Dependencies

- ArduinoJson (for JSON handling in MQTT and Web Server)