# ThermoFlow-dokumentation

Välkommen till den publika handboken för **ThermoFlow** — ESP32-S3-baserad klimatövervakning och styrning (mobil AC, värmeväxlare, Mini-FTX, sensor-only).

Denna sajt byggs automatiskt från mappen [`docs/`](https://github.com/itoaa/ThermoFlow/tree/main/docs) i GitHub-repot vid varje push till `main` (eller manuellt via Actions).

## Snabbval

| Jag vill … | Gå till |
|------------|---------|
| Koppla SHT40 med **Cat 5e / Cat 6** | [Sensoranslutning](SENSOR_WIRING.md) |
| Förstå **GPIO 8/9**, plintar, färgkod | [Sensoranslutning](SENSOR_WIRING.md) |
| Flasha utan att tappa WiFi | [WiFi och flash](WIFI_AND_FLASH.md) |
| Köra **mobil AC**-läge | [Mobil AC](MOBILE_AC.md) |
| Välja applikationsläge | [Applikationslägen](APPLICATION_MODES.md) |
| Bygga Mini-FTX | [FTX-extension](FTX_EXTENSION.md) |

## Koppling till enhetens webbgränssnitt

ThermoFlows lokala UI (på ESP:n) visar live-data och styrning. **?**-knappar och menyn **Dokumentation** öppnar sidor här på docs-sajten (kräver att telefon/PC har internet).

| Plats i UI | Länk hit |
|------------|----------|
| Nav → Dokumentation | Denna startsida |
| Inställningar → Dokumentation | Snabb länkar |
| Hjälp-modal → “Öppna dokumentation” | Relevant kapitel (t.ex. Mobil AC) |

Standard-URL: `https://itoaa.github.io/ThermoFlow/`  
(API: `GET /api/device/info` → fältet `docs_url`)

## Lokal förhandsvisning

```bash
pip install -r requirements-docs.txt
mkdocs serve
# → http://127.0.0.1:8000
```

## Firmware vs dokumentation

| | Firmware (ESP) | Denna docs-sajt |
|--|----------------|-----------------|
| Innehåll | Dashboard, API, styrning | Handböcker, kopplingsscheman |
| Uppdatering | OTA / USB-flash | Automatisk vid docs-commit |
| Offline | Ja (lokalt nät) | Nej (om du inte sparat sidan) |

Kort offline-hjälp (pin-map) kan finnas i enhetens UI; fullständig dokumentation hålls här så flashen inte fylls.
