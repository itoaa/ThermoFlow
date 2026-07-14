# ThermoFlow - Pure ESP-IDF Build

**Senast uppdaterad:** 2026-07-14

Pure ESP-IDF project (no PlatformIO). Requires ESP-IDF v5.1.2+.

## Project Structure

```
ThermoFlow/
├── CMakeLists.txt
├── sdkconfig.defaults / sdkconfig.ci.defaults
├── partitions.csv
├── scripts/generate_version.py   # CalVer header generation
├── build-local.ps1 / flash-local.ps1   # Windows helpers
├── build.sh / flash.sh
├── main/
│   └── main.c
├── include/
│   └── thermoflow_version.h      # Auto-generated — do not edit
└── components/
    ├── wifi_manager/             # WiFi + encrypted NVS storage
    ├── web_server/               # HTTP API + SPA
    ├── sensor_manager/
    ├── fan_control/
    ├── hardware_manager/
    ├── heat_recovery/
    ├── mqtt_client/
    ├── ota_manager/
    ├── security_utils/
    ├── audit_log/
    └── ...
```

## Build Instructions

### 1. Install ESP-IDF

```bash
git clone -b v5.1.2 --recursive https://github.com/espressif/esp-idf.git
./esp-idf/install.sh esp32s3
. ./esp-idf/export.sh
```

### 2. Build

```bash
cd ThermoFlow
idf.py set-target esp32s3    # first time
idf.py build
```

Windows: `powershell -ExecutionPolicy Bypass -File build-local.ps1`

### 3. Flash

**Recommended — preserves NVS (WiFi, device name):**

```bash
idf.py -p PORT app-flash
```

**Full erase (factory reset):**

```bash
idf.py -p PORT erase-flash flash
```

Wrapper scripts default to app-flash. Set `ERASE_FLASH=1` for full wipe on Linux/macOS.

### 4. Monitor

```bash
idf.py -p PORT monitor
```

## Configuration

```bash
idf.py menuconfig
# or
cp sdkconfig.defaults sdkconfig
```

CI/local Windows builds use `sdkconfig.ci.defaults`.

## Versioning

Version header generated before build:

```bash
python3 scripts/generate_version.py
```

Format: `YYYY.WW.BUILD` (e.g. `2026.29.42`). See [docs/VERSIONING.md](docs/VERSIONING.md).

## Testing

```bash
idf.py test
idf.py test --filter test_wifi_encryption
```

## Troubleshooting

```bash
idf.py fullclean
idf.py set-target esp32s3
idf.py build
```

If `sdkconfig` conflicts: `rm sdkconfig` and rebuild.

## Related docs

- [BUILD.md](BUILD.md) — quick start and WiFi
- [docs/WIFI_AND_FLASH.md](docs/WIFI_AND_FLASH.md) — NVS and onboarding