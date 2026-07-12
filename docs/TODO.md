# ThermoFlow — TODO och förbättringsförslag

**Senast uppdaterad:** 2026-07-12  
**Firmware-version:** 1.3.0 (`include/thermoflow_version.h`)

## Klart (2026-07-12)

- [x] Dokumentation synkad med kod
- [x] Privat signeringsnyckel borttagen + `.gitignore`
- [x] `thermoflow_version.h` central version
- [x] `main.c` integrerar alla kärnkomponenter
- [x] SHT40-läsning i `sensor_manager`
- [x] LEDC PWM för fläktar
- [x] WiFi AES-256-CBC credential-kryptering
- [x] Ed25519 via Monocypher
- [x] OTA via `esp_https_ota`
- [x] SR-010 per driftläge (`thermoflow_mode_t`)
- [x] SSD1306 display (grundläggande)
- [x] Rate limiter i web_server
- [x] Unity-test för heat_recovery
- [x] GitHub Actions CI
- [x] SBOM + threat model
- [x] Städning: `library.json`, duplicerade `include/`-headers

## Kvarstående

### Säkerhet
- [ ] Rensa exponerad privat nyckel ur **git-historiken** (`git filter-repo`)
- [ ] Rotera secure boot-nyckel om den publicerats
- [ ] Aktivera HTTPS i produktion (HTTP används i dev-läge)
- [ ] Fullständig certificate pinning för MQTT i drift

### Hårdvara / test
- [ ] HIL-test på ESP32-S3 + SHT40 + PWM-fläktar
- [ ] Förbättra OLED-font (nu enkel bitmap)
- [ ] MicroSD-loggning (audit log till flash/SD) — ej implementerat

### MQTT / konfiguration
- [ ] Sätt `MQTT_BROKER_DEFAULT` och `OTA_SERVER_URL` via menuconfig/NVS
- [ ] Ladda OTA Ed25519 publik nyckel från NVS vid boot

### Dokumentation / release
- [ ] Bygg och uppdatera `binaries/` för v1.3.0
- [ ] Verifiera CI-build mot `espressif/idf:release-v5.1`

## Driftlägen (SR-010)

| Läge | Konfiguration | Beteende vid \>90 % RH |
|------|---------------|------------------------|
| AC Monitor | `TF_MODE_AC_MONITOR` | Larm, fläktar oförändrade |
| Värmeväxlare | `TF_MODE_HEAT_EXCHANGER` (default) | Fläktar AV |
| Mini-FTX | `TF_MODE_MINI_FTX` | Max ventilation |

Ändra via `THERMOFLOW_DEFAULT_MODE` i `thermoflow_config.h`.