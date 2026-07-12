# ThermoFlow — TODO och förbättringsförslag

**Senast uppdaterad:** 2026-07-12  
**Firmware-version i `main.c`:** 1.2.0  
**Syfte:** Samla allt som inte är färdigt implementerat, inte är kopplat till körning, samt förbättringsförslag från kodgranskning.

Se även [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) för detaljerad komponentstatus.

---

## Statusförklaring

| Symbol | Betydelse |
|--------|-----------|
| 🔴 Kritisk | Säkerhetsrisk eller blockerar produktion |
| 🟠 Hög | Viktig funktion saknas eller är felkopplad |
| 🟡 Medel | Delvis implementerat eller inkonsekvent |
| 🟢 Låg | Städning, dokumentation, nice-to-have |

---

## 1. Kritiska åtgärder (säkerhet)

### 🔴 Privata nycklar i repot
- **Status:** ✅ Privat nyckel borttagen från working tree; `.gitignore` och `keys/README.md` tillagda (2026-07-12)
- **Kvarstår:** Rensa nyckeln ur git-historiken (`git filter-repo` eller BFG) och rotera nyckel om den exponerats publikt

### 🔴 Ed25519-signering är placeholder
- **Fil:** `components/security_utils/ed25519_impl.c`
- **Problem:** Signering och verifiering är stub; `ed25519_verify()` returnerar alltid fel.
- **Konsekvens:** OTA-signaturer och secure boot kan inte användas på riktigt.
- **Åtgärd:** Integrera `ed25519-donna` eller libsodium; ta bort placeholder-logik.

### 🔴 WiFi-krypteringsstub i kompatibilitetsläge
- **Fil:** `components/wifi_manager/wifi_secure_storage.c`
- **Problem:** `decrypt_credential()`, `wifi_secure_has_credentials()` och migrering är stubbar som loggar "NOT SECURE".
- **Åtgärd:** Slutför AES-256-CBC + HMAC-implementationen eller inaktivera krypteringsflaggan tills den fungerar.

---

## 2. Integration i `main.c` (ej kopplat till körning)

Följande komponenter finns i `main/CMakeLists.txt` men **startas inte** i `main.c`:

| Komponent | Init-funktion | Prioritet |
|-----------|---------------|-----------|
| Web server | `web_server_init()` + `web_server_start()` | 🟠 Hög |
| MQTT / FTX | `mqtt_ftx_init()` | 🟠 Hög |
| Värmeåtervinning | `ftx_init()` + koppling till control loop | 🟠 Hög |
| Display | `display_manager_init()` | 🟡 Medel |
| Audit log | `audit_log_init()` | 🟡 Medel |
| Rate limiter | `rate_limiter_init()` | 🟡 Medel |
| Security manager | `security_manager_init()` | 🟠 Hög |
| Cert manager | `cert_manager_init()` | 🟡 Medel |

### Föreslagen integrationsordning i `app_main()`
1. `security_manager_init()`
2. `audit_log_init()`
3. `rate_limiter_init()`
4. Befintlig WiFi-init
5. `web_server_init()` → `web_server_start()` (eller HTTPS när det fungerar)
6. `mqtt_ftx_init()` efter WiFi-anslutning
7. `ftx_init()` och koppla sensordata till `heat_recovery`
8. `display_manager_init()` om OLED detekteras

---

## 3. Funktioner som inte är implementerade

### OTA-uppdateringar
- **Fil:** `components/ota_manager/ota_manager.c`
- **Status:** Endast init och status; `start_update`, `apply_update`, `rollback` returnerar `ESP_ERR_NOT_SUPPORTED`
- **Problem i main:** `ota_manager_mark_valid()` anropas direkt efter boot utan hälsokontroll
- **Åtgärd:** Använd `esp_https_ota`, verifiera signatur före flash, markera valid först efter subsystem-test

### PWM-fläktstyrning (hårdvara)
- **Fil:** `components/fan_control/fan_controller.c`
- **Status:** Uppdaterar intern state/loggning; ingen `ledc_` eller GPIO-PWM
- **Åtgärd:** Konfigurera LEDC på GPIO 10/11, koppla `set_speed()` till duty cycle

### SHT40-läsning i sensor_manager
- **Fil:** `components/sensor_manager/sensor_manager.c` (rad ~179)
- **Status:** `sht4x_sensor`-drivrutinen finns men anropas inte; all data är simulerad även i "hardware mode"
- **Åtgärd:** Initiera `sht4x_init()` per detekterad adress och läs i `sensor_manager_read_all()`

### OLED-display
- **Fil:** `components/display_driver/display_manager.c`
- **Status:** Stub som loggar till serial
- **Åtgärd:** Implementera SSD1306/I2C-skrivning

### HTTPS-webbserver
- **Fil:** `components/web_server/web_server.c`
- **Status:** `web_server_start_https()` faller alltid tillbaka till HTTP (ESP-IDF v5.1.2-kompatibilitet)
- **Åtgärd:** Lös `esp_tls_handshake_callback`-problemet eller uppgradera ESP-IDF

### MicroSD-loggning
- **Status:** Nämns i README och `PROJECT_FRAMEWORK.md`; ingen `sdmmc`-kod finns
- **Åtgärd:** Implementera SD-kort för audit log och sensordata, eller ta bort från dokumentation tills det byggs

### SHT3x-sensor
- **Fil:** `components/sht3x_sensor/`
- **Status:** Komponent finns men används inte (projektet standardiserar på SHT40)
- **Åtgärd:** Ta bort eller dokumentera som legacy/alternativ

### Threat model och SBOM
- **Saknas:** `docs/security/threat-model.md` (refererad i framework)
- **Saknas:** SBOM enligt SR-008
- **Åtgärd:** Skapa dokument eller markera som planerat i roadmap

### CI/CD
- **Saknas:** `.github/workflows/`, `ci/`-mapp (nämns i `PROJECT_FRAMEWORK.md`)
- **Åtgärd:** GitHub Actions med `idf.py build` och Unity-tester

---

## 4. Logiska inkonsekvenser

### SR-010: Anti-kondens vs fläktbeteende
- **`PROJECT_FRAMEWORK.md`:** \>90 % RH → fläktar **AV**
- **`main.c`:** Vid kondensationslarm → fläktar till **100 %**
- **Åtgärd:** Definiera beteende per driftläge (AC / värmeväxlare / FTX) och gör det konsekvent i kod och docs

### Versionsnummer
| Plats | Version |
|-------|---------|
| `main.c` | 1.2.0 |
| `README.md` (före uppdatering) | 1.5.0 |
| `CHANGELOG.md` | 2.0.0 |
| `binaries/README.md` | 1.4.0 |

**Åtgärd:** Använd `main.c` som källa för firmware-version tills en central `version.h` införs.

### Dubbla header-filer
- **`include/`** och **`components/*/include/`** innehåller duplicerade headers (`fan_controller.h`, `web_server.h`, m.fl.)
- **`include/wifi_manager.h`** är en inline-stub som inte matchar den riktiga komponenten
- **Åtgärd:** Ta bort `include/`-stubbar eller gör `include/` till enda publikt API

### PlatformIO-rester
- **`library.json`** i 11 komponenter (onödiga efter ESP-IDF-migrering)
- **Åtgärd:** Ta bort eller dokumentera som legacy

---

## 5. Tester

### Körs idag (`tests/test_main.c`)
- `test_sht4x_sensor`
- `test_fan_controller`
- `test_anti_condensation`

### Finns men körs inte
- `tests/test_https.c`
- `tests/test_wifi_encryption.c`
- `tests/test_secure_boot.c`
- `components/mqtt_client/test_mqtt_tls.c`

### Saknas
- Integrationstest: WiFi → web_server → API
- HIL-test (hardware-in-the-loop) för I2C och PWM
- Test av `heat_recovery`-beräkningar (finns logik men inga Unity-tester)

**Åtgärd:** Utöka `test_main.c` eller lägg till CI-steg per testsvit.

---

## 6. Förbättringsförslag (prioriterad roadmap)

### Fas 1 — Stabil grund (1–2 veckor)
1. 🔴 Rotera nycklar och fixa `.gitignore`
2. 🟠 Koppla `web_server` till `main.c` (HTTP räcker i dev)
3. 🟠 Koppla `sht4x_sensor` till `sensor_manager`
4. 🟠 Implementera LEDC PWM för fläktar
5. 🟡 Synka versionsnummer (`include/thermoflow_version.h`)

### Fas 2 — FTX och nätverk (2–4 veckor)
6. 🟠 Integrera `heat_recovery` i control loop
7. 🟠 Starta MQTT efter WiFi-anslutning
8. 🟡 Koppla `audit_log` och `rate_limiter` till web_server
9. 🟡 Lös SR-010-beteende per driftläge
10. 🟡 Hårdvarutest på ESP32-S3 + SHT40

### Fas 3 — Säkerhet och produktion (1–2 månader)
11. 🔴 Riktig Ed25519 + OTA via `esp_https_ota`
12. 🟠 HTTPS (eller tydlig dev/prod-profil)
13. 🟠 Slutför WiFi-krypterad lagring
14. 🟡 SBOM + threat model
15. 🟡 GitHub Actions CI
16. 🟢 OLED-display, MicroSD (om fortfarande önskat)

---

## 7. Dokumentation att hålla synkad

När kod ändras, uppdatera alltid:
- [README.md](../README.md) — funktioner och quick start
- [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md) — komponentmatris
- [TODO.md](TODO.md) — denna fil
- [CHANGELOG.md](../CHANGELOG.md) — vid release

**Regel:** Markera aldrig en funktion som "✅ Complete" om den inte både är implementerad **och** anropas från `main.c` (eller dokumenteras som fristående bibliotek).

---

**Projektägare:** Ola Andersson  
**GitHub:** https://github.com/itoaa/ThermoFlow