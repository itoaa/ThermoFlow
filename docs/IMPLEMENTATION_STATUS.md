# ThermoFlow Implementation Status

**Document Version:** 2.0.0  
**Last Updated:** 2026-07-12  
**Firmware Version (`main.c`):** 1.2.0  
**Project:** ThermoFlow — ESP32-S3 Climate Monitoring and Control System

> Denna fil beskriver **vad som faktiskt finns i kod och körs i firmware**, inte vad som är planerat eller dokumenterat i äldre versioner.  
> För allt som saknas och förbättringsförslag, se [TODO.md](TODO.md).

---

## Sammanfattning

ThermoFlow har en välstrukturerad komponentarkitektur med många färdiga moduler, men **huvudapplikationen (`main.c`) integrerar bara en delmängd**. Vid boot körs idag:

1. NVS-init  
2. Hardware detection (I2C-probe för SHT40/OLED)  
3. Sensor manager (simulerad data)  
4. WiFi manager (AP-läge eller anslutning)  
5. Fan controller (mjukvarustate, ingen PWM)  
6. Anti-condensation  
7. OTA manager (init + monitor, utan faktisk uppdatering)  
8. Control task + OTA monitor task  

**Startas inte vid boot:** web_server, MQTT, heat_recovery, display, audit_log, rate_limiter, security_manager, cert_manager.

---

## Statusförklaring

| Status | Betydelse |
|--------|-----------|
| **Körs** | Initieras/anropas från `main.c` |
| **Implementerad** | Fungerande kod i komponenten |
| **Delvis** | Kod finns men stub, fallback eller ofullständig |
| **Ej kopplad** | Implementerad komponent som inte anropas från `main.c` |
| **Stub** | Platshållare utan riktig funktionalitet |
| **Saknas** | Nämns i docs men ingen kod |

---

## Komponentmatris

| Component | Kodstatus | Körs i `main.c` | Tester | Kommentar |
|-----------|-----------|-----------------|--------|-----------|
| **hardware_manager** | Delvis | ✅ Körs | — | I2C-probe för SHT40/OLED fungerar; fläktar markeras "detekterade" om GPIO kan konfigureras |
| **sensor_manager** | Delvis | ✅ Körs | — | Returnerar alltid simulerad data; `sht4x`-drivrutin anropas inte |
| **sht4x_sensor** | Implementerad | ❌ Ej kopplad | ✅ Unity | Full I2C-drivrutin med CRC; används inte av sensor_manager |
| **sht3x_sensor** | Implementerad | ❌ Ej kopplad | — | Legacy-komponent, inte integrerad |
| **wifi_manager** | Delvis | ✅ Körs | — | AP-läge och WiFi-anslutning fungerar |
| **wifi_secure_storage** | Stub | Via wifi_manager | ⏳ Finns | Krypteringsstubbar i kompatibilitetsläge |
| **fan_control** | Delvis | ✅ Körs | ✅ Unity | Fail-safe-logik i mjukvara; **ingen LEDC/PWM** |
| **anti_condensation** | Implementerad | ✅ Körs | ✅ Unity | Tröskelvärden och hysteresis fungerar |
| **web_server** | Delvis | ❌ Ej kopplad | ⏳ Finns | HTTP-API och SPA finns; **HTTPS inaktiverat**; startas inte i main |
| **mqtt_client** | Delvis | ❌ Ej kopplad | ⏳ Finns | Omfattande TLS-kod; ej startad från main |
| **mqtt_ftx** | Delvis | ❌ Ej kopplad | — | FTX-specifika MQTT-topics; ej startad |
| **heat_recovery** | Implementerad | ❌ Ej kopplad | — | Beräkningslogik (frost, effektivitet); fristående bibliotek |
| **display_driver** | Stub | ❌ Ej kopplad | — | Loggar till serial, ingen OLED-skrivning |
| **ota_manager** | Stub | ✅ Körs | — | Init + status; download/apply/rollback **ej implementerat** |
| **security_utils** | Delvis | ❌ Ej kopplad | — | Certifikathantering delvis; **Ed25519 är placeholder** |
| **cert_manager** | Delvis | ❌ Ej kopplad | — | Finns som komponent, ej kopplad till main |
| **audit_log** | Implementerad | ❌ Ej kopplad | — | Minnesbuffert med checksum; ej startad |
| **rate_limiter** | Implementerad | ❌ Ej kopplad | — | Token bucket; ej kopplad till web_server |
| **anti_condensation** | Implementerad | ✅ Körs | ✅ Unity | Se SR-010-konflikt i [TODO.md](TODO.md) |

**Förkortningar:** ⏳ Finns = testfil finns men ingår inte i `test_main.c`

---

## Vad `main.c` faktiskt gör

### Startsekvens (implementerad)
```
NVS → hardware_manager → sensor_manager → wifi_manager
  → fan_controller (om fläkt "detekterad") → anti_condensation → ota_manager
  → control_task + ota_monitor_task
```

### Control loop (implementerad)
- Läser sensorer via `sensor_manager_read_all()` (simulerat)
- Kör `anti_condensation_check()`
- Styr fläkt 1 baserat på temperatur/kondensationslarm (mjukvarustate)
- Anropar `wifi_manager_run()`
- Loggar status var 5:e sekund

### Control loop (ej implementerad)
- Fläkt 2-styrning
- FTX/värmeåtervinningslogik
- MQTT-publicering
- Web API-uppdatering
- Displayuppdatering
- Audit logging av händelser

---

## Funktioner per README — ärlig status

| Funktion | Dokumenterad | Verklig status |
|----------|--------------|----------------|
| Multi-sensor SHT40 (upp till 4 st) | ✅ | Delvis — detektering ja, läsning nej (simulation) |
| PWM-fläktstyrning (2 st) | ✅ | Delvis — logik ja, hårdvara nej |
| Anti-kondens (\>90 % RH) | ✅ | Delvis — detektion ja; fläktbeteende inkonsekvent med SR-010 |
| MQTT + TLS | ✅ | Ej kopplad — kod finns, startas inte |
| Webbgränssnitt (SPA/PWA) | ✅ | Ej kopplad — startas inte från main |
| HTTPS | ✅ | Delvis — faller tillbaka till HTTP |
| OTA med Ed25519 | ✅ | Stub — ingen faktisk uppdatering |
| WiFi Manager (AP-läge) | ✅ | Körs |
| Hårdvarudetektering | ✅ | Delvis — I2C ja, fläktar förenklad detektering |
| Simuleringsläge | ✅ | Körs |
| Mini-FTX / värmeåtervinning | ✅ | Ej kopplad — bibliotek finns |
| OLED-display | ✅ | Stub |
| MicroSD-loggning | ✅ | Saknas |
| IEC 62443 SL-2 | ✅ | Delvis — ramverk och vissa moduler, ej full integration |

---

## Säkerhetskrav (SR-001 – SR-011)

| Requirement | Component | Kodstatus | Körs i firmware |
|-------------|-----------|-----------|-----------------|
| SR-001: Input Validation | sensor_manager | Delvis | ✅ (validering av simulerad data) |
| SR-002: Authentication | security_utils | Delvis | ❌ |
| SR-003: Secure Communication | mqtt_client, web_server | Delvis | ❌ (ej startade; HTTPS av) |
| SR-004: Fail-Safe Defaults | fan_control | Delvis | ✅ (mjukvara) |
| SR-005: Audit Logging | audit_log | Implementerad | ❌ |
| SR-006: Resource Limits | rate_limiter | Implementerad | ❌ |
| SR-007: Error Handling | Alla | Delvis | Varierar |
| SR-008: Dependency Management | — | Saknas | ❌ SBOM ej skapad |
| SR-009: Actuator Fail-Safe | fan_control | Delvis | ✅ (ingen HW-failsafe) |
| SR-010: Environmental Limits | anti_condensation | Implementerad | ⚠️ Konflikt med main.c-fläktlogik |
| SR-011: OTA Security | ota_manager, ed25519 | Stub | ❌ |

---

## Teststatus

### Körs via `tests/test_main.c`
| Test suite | Fil | Status |
|------------|-----|--------|
| SHT4x sensor | `test_sht4x.c` | ✅ I runner |
| Fan controller | `test_fan_controller.c` | ✅ I runner |
| Anti-condensation | `test_anti_condensation.c` | ✅ I runner |

### Finns men körs inte automatiskt
| Test suite | Fil |
|------------|-----|
| HTTPS / web server | `test_https.c` |
| WiFi encryption | `test_wifi_encryption.c` |
| Secure boot | `test_secure_boot.c` |
| MQTT TLS | `components/mqtt_client/test_mqtt_tls.c` |

---

## Build Status

Senast dokumenterad bygginfo (från tidigare release):

```
Target: ESP32-S3
ESP-IDF: v5.1.2
Binary: ~750 KB (28 % flash)
```

För att bygga:

```bash
cd ThermoFlow
./build.sh
```

---

## Kända säkerhetsproblem

Se [TODO.md](TODO.md) avsnitt 1. Kortfattat:

1. **Privata nycklar i `keys/`** — måste roteras och tas bort från repot  
2. **Ed25519 placeholder** — inte lämplig för produktion  
3. **WiFi-krypteringsstub** — loggar "NOT SECURE" i vissa kodvägar  

---

## Nästa steg

Prioriterad lista finns i [TODO.md](TODO.md). De tre viktigaste:

1. Koppla `web_server` och `sensor_manager` → riktig SHT40-läsning i `main.c`  
2. Implementera PWM (LEDC) och lös SR-010-konflikten  
3. Rotera nycklar och slutför säkerhetsstubbar innan produktion  

---

**Project Owner:** Ola Andersson  
**GitHub:** https://github.com/itoaa/ThermoFlow