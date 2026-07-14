# ThermoFlow — TODO och förbättringsförslag

**Senast uppdaterad:** 2026-07-14  
**Firmware-version:** CalVer `YYYY.WW.BUILD` — se [VERSIONING.md](VERSIONING.md)

## Klart (2026-07-14)

- [x] Unified `log_manager` (serial, web, NVS, MQTT sink, SD stub)
- [x] Structured logging API (`TF_LOG_*`), export NDJSON, web config
- [x] [LOGGING.md](LOGGING.md) architecture documentation
- [x] CalVer versioning `YYYY.WW.BUILD` + lokal `+gitsha`
- [x] WiFi NVS-persistens (dual-write, legacy backup, app-flash default)
- [x] AP+STA fallback utan att radera credentials
- [x] Reconnect-sida — ingen falsk onboarding vid fallback
- [x] Enhets-ID från MAC + separat visningsnamn
- [x] Auditlogg i webben (`/api/logs`, Logg-flik)
- [x] `build-local.ps1` / `flash-local.ps1` (Windows)
- [x] Dokumentation synkad: README, BUILD, WIFI_AND_FLASH, VERSIONING, CHANGELOG

## Klart (2026-07-12)

- [x] `main.c` integrerar alla kärnkomponenter
- [x] SHT40-läsning, LEDC PWM, WiFi AES-256-CBC
- [x] Ed25519 via Monocypher, OTA via `esp_https_ota`
- [x] SR-010 driftlägen, SSD1306, rate limiter, CI, SBOM

## Kvarstående

### Säkerhet
- [ ] Rensa exponerad privat nyckel ur **git-historiken** (`git filter-repo`)
- [ ] Rotera secure boot-nyckel om den publicerats
- [ ] Aktivera HTTPS i produktion (HTTP används i dev-läge)
- [ ] Fullständig certificate pinning för MQTT i drift

### Hårdvara / test
- [ ] HIL-test på ESP32-S3 + SHT40 + PWM-fläktar
- [ ] Förbättra OLED-font
- [ ] MicroSD-loggning — ej implementerat

### MQTT / konfiguration
- [ ] Sätt `MQTT_BROKER_DEFAULT` och `OTA_SERVER_URL` via menuconfig/NVS
- [ ] Ladda OTA Ed25519 publik nyckel från NVS vid boot

### Dokumentation / release
- [ ] Bygg och uppdatera `binaries/` för aktuell CalVer
- [ ] Verifiera CI-build mot `espressif/idf:release-v5.1`

## Dokumentationspolicy

**Vid varje beteendeändring, nytt API eller flash-relaterad ändring:**

1. Uppdatera [CHANGELOG.md](../CHANGELOG.md) under `[Unreleased]`
2. Uppdatera berörd guide (t.ex. [WIFI_AND_FLASH.md](WIFI_AND_FLASH.md))
3. Uppdatera [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) om komponentstatus ändras
4. Uppdatera [README.md](../README.md) om användarflöde eller quick start påverkas

## Driftlägen (SR-010)

| Läge | Konfiguration | Beteende vid >90 % RH |
|------|---------------|------------------------|
| AC Monitor | `TF_MODE_AC_MONITOR` | Larm, fläktar oförändrade |
| Värmeväxlare | `TF_MODE_HEAT_EXCHANGER` (default) | Fläktar AV |
| Mini-FTX | `TF_MODE_MINI_FTX` | Max ventilation |

Ändra via `THERMOFLOW_DEFAULT_MODE` i `thermoflow_config.h`.