# ThermoFlow Implementation Status

**Document Version:** 4.0.0  
**Last Updated:** 2026-07-14  
**Firmware Version:** CalVer `2026.29.BUILD` (see [VERSIONING.md](VERSIONING.md))

## Sammanfattning

Firmware integrerar alla kärnkomponenter i `main.c`. HTTP-webbserver startas vid boot. WiFi-uppgifter sparas krypterat med legacy-backup i NVS och överlever `app-flash`. AP+STA-fallback visar väntesida istället för onboarding när uppgifter finns sparade.

## Komponentmatris

| Component | Kod | Körs i main | Tester |
|-----------|-----|-------------|--------|
| hardware_manager | ✅ | ✅ | — |
| sensor_manager + sht4x | ✅ | ✅ | ✅ |
| fan_control (LEDC PWM) | ✅ | ✅ | ✅ |
| anti_condensation | ✅ | ✅ | ✅ |
| wifi_manager | ✅ | ✅ | — |
| wifi_secure_storage + legacy backup | ✅ | ✅ | ✅ |
| web_server (HTTP + SPA + logg) | ✅ | ✅ | ⏳ |
| mqtt_client / mqtt_ftx | ✅ | ✅* | ⏳ |
| heat_recovery | ✅ | ✅ | ✅ |
| ota_manager | ✅ | ✅ | — |
| security_utils + Ed25519 | ✅ | ✅ | — |
| audit_log (+ web `/api/logs`) | ✅ | ✅ | — |
| rate_limiter | ✅ | ✅ | — |
| display_driver (SSD1306) | ✅ | ✅** | — |
| CalVer `generate_version.py` | ✅ | ✅ | — |

\* MQTT kräver `MQTT_BROKER_DEFAULT` i `thermoflow_config.h`  
\** Endast om OLED detekteras vid boot

## Startsekvens i `main.c`

```
NVS → security_manager → audit_log → rate_limiter → hardware_manager
→ sensor_manager → ftx_init → wifi_manager → web_server → mqtt_ftx_init
→ fan_controller → display → anti_condensation → ota → control_task
```

## WiFi och NVS (2026-07-14)

| Funktion | Status |
|----------|--------|
| Krypterad lagring (AES-256-CBC) | ✅ |
| Legacy NVS-backup (dual-write) | ✅ |
| app-flash bevarar WiFi | ✅ (standard i flash-skript) |
| AP+STA fallback (90 s, 15 retries) | ✅ |
| Reconnect-sida vid fallback | ✅ |
| Enhets-ID från fabriks-MAC | ✅ |
| Redigerbart visningsnamn | ✅ |

## Webbgränssnitt

| Vy | Status |
|----|--------|
| Dashboard | ✅ |
| Sensorer | ✅ |
| FTX | ✅ |
| Inställningar (ID, namn, WiFi) | ✅ |
| Logg (audit events) | ✅ |
| Onboarding (`wifi_config.html`) | ✅ (endast utan sparad WiFi) |
| Väntesida (`wifi_reconnect.html`) | ✅ (AP+STA fallback) |

## Kända begränsningar

| Område | Status |
|--------|--------|
| HTTPS | Kod finns, runtime fallback till HTTP |
| MicroSD | Ej implementerat |
| OTA URL / MQTT broker | Tomma strängar i config — måste sättas |
| Git-historik nycklar | Manuell rensning krävs |

Se [TODO.md](TODO.md) för kvarvarande arbete.