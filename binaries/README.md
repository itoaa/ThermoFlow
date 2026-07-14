# ThermoFlow Binaries

Denna mapp innehåller färdigbyggda binärer för ThermoFlow.

## Filstruktur

```
binaries/
├── ThermoFlow.bin          # Huvudapplikation (ESP32-S3)
├── bootloader.bin          # Bootloader
└── partition-table.bin     # Partitionstabell
```

## Flasha binärerna

### Krav
- ESP32-S3 utvecklingskort
- USB-kabel
- esptool.py (installeras med ESP-IDF)

### Kommandon

```bash
# Installera ESP-IDF först (om inte redan gjort)
export IDF_PATH="$HOME/esp-idf"
. $IDF_PATH/export.sh

# Flasha alla delar
esptool.py --chip esp32s3 \
    --port /dev/ttyUSB0 \
    --baud 460800 \
    --before default_reset \
    --after hard_reset \
    write_flash \
    -z \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 8MB \
    0x0 bootloader.bin \
    0x8000 partition-table.bin \
    0x10000 ThermoFlow.bin

# Uppdatera endast app (bevarar NVS/WiFi) — rekommenderat:
idf.py -p /dev/ttyUSB0 app-flash

# Full flash (raderar NVS):
idf.py -p /dev/ttyUSB0 erase-flash flash
```

Se [docs/WIFI_AND_FLASH.md](../docs/WIFI_AND_FLASH.md) för skillnaden mellan app-flash och erase-flash.

## Versioner

Binärerna i denna mapp kan vara äldre än källkoden. Aktuell version:

- **CalVer:** `YYYY.WW.BUILD` (t.ex. `2026.29.42`)
- **Källa:** `include/thermoflow_version.h` (genereras av `scripts/generate_version.py`)
- **Status:** [IMPLEMENTATION_STATUS.md](../docs/IMPLEMENTATION_STATUS.md)

## Bygga själv

För att bygga från källkod:

```bash
cd /path/to/ThermoFlow
./build.sh
```

Se [BUILD.md](../BUILD.md) för detaljerade instruktioner.

## License

Samma som projektet: MIT License
