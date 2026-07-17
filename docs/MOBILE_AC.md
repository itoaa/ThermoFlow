# Mobil AC – övervakning och styrning

**Version:** 1.2  
**Senast uppdaterad:** 2026-07-17  
**Läge-ID:** `ac_monitor`

Webbgränssnittets **?**-ikoner använder samma definitioner som denna sida.  
Översikt över alla lägen: [APPLICATION_MODES.md](APPLICATION_MODES.md).

---

## Syfte

ThermoFlow kopplas till en **portabel luftkonditionering** med upp till **fyra** temperatursensorer:

| Sensorroll (API-nyckel) | Visningsnamn | Placering |
|-------------------------|--------------|-----------|
| `supply` (`supply_temp`) | **Utgående kall luft** | Kylutblås in i rummet (efter förångare) |
| `extract` (`extract_temp`) | **Ingående kall luft** | Luft in till kalla sidan / förångare |
| `exhaust` (`exhaust_temp`) | **Utgående varm luft** | Kondensorutblås / slang ut |
| `outdoor` (`outdoor_temp`) | **Varmsida intag** | Luft in till kondensorn — se nedan |

Målet är att **se hur AC:n presterar** (kyllyft, värmeavgivning, verkningsgrad) och, om tillval är på, **styra stödaktuatorer**.

### UI-regel för data

Webbgränssnittet visar **endast riktiga mätvärden eller `N/A`**, utom när:

- **Demo-läge** (`?demo=1` i webbläsaren), eller  
- **Simulering / Sin-mode** (datakälla = *Simulering* under Inställningar).

Auto-fallback till simulerad data (inga sensorer) **döljs** i UI (visas som N/A) så att man inte förväxlar fejkade siffror med mätning. Saknade/ogiltiga kanaler skickas som JSON `null` från API.

---

## Sensorplacering

```
        ┌─────────────────────────────────────┐
Rum ──► │ extract (kall in)                   │
        │         ↓  förångare                │
        │ supply (kall ut)  ──► rum           │
        │                                     │
        │ outdoor_temp = **Varmsida intag**   │
        │   1-slang: ofta rumsluft            │
        │   2-slang: oftast uteluft           │
        │         ↓  kondensor                │
        │ exhaust (varm ut) ──► slang/ute     │
        └─────────────────────────────────────┘
```

### Varmsida intag

Tidigare kallades sloten “outdoor” / utomhustemp, vilket missvisar för **1-slangars** portabel AC där kondensorn oftast suger **rumsluft**. Vid **2-slangars** ombyggnad är samma mätpunkt däremot normalt **uteluft**.

Därför heter mätpunkten i UI **Varmsida intag** (neutral). API-nyckeln förblir `outdoor_temp` så att samma sensor-slot fungerar i Mini-FTX/värmeväxlare (där den betyder uteluft).

**Tips för utgående varm luft:** mät **blandad bulktemperatur** (mitt i flödet / efter en kort blandningssträcka), inte bara i kanten av slangen. Stratifierat flöde ger fel ΔT och fel COP-proxy.

I2C-adresser (fasta slots): sensor 0=`supply`, 1=`extract`, 2=`exhaust`, 3=`outdoor` (AC: varmsida intag).

---

## Inspiration från liknande projekt

| Projekt / mönster | Vad vi tar med |
|-------------------|----------------|
| [ESPHome PWM fan + tach](https://github.com/patrickcollins12/esphome-fan-controller) | Separat **börvärde (PWM %)** och **uppmätt RPM** |
| HVAC energibalans utan elmätare | COP-proxy från ΔT på båda sidor |
| HVAC “fault response” | Vid risk: **observera**, **förstärk flöde**, eller **mildra kylning** |

---

## Nyckeltal (Överblick)

Beräknas i UI (`computeAcMetrics`) från de fyra temperaturerna:

| Nyckeltal | Formel | Betydelse |
|-----------|--------|-----------|
| **Kyllyft (ΔT)** | \(T_{kall\_in} - T_{kall\_ut}\) | Hur många grader luftströmmen kyls |
| **Värmeavgivning** | \(T_{varm\_ut} - T_{varmsida\_intag}\) | Hur mycket varmsidan värmer luften |
| **Sidobalans** | \(\Delta T_{varm} / \Delta T_{kall}\) (om kyllyft > 0,5 °C) | Ofta > 1 (kompressorvärme på varmsidan) |
| **Termisk verkningsgrad η** | \(\Delta T_{kall} / \Delta T_{varm} \times 100\%\) | Andel av avgiven värme som kommer från kyla (lika massflöde) |
| **COP-proxy** | \(\Delta T_{kall} / (\Delta T_{varm} - \Delta T_{kall})\) | Luftsidig uppskattning av “kyla per kompressorvärme” |
| **Blandtemp. ut** | \((T_{kall\_ut} + T_{varm\_ut}) / 2\) | Snabb kontroll av mätpunkter / flödesbalans |
| **Kondensrisk** | Låg / medel / hög från RH + T på kylutblås | Se nedan |
| **Kylindex** | 0–100 från kyllyft (~12 °C → 100) | Lättläst “hur hårt den kyler” |

Saknas kallingång används varmingång som fallback-referens för kyllyft.

### Blandtemperatur och verkningsgrad (metod)

Utan elmätare kan man inte räkna fabrikens **elektriska COP**. Däremot ger energibalans på luftsida en användbar proxy:

1. **Kyleffekt** (relativ): \(Q_c \propto \dot m_c\, c_p\, \Delta T_c\)
2. **Värmeavgivning**: \(Q_h \propto \dot m_h\, c_p\, \Delta T_h\)
3. **Energibalans**: \(Q_h \approx Q_c + W_{kompressor}\)
4. Med **ungefär lika massflöde** (\(\dot m_c \approx \dot m_h\)):

\[
\mathrm{COP_{proxy}} \approx \frac{\Delta T_c}{\Delta T_h - \Delta T_c},\quad
\eta \approx \frac{\Delta T_c}{\Delta T_h}
\]

**Varför blanda varmutblåset?** En punktsensor i en stratifierad slang underskattar/överskattar \(\Delta T_h\). Genom att mäta en **representativ blandtemperatur** i utblåset blir \(Q_h\) mer trovärdig, och därmed η och COP-proxy.

Begränsningar:

- Inte samma sak som märk-COP (kräver eleffekt och ofta fukt/latenthänsyn).
- Olika massflöde kallt/varmt (t.ex. enkel-slang AC) ger bias — sidobalans hjälper att upptäcka det.
- Värden är **N/A** tills båda sidor har giltiga mätningar.

---

## Inställningar: vilka tillval finns?

Under **Inställningar → Mobil AC** (inte under Styrning-fliken):

| UI-namn | API | Roll |
|---------|-----|------|
| **Sensorövervakning** | `sensing` | Alltid på – mätning + nyckeltal |
| **IR-fjärr** | `ir_remote` | IR till AC-aggregatet (stub tills TX) |
| **Linjestyrning** | `line_control` | Relä/torrkontakt (stub tills GPIO) |
| **Hjälpfläktar** | `assist_fans` | PWM-utgångar GPIO10/11 + ev. tach |

**Aktivering sker här.** Fliken Styrning har ingen separat “på/av”-master.

`PUT /api/device/profile`:

```json
{
  "ac_modules": {
    "ir_remote": true,
    "line_control": false,
    "assist_fans": true
  }
}
```

---

## Fliken Mobil AC

### Överblick

- Fyra mätpunkter (in/ut kall, in/ut varm)
- Graf: alla fyra temperaturer + kyllyft
- Nyckeltal enligt tabellen ovan

### Styrning

Visas när minst ett av IR / linje / hjälpfläktar är valt.

#### 1. Statusrad (live)

- **Reglerläge** – manuell / automatisk  
- **Kondensrisk**  
- **Policy vid risk**  
- **Kylindex**

#### 2. Driftpolicy

| Fält | API | Betydelse |
|------|-----|-----------|
| Fläktreglering | `fan_mode`: `manual` \| `auto` | Manuell PWM vs fuktstyrd auto |
| Vid kondensrisk | `ac_cond_action` | Se nästa avsnitt |

#### 3. Hjälpfläktar (om tillval)

Per fläkt: börvärde (PWM %), RPM om tach, status. Auto baseras på fukt i kalluft.

#### 4. Fjärrkommando (IR/linje)

| Knapp | Kommando (API) | Avsikt |
|-------|----------------|--------|
| Ström växla | `power` | På/av AC |
| Kylläge | `cool` | Aktiv kyla |
| Fläktläge | `fan_only` | Bara fläkt (mindre kondensrisk) |

---

## Kondensrisk – åtgärder

Klassning (kalluft / supply):

| Nivå | Tumregel |
|------|----------|
| Låg | RH &lt; 70 % eller T &gt; 18 °C |
| Medel | RH ≥ 70 % och T ≤ 18 °C |
| Hög | RH ≥ 85 % och T ≤ 16 °C |

### Policy `ac_cond_action`

| Värde | UI-namn | Firmware-beteende |
|-------|---------|-------------------|
| `observe` | Endast övervaka | Ingen extra fläkt; risk syns i UI/logg |
| `boost_assist` | Förstärk hjälpfläktar | Tvingar hjälpfläkt ≥ ~85 % vid risk |
| `request_fan_only` | Begär fläktläge på AC | Sätter `ac_last_command=fan_only_request` |

---

## API (styrrelevant)

```json
{
  "control": {
    "fan_mode": "auto",
    "fan1_speed": 40,
    "fan2_speed": 40,
    "ac_modules": {
      "sensing": true,
      "ir_remote": false,
      "line_control": false,
      "assist_fans": true
    },
    "ac_cond_action": "boost_assist",
    "ac_last_command": "",
    "assist_fans_status": [
      { "id": 1, "setpoint_pct": 45, "rpm": 0, "fault": false },
      { "id": 2, "setpoint_pct": 45, "rpm": 0, "fault": false }
    ]
  }
}
```

Sensor-API (`GET /api/ftx`, `GET /api/ftx/sensors`) skickar `null` för ogiltiga kanaler plus:

```json
"valid": { "supply": true, "extract": true, "exhaust": false, "outdoor": true }
```

(`outdoor` = varmsida intag i Mobil AC; uteluft i FTX/HX.)

---

## Kodplatser

| Del | Fil |
|-----|-----|
| Moduler + policy NVS | `components/device_profile/` |
| Sensor-slots + simulering | `components/sensor_manager/` |
| Valid-flaggor i FTX-struct | `components/heat_recovery/` |
| Auto/manuell AC-fläkt | `main/main.c` → `apply_fan_policy` |
| API (null för saknade sensorer) | `components/web_server/web_server.c` |
| UI Överblick/graf/nyckeltal | `components/web_server/web/*` |

---

## Hjälp i gränssnittet

| | |
|--|--|
| Hover **?** | Kort tooltip |
| Klick **?** | Längre text + länk till denna fil på GitHub |

Håll definitioner synkade: ändra formler här **och** i `script.js` (`computeAcMetrics` / `HELP_CATALOG`).
