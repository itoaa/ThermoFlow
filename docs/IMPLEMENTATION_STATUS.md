# ThermoFlow Implementation Status

**Document Version:** 3.0.0  
**Last Updated:** 2026-07-12  
**Firmware Version:** 1.3.0

## Sammanfattning

Firmware **1.3.0** integrerar alla kärnkomponenter i `main.c`. HTTP-webbserver startas vid boot. MQTT ansluter när WiFi är uppe och broker är konfigurerad. HTTPS faller fortfarande tillbaka till HTTP tills certifikat/ESP-IDF-problem löses.

## Komponentmatris

| Component | Kod | Körs i main | Tester |
|-----------|-----|-------------|--------|
| hardware_manager | ✅ | ✅ | — |
| sensor_manager + sht4x | ✅ | ✅ | ✅ |
| fan_control (LEDC PWM) | ✅ | ✅ | ✅ |
| anti_condensation | ✅ | ✅ | ✅ |
| wifi_manager | ✅ | ✅ | — |
| wifi_secure_storage | ✅ | ✅ | — |
| web_server (HTTP) | ✅ | ✅ | ⏳ |
| mqtt_client / mqtt_ftx | ✅ | ✅* | ⏳ |
| heat_recovery | ✅ | ✅ | ✅ |
| ota_manager | ✅ | ✅ | — |
| security_utils + Ed25519 | ✅ | ✅ | — |
| audit_log | ✅ | ✅ | — |
| rate_limiter | ✅ | ✅ | — |
| display_driver (SSD1306) | ✅ | ✅** | — |
| sht3x_sensor | ✅ | ❌ legacy | — |

\* MQTT kräver `MQTT_BROKER_DEFAULT` satt i `thermoflow_config.h`  
\** Endast om OLED detekteras vid boot

## Startsekvens i `main.c`

```
NVS → security_manager → audit_log → rate_limiter → hardware_manager
→ sensor_manager → ftx_init → wifi_manager → web_server → mqtt_ftx_init
→ fan_controller → display → anti_condensation → ota → control_task
```

## Kända begränsningar

| Område | Status |
|--------|--------|
| HTTPS | Kod finns, runtime fallback till HTTP |
| MicroSD | Ej implementerat |
| OTA URL / MQTT broker | Tomma strängar i config — måste sättas |
| Git-historik nycklar | Manuell rensning krävs |

Se [TODO.md](TODO.md) för kvarvarande arbete.