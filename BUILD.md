# ThermoFlow - ESP-IDF Build Instructions

## Prerequisites

ESP-IDF must be installed at `$HOME/esp-idf`.

### Installation
```bash
cd ~
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

## Quick Start

```bash
cd ThermoFlow

# Build only
./build.sh

# Build and flash
./build.sh && ./flash.sh /dev/ttyUSB0
```

## Project Components (12)

| Component | Purpose |
|-----------|---------|
| `sht4x_sensor` | Temperature/humidity sensing |
| `fan_control` | PWM fan control with fail-safe |
| `mqtt_client` | MQTT over TLS |
| `web_server` | HTTPS web interface + modern SPA |
| `security_utils` | Ed25519 signing and auth |
| `display_driver` | OLED display support |
| `anti_condensation` | Condensation protection |
| `sensor_manager` | Multi-sensor orchestration |
| `rate_limiter` | Token bucket rate limiting |
| `audit_log` | Security audit logging |
| `heat_recovery` | Mini-FTX calculations |
| `wifi_manager` | AP mode + WiFi configuration |

## Manual Build

If scripts don't work, use ESP-IDF directly:

```bash
export IDF_PATH="$HOME/esp-idf"
. $IDF_PATH/export.sh

cd ThermoFlow

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

## WiFi Configuration

### First Boot (AP Mode)
1. Enheten startar som `ThermoFlow-XXXX` (där XXXX är sista 4 hex av MAC)
2. Anslut till AP:n från din telefon/dator
3. Öppna http://192.168.4.1 i webbläsare
4. Ange ditt WiFi-nätverk och lösenord
5. Enheten startar om och ansluter till nätverket

### API Endpoints
| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/device/info` | GET | MAC, namn, version, IP |
| `/api/wifi/config` | POST | Spara WiFi-konfiguration |

### JSON Format (POST /api/wifi/config)
```json
{
  "ssid": "DittWiFiNamn",
  "password": "DittWiFiLosenord"
}
```

## Web Interface Features

- **Dashboard**: Realtidsöverblick med gauges och charts
- **Sensors**: Detaljerade sensorläsningar
- **FTX**: Värmeväxlare visualisering och kontroll
- **Settings**: Enhetsinfo och inställningar
- **PWA**: Kan installeras som app på telefon

### Keyboard Shortcuts
| Shortcut | Action |
|----------|--------|
| Ctrl+1 | Dashboard |
| Ctrl+2 | Sensors |
| Ctrl+3 | FTX |
| Ctrl+4 | Settings |
| Ctrl+R | Uppdatera data |

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

Pre-compiled binaries available in `binaries/` folder.

## Troubleshooting

### Missing sdkconfig
```bash
cp sdkconfig.defaults sdkconfig
```

### Clean build
```bash
idf.py fullclean
./build.sh
```

### Component not found
Check that all components have `CMakeLists.txt` with `idf_component_register()`.

### Permission denied on /dev/ttyUSB0
```bash
sudo usermod -a -G dialout $USER
# Log out and back in for changes to take effect
```

### WiFi Manager Issues
If device doesn't connect:
1. Hold reset button for 5 seconds to enter AP mode
2. Or use `/api/wifi/config` with DELETE method to reset

## Version Info

- **Target**: ESP32-S3
- **ESP-IDF**: v5.1.2
- **Current Version**: 1.5.0 (WiFi Manager + Modern Web GUI)
