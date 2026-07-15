# Mobil AC – övervakning och styrning

**Version:** 1.1  
**Senast uppdaterad:** 2026-07-15  
**Läge-ID:** `ac_monitor`

Webbgränssnittets **?**-ikoner använder samma definitioner som denna sida.  
Översikt över alla lägen: [APPLICATION_MODES.md](APPLICATION_MODES.md).

---

## Syfte

ThermoFlow kopplas till en **portabel luftkonditionering** med temperatursensorer på:

| Sensorroll (API) | Visningsnamn | Placering |
|------------------|--------------|-----------|
| `supply` | **Kylutblås** | Kall luft in i rummet |
| `exhaust` | **Kondensorutblås** | Varm avluft / slang ut |
| `outdoor` | **Rumsluft** | Referens i rummet |
| `extract` | **Extra mätpunkt** | Valfri |

Målet är att **se hur AC:n presterar** och, om tillval är på, **styra stödaktuatorer** med tydlig policy.

---

## Inspiration från liknande projekt

| Projekt / mönster | Vad vi tar med |
|-------------------|----------------|
| [ESPHome PWM fan + tach](https://github.com/patrickcollins12/esphome-fan-controller) | Separat **börvärde (PWM %)** och **uppmätt RPM**; fel om RPM uteblir |
| [HA/ESP32 fan controllers](https://community.home-assistant.io/t/pwm-fan-controller/433316) | **Manuell** vs **temp/fukt-auto**; live status i UI |
| HVAC “fault response” | Vid risk: **observera**, **förstärk flöde**, eller **mildra kylning** – inte bara en röd lampa |

Vi kopierar inte HA/ESPHome rakt av, men samma uppdelning: *policy → signal → feedback*.

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

Nyckeltal (beräknas i UI från sensorer; samma formler som nedan):

| Nyckeltal | Formel / tumregel |
|-----------|-------------------|
| **Kyllyft** | \(T_{rum} - T_{kyl}\) |
| **Värmeavgivning** | \(T_{varm} - T_{rum}\) |
| **Sidobalans** | värmeavgivning / kyllyft (om kyllyft > 0,5 °C) |
| **Kondensrisk** | Låg / medel / hög från RH + T på kylutblås |
| **Kylindex** | 0–100, normaliserat från kyllyft (~12 °C → 100) |
| **Fukt i kylutblås** | `supply_rh` |

### Styrning

Visas när minst ett av IR / linje / hjälpfläktar är valt.

#### 1. Statusrad (live)

- **Reglerläge** – manuell / automatisk  
- **Kondensrisk** – samma klassning som Överblick  
- **Policy vid risk** – se nedan  
- **Kylindex** – snabb “hur hårt kyler den?”

#### 2. Driftpolicy

| Fält | API | Betydelse |
|------|-----|-----------|
| Fläktreglering | `fan_mode`: `manual` \| `auto` | Manuell PWM vs fuktstyrd auto |
| Vid kondensrisk | `ac_cond_action` | Se nästa avsnitt |

#### 3. Hjälpfläktar (om tillval)

Per fläkt:

| Visning | Källa |
|---------|--------|
| **Börvärde** | `control.assist_fans_status[].setpoint_pct` (PWM som skickas) |
| **RPM** | `...rpm` om tach är inkopplad, annars — |
| **Status** | Fel / ingen feedback / OK |
| Manuell slider | Endast i **manuellt** reglerläge → `fan1_speed` / `fan2_speed` |

**Auto (firmware):** bashastighet ökar med fukt i kalluft; typiskt 20–70 %.  
**4-pin fläkt:** tach (öppen kollektor) + pull-up till 3,3 V; ESP32 pulse counter (som ESPHome). Utan tach visas RPM som saknad – det är förväntat för 3-pin.

#### 4. Fjärrkommando (IR/linje)

| Knapp | Kommando (API) | Avsikt |
|-------|----------------|--------|
| Ström växla | `power` | På/av AC |
| Kylläge | `cool` | Aktiv kyla |
| Fläktläge | `fan_only` | Bara fläkt (mindre kondensrisk) |

Kommandon sparas i `ac_last_command` och loggas. Fysisk IR/relä kommer senare.

---

## Kondensrisk – åtgärder

Klassning (kalluft):

| Nivå | Tumregel |
|------|----------|
| Låg | RH &lt; 70 % eller T &gt; 18 °C |
| Medel | RH ≥ 70 % och T ≤ 18 °C |
| Hög | RH ≥ 85 % och T ≤ 16 °C |

### Policy `ac_cond_action`

| Värde | UI-namn | Firmware-beteende |
|-------|---------|-------------------|
| `observe` | Endast övervaka | Ingen extra fläkt; risk syns i UI/logg |
| `boost_assist` | Förstärk hjälpfläktar | Tvingar hjälpfläkt ≥ ~85 % vid risk (kräver `assist_fans`) |
| `request_fan_only` | Begär fläktläge på AC | Sätter `ac_last_command=fan_only_request` (+ höjer assist lite); IR/linje stub |

Varför “förstärk fläktar” och inte bara stänga?  
För portabel AC sitter kondens ofta i **slang/utblås**. Ökat stödflöde kan hjälpa fukttransport; att stänga fläkt kan förvärra stillastående fukt. (Jämför med “increase exhaust on high humidity” i många rack/DIY-fläktprojekt.)

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

`POST /api/ftx/control`:

| command | Effekt |
|---------|--------|
| `fan1` / `fan2` | Manuellt börvärde |
| `mode` + `mode: auto\|manual` | Reglerläge |
| `power` / `cool` / `fan_only` | Fjärravsikt |
| `ac_cond_action` + `mode` | Kondenspolicy |

---

## Kodplatser

| Del | Fil |
|-----|-----|
| Moduler + policy NVS | `components/device_profile/` |
| Auto/manuell AC-fläkt + kondensoverride | `main/main.c` → `apply_fan_policy` |
| API control + fan status | `components/web_server/web_server.c` |
| UI Överblick/Styrning + hjälp | `components/web_server/web/*` |
| PWM / status / framtida tach | `components/fan_control/` |

---

## Hjälp i gränssnittet

| | |
|--|--|
| Hover **?** | Kort tooltip |
| Klick **?** | Längre text + länk till denna fil på GitHub |

Håll definitioner synkade: ändra formler här **och** i `script.js` (`computeAcMetrics` / `HELP_CATALOG`).
