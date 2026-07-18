# WiFi, onboarding och flash

**Senast uppdaterad:** 2026-07-14  
**Firmware:** CalVer `YYYY.WW.BUILD` (t.ex. `2026.29.42`)

Detta dokument beskriver hur WiFi-konfiguration sparas, när onboarding behövs, och hur du flashar utan att tappa inställningar.

---

## Snabbreferens

| Situation | Vad du ska göra |
|-----------|-----------------|
| Normal firmware-uppdatering | `app-flash` (standard i `flash.sh` / `flash-local.ps1`) |
| Fabriksåterställning / ny enhet | `ERASE_FLASH=1 ./flash.sh PORT` eller `idf.py erase-flash flash` |
| Ser setup-AP efter flash | **Vänta 1–2 min** — enheten kan vara i AP+STA-fallback |
| Onboarding behövs | Endast om inga WiFi-uppgifter finns i NVS |

---

## WiFi-lagring (NVS)

WiFi-uppgifter sparas i NVS på två sätt:

1. **Krypterad lagring** (`wifi_sec_cfg`) — AES-256-CBC + HMAC (SEC-021)
2. **Legacy-backup** (`wifi_config`) — klartext som redundans

Vid sparning skrivs båda. Vid laddning försöks krypterad lagring först, sedan legacy. Migration från legacy till krypterad lagring **behåller** legacy-kopian.

### Enhetsidentitet (separat från WiFi)

| Fält | Källa | Redigerbar |
|------|-------|------------|
| **Enhets-ID** (`ThermoFlow-XXXX`) | Fabriks-MAC (sista 4 hex) | Nej |
| **Visningsnamn** | NVS `device_config` | Ja (Inställningar i webben) |

Enhets-ID läses via `esp_read_mac()` före WiFi-init så att det alltid matchar MAC-adressen.

---

## Anslutningsbeteende vid boot

```
Boot
  │
  ├─ Inga sparade WiFi-uppgifter → AP-läge (onboarding)
  │
  └─ Uppgifter finns → STA-anslutning (upp till 90 s, 15 försök)
        │
        ├─ Ansluten → normal drift (dashboard)
        │
        └─ Timeout / misslyckande → AP+STA-fallback
              ├─ Setup-AP: ThermoFlow-XXXX (192.168.4.1)
              └─ Bakgrund: fortsätter försöka ansluta till sparat SSID
```

**Viktigt:** AP+STA-fallback betyder **inte** att uppgifterna försvunnit. Setup-AP:t är en reservväg medan hemmanätverket försöks igen.

---

## Webbgränssnitt vid olika lägen

| Läge | `http://192.168.4.1/` visar |
|------|----------------------------|
| Ingen sparad WiFi | Onboarding (`wifi_config.html`) |
| AP+STA-fallback (uppgifter sparade) | **Ansluter till WiFi** (`wifi_reconnect.html`) |
| Ansluten till hemmanätverk | Dashboard (`index.html`) |

Direktlänkar:
- `/wifi_config.html` — alltid tillgänglig för att byta nätverk
- `/wifi_reconnect.html` — väntesida under fallback

---

## Flash utan att tappa WiFi

### Linux / macOS

```bash
./flash.sh /dev/ttyUSB0          # app-flash (NVS bevaras)
ERASE_FLASH=1 ./flash.sh /dev/ttyUSB0   # full erase (raderar NVS)
```

### Windows

```powershell
powershell -ExecutionPolicy Bypass -File flash-local.ps1 COM4
```

`flash-local.ps1` använder `idf.py app-flash` — endast app-partitionen uppdateras.

### Vad som bevaras vid app-flash

- WiFi SSID och lösenord (krypterat + legacy)
- Visningsnamn
- Övriga NVS-inställningar

### Vad som raderas vid erase-flash

Hela flash minnet inklusive NVS — **onboarding krävs** efteråt.

---

## API (WiFi-relaterat)

### `GET /api/device/info`

Relevanta fält:

```json
{
  "device_id": "ThermoFlow-1440",
  "device_name": "Mitt kök",
  "has_custom_name": true,
  "wifi_state": "connected",
  "wifi_credentials_saved": true,
  "wifi_ap_fallback": false,
  "wifi_saved_ssid": "S22",
  "ip_address": "192.168.32.241",
  "firmware_version": "2026.29.42",
  "version_full": "2026.29.42+040f567"
}
```

| Fält | Betydelse |
|------|-----------|
| `wifi_credentials_saved` | Uppgifter finns i NVS |
| `wifi_ap_fallback` | AP+STA-fallback aktiv (försöker hemmanätverk i bakgrunden) |
| `wifi_saved_ssid` | Sparat nätverksnamn (för väntesidan) |

### `POST /api/wifi/config`

```json
{ "ssid": "MittWiFi", "password": "hemligt" }
```

Sparar (dual-write), startar om enheten.

### `DELETE /api/wifi/config`

Raderar WiFi-uppgifter (krypterat + legacy), startar om i ren onboarding.

---

## Felsökning

| Symptom | Trolig orsak | Åtgärd |
|---------|--------------|--------|
| ThermoFlow-XXXX syns efter flash | AP+STA-fallback eller långsam router | Vänta 1–2 min; öppna 192.168.4.1 — ska visa "Ansluter" |
| Onboarding varje gång | `erase-flash` används | Byt till `app-flash` |
| Onboarding efter NVS-fel | `nvs_flash_erase()` vid korrupt NVS | Spara WiFi en gång; sedan app-flash |
| Fel enhets-ID (t.ex. -7A5D) | Gammalt namn i NVS före fix | Uppdatera firmware; ID kommer från MAC |

### NVS-radering vid boot

Om NVS-partitionen är korrupt eller full raderas den vid boot (`main.c` → `init_nvs()`). Det loggas:

```
NVS partition needs erase — WiFi and other saved settings will be cleared
```

Efter detta krävs onboarding **en gång**; därefter överlever uppgifterna app-flash.

---

## Relaterad dokumentation

- [WiFi_Encryption.md](WiFi_Encryption.md) — krypteringsdetaljer (SEC-021)
- [BUILD.md](https://github.com/itoaa/ThermoFlow/blob/main/BUILD.md) — bygg och flash
- [CHANGELOG.md](https://github.com/itoaa/ThermoFlow/blob/main/CHANGELOG.md) — versionshistorik