# ThermoFlow – applikationslägen (modes)

**Version:** 1.0  
**Senast uppdaterad:** 2026-07-15

Varje enhet kör **ett aktivt läge** (NVS). Läge styr UI, sensorroller, vilka aktuatorer som är relevanta och om styrning får köras. Systemet ska alltid kunna **mäta** även när styrning är avstängd.

---

## Översikt

| ID (API) | Visningsnamn | Huvudidé | Fläktar | Styrning |
|----------|--------------|----------|---------|----------|
| `mini_ftx` | **Mini-FTX** | Regenerativ värmeåtervinning via keramiskt element; flöde växelvis in/ut | 1 (växlande riktning) | PWM-fläkt, cykel (in/ut), auto/manuell |
| `heat_exchanger` | **Värmeväxlare** | Kontinuerligt tilluft + frånluft samtidigt | 2 (oberoende in/ut) | Separat hastighet per fläkt, auto/manuell |
| `ac_monitor` | **Mobil AC** | Övervaka kall- och varmsida på portabel AC | 0 (normalt) | Valfri: IR och/eller elektrisk signal |
| `sensor_only` | **Endast sensorer** | Ren mätning/loggning | 0 | Ingen |

---

## 1. Mini-FTX (`mini_ftx`)

### Fysik / användning
- Ett **keramiskt lagringselement** tar upp värme från frånluft och ger tillbaka den när flödet vänder.
- **En fläkt** (eller fläkt + spjäll) kör i **cykler**: period ut (laddning) → period in (urladdning).
- Mål: ventilation med värmeåtervinning i enkanaliga/DIY-FTX-lösningar.

### Sensorroller (typiskt 2–4 SHT)
| Roll | Betydelse |
|------|-----------|
| `outdoor` | Utomhus / friskluft |
| `supply` | Tilluft (in i rum) |
| `extract` | Frånluft (ut ur rum) |
| `exhaust` | Avluft (ut efter kärna), om mätt |

### Styrning
| Parameter | Beskrivning |
|-----------|-------------|
| `control_enabled` | Master: av = endast mätning |
| `fan_mode` | `off` / `manual` / `auto` |
| `fan1_speed` | Hastighet 0–100 % (manuell) |
| `cycle_period_s` | Cykelperiod in+ut (auto; UI kan visa, firmware kan utöka) |
| `phase` | Aktuell fas: `intake` / `exhaust` (status) |

### UI-vyer
Dashboard · Sensorer · **Styrning (FTX)** · Logg · Inställningar  

Visa: värmeåtervinnings-% , frostskydd, cykelfas, en fläktreglage.

### Implementeringsstatus
| Del | Status |
|-----|--------|
| Sensorer + efficiency-beräkning | ✅ |
| Auto fläkthastighet (FTX-rekommendation) | ✅ (enkel modell) |
| Växlande riktning / cykelfas | ⏳ UI+API-status; motorik/spjäll senare |
| `control_enabled` | ✅ |

---

## 2. Värmeväxlare (`heat_exchanger`)

### Fysik / användning
- **Kontinuerligt** flöde: samtidig tilluft och frånluft (två kanaler eller två fläktar).
- DIY-värmeväxlare, korsflödeskärna m.m. – **inte** regenerativ cykel.

### Sensorroller
| Roll | Betydelse |
|------|-----------|
| `outdoor` / intag | Uteluft in i växlaren |
| `supply` | Tilluft till rum |
| `extract` | Frånluft från rum |
| `exhaust` | Avluft ut |

### Styrning
| Parameter | Beskrivning |
|-----------|-------------|
| `control_enabled` | Master av/på |
| `fan_mode` | `off` / `manual` / `auto` |
| `fan1_speed` | Tilluft / fläkt 1 (GPIO10) |
| `fan2_speed` | Frånluft / fläkt 2 (GPIO11) – **oberoende** |

### UI-vyer
Dashboard · Sensorer · **Fläktar** · Logg · Inställningar  

Visa: två oberoende sliders, kondenseringsskydd, ingen regenerativ cykel.

### Implementeringsstatus
| Del | Status |
|-----|--------|
| Dubbla PWM-fläktar | ✅ hårdvara |
| Oberoende hastighet i policy | ✅ när `control_enabled` |
| Auto baserat på temperatur | ✅ enkel modell |

---

## 3. Mobil AC (`ac_monitor`)

Se detaljerad guide: **[MOBILE_AC.md](MOBILE_AC.md)** (samma texter som UI-hjälp).

### Fysik / användning
- Portabel AC: mät **kylutblås** och **kondensorutblås**, plus **rumsluft**.
- Primärt **övervakning**; tillval aktiveras under Inställningar.

### Tillval (Inställningar)
| Modul | UI-namn | Beskrivning |
|-------|---------|-------------|
| sensing | Sensorövervakning | Alltid på |
| ir_remote | IR-fjärr | Infraröd fjärr (stub) |
| line_control | Linjestyrning | Relä/torrkontakt (stub) |
| assist_fans | Hjälpfläktar | PWM-stödfläktar |

### Sensorroller
| Roll | Visningsnamn |
|------|----------------|
| `supply` | Kylutblås |
| `exhaust` | Kondensorutblås |
| `outdoor` | Rumsluft |
| `extract` | Extra mätpunkt |

### Nyckeltal (Överblick)
Kyllyft, värmeavgivning, sidobalans, kondensrisk, kylindex, fukt i kylutblås — se MOBILE_AC.md.

### UI-vyer
Dashboard · Sensorer · **Mobil AC** (Överblick + ev. Styrning) · Logg · Inställningar

---

## 4. Endast sensorer (`sensor_only`)

- Inga fläktar, ingen aktuator-UI.
- Dashboard + sensorer + logg + inställningar.
- `control_enabled` ignoreras (alltid av).

---

## Gemensam API-modell

### `GET /api/device/profile` / fält i `/api/device/info`

```json
{
  "application_profile": "heat_exchanger",
  "application_profile_label": "Värmeväxlare",
  "application_profile_description": "...",
  "control": {
    "enabled": true,
    "method": "pwm",
    "fan_mode": "auto",
    "fan1_speed": 40,
    "fan2_speed": 40,
    "cycle_phase": "idle",
    "cycle_period_s": 60
  },
  "capabilities": {
    "views": ["dashboard", "sensors", "control", "logs", "settings"],
    "control_nav_label": "Fläktar",
    "fans": { "count": 2, "independent": true, "roles": ["supply", "exhaust"] },
    "sensors": [
      { "role": "supply", "label": "Tilluft" },
      { "role": "exhaust", "label": "Frånluft/avluft" }
    ],
    "features": {
      "heat_recovery_stats": false,
      "alternating_cycle": false,
      "dual_fan": true,
      "ir_control": false,
      "electrical_control": false,
      "control_optional": true
    }
  },
  "available_profiles": [ ... ]
}
```

### `PUT /api/device/profile`

```json
{
  "profile": "mini_ftx",
  "control_enabled": true,
  "control_method": "pwm",
  "fan_mode": "manual",
  "fan1_speed": 55,
  "fan2_speed": 45
}
```

Alla fält utom `profile` är valfria. Okända metoder/hastigheter valideras.

### `POST /api/ftx/control` (bakåtkompatibel)

Fortsätter acceptera `{ "command", "value" }` och mappar till samma control-state där det går.

---

## Stabilitet / säkerhet

1. **Fail-safe:** vid okänd fault eller avstängd styrning → fläktar 0 % (utom ev. SR-010 policy som kan kräva max flöde i FTX).
2. **Kondenseringsskydd (SR-010):** aktivt i lägen med fläktstyrning.
3. **Stubs (IR/el):** sparas i NVS och visas i UI men skickar inte hårdvarukommandon förrän drivrutin finns — loggas som audit `CONFIG_CHANGE`.

---

## Namngivning (svenska i UI)

Använd alltid:

- **Mini-FTX** (inte bara “FTX” i inställningar – nav kan säga “FTX”)
- **Värmeväxlare**
- **Mobil AC**
- **Endast sensorer**

API-id:n (`mini_ftx`, …) är stabila; byt inte utan migrering.
