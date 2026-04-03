# Mini-FTX Extension for ThermoFlow

## Översikt

Denna utökning lägger till stöd för **mini-FTX** (Frånluftsventilation med värmeåtervinning) i ThermoFlow-systemet.

### Vad är Mini-FTX?

En kompakt värmepump/luftväxlare för hemmafixare som vill:
- Återvinna värme från frånluften (upp till 85%)
- Minska uppvärmningskostnader
- Behålla bra inomhusluft
- Bygga själv med standardkomponenter

---

## Hårdvarukonfiguration

### Standard FTX-uppsättning

```
Uteluft (kall)          Frånluft (varm, fuktig)
      │                        │
      ▼                        ▼
┌─────────────┐          ┌─────────────┐
│   Filter    │          │   Filter    │
└─────────────┘          └─────────────┘
      │                        │
      ▼                        ▼
┌─────────────┐          ┌─────────────┐
│   Fläkt 1   │◄────────►│   Fläkt 2   │
│  (Tilluft)  │          │ (Frånluft)  │
└─────────────┘          └─────────────┘
      │                        │
      ▼                        ▼
┌─────────────────────────────────────┐
│      VÄRMEVÄXLARE (kärna)           │
│    Motströmsvärmeväxlare            │
│   (Plast eller aluminiumplåt)      │
└─────────────────────────────────────┘
      │                        │
      ▼                        ▼
┌─────────────┐          ┌─────────────┐
│  Tilluft    │          │   Avluft    │
│  (värmd)    │          │  (avkylld)  │
└─────────────┘          └─────────────┘
      │                        │
      ▼                        ▼
   Inomhus                  Utomhus
```

---

## Sensorplacering

| Sensor | Placering | Syfte |
|--------|-----------|-------|
| SHT40 #1 | Uteluft (före filter) | Ute-temp/fukt |
| SHT40 #2 | Tilluft (efter värmeväxlare) | Verifiera värmeåtervinning |
| SHT40 #3 | Frånluft (före värmeväxlare) | Rumstemperatur |
| SHT40 #4 | Avluft (efter värmeväxlare) | Verifiera effektivitet |

---

## Nyckelberäkningar

### 1. Värmeåtervinningseffektivitet

```
η = (T_tilluft - T_uteluft) / (T_frånluft - T_uteluft) × 100%
```

**Exempel:**
- Uteluft: 0°C
- Frånluft: 22°C  
- Tilluft: 18.7°C

```
η = (18.7 - 0) / (22 - 0) × 100% = 85%
```

### 2. Energiåtervinning (Watt)

```
P = ṁ × Cp × ΔT

Där:
ṁ = luftflöde [kg/s] = (m³/h × 1.2 kg/m³) / 3600
Cp = 1005 J/(kg·K) (luftens specifika värmekapacitet)
ΔT = temperaturskillnad
```

**Exempel:**
- Luftflöde: 150 m³/h
- ΔT: 22°C - 18.7°C = 3.3°C
- Effektivitet: 85%

```
ṁ = (150 × 1.2) / 3600 = 0.05 kg/s
P = 0.05 × 1005 × 22 × 0.85 = 940 W
```

### 3. Daglig energibesparing

```
Energibesparing [kWh/dag] = (P × 24) / 1000

Exempel: 940 W × 24 / 1000 = 22.6 kWh/dag
```

---

## Programvaruarkitektur

### Nya komponenter

```
components/
├── heat_recovery/       # NY - Värmeåtervinningsberäkningar
│   ├── include/
│   │   └── heat_recovery.h
│   ├── src/
│   │   └── heat_recovery.c
│   └── CMakeLists.txt
├── bypass_control/      # NY - Sommar/vinter-läge
├── filter_monitor/      # NY - Filterpåtryck
└── [befintliga komponenter...]
```

### Integration med befintlig kod

**Fil: main/main.c**

```c
#include "heat_recovery.h"

// I huvudloopen:
ftx_data_t ftx_data;

// Läs sensorer
ftx_data.outdoor_temp = sht40_read_temp(SENSOR_OUTDOOR);
ftx_data.supply_temp = sht40_read_temp(SENSOR_SUPPLY);
ftx_data.exhaust_temp = sht40_read_temp(SENSOR_EXHAUST);

// Beräkna effektivitet
ftx_data.efficiency = ftx_calc_efficiency(
    ftx_data.outdoor_temp,
    ftx_data.supply_temp, 
    ftx_data.exhaust_temp
);

// Beräkna återvunnen effekt
ftx_data.power_recovered = ftx_calc_power(
    ftx_data.airflow,
    ftx_data.exhaust_temp - ftx_data.outdoor_temp
);

// Kontrollera frost-risk
ftx_data.frost_risk = ftx_check_frost(
    ftx_data.outdoor_temp,
    ftx_data.exhaust_rh
);

// Justera fläkthastighet
int new_speed = ftx_recommend_speed(&ftx_data);
fan_control_set_speed(FAN_SUPPLY, new_speed);
fan_control_set_speed(FAN_EXHAUST, new_speed);
```

---

## MQTT API för Home Assistant

### Topics

**Status:**
```json
// thermoflow/ftx/status
{
  "outdoor_temp": 0.5,
  "outdoor_rh": 65,
  "supply_temp": 18.7,
  "supply_rh": 42,
  "exhaust_temp": 22.0,
  "exhaust_rh": 48,
  "extract_temp": 21.5,
  "efficiency": 84.7,
  "power_recovered": 942,
  "airflow": 150,
  "frost_risk": false,
  "bypass_active": false,
  "filter_status": "ok"
}
```

**Statistik (daglig):**
```json
// thermoflow/ftx/stats/daily
{
  "date": "2026-04-03",
  "energy_recovered_kwh": 22.6,
  "energy_saving_sek": 45.2,
  "avg_efficiency": 83.2,
  "runtime_hours": 24,
  "filter_hours_remaining": 2160
}
```

---

## Web Interface

### Ny vy: FTX Dashboard

```
┌─────────────────────────────────────────┐
│  🏠 Mini-FTX Dashboard                  │
├─────────────────────────────────────────┤
│                                         │
│  🌡️ Temperaturer          💧 Fukt       │
│  ─────────────────────────────────────  │
│  Ute:    +0.5°C           65% RH        │
│  Tilluft: +18.7°C         42% RH        │
│  Rum:     +22.0°C         48% RH        │
│                                         │
│  ⚡ Värmeåtervinning                   │
│  ─────────────────────────────────────  │
│  Effektivitet:     84.7%  ████████░    │
│  Återvunnen effekt: 942 W              │
│  Daglig besparing:  22.6 kWh           │
│  Kostnadsbesparing: 45 kr/dag          │
│                                         │
│  🌀 Luftflöde: 150 m³/h    [▲] [▼]     │
│                                         │
│  [Auto] [Sommar] [Vinter] [Stäng av]   │
│                                         │
└─────────────────────────────────────────┘
```

---

## Förutsättningar för Byggnation

### Minimal FTX (DIY)

| Del | Kostnad | Var köpa |
|-----|---------|----------|
| Värmeväxlarkärna | 800-1500 kr | Byggmax, Daikin |
| Fläktar 120mm PWM | 2×200 kr | Kjell&Company |
| SHT40 sensorer | 4×150 kr | Electrokit |
| ESP32-S3 | 150 kr | AliExpress |
| Filter (G4) | 100 kr | Bauhaus |
| Kanal 125mm | 500 kr | Byggmax |
| **Totalt** | **~3000 kr** | |

### Prestanda

| Parameter | Värde |
|-----------|-------|
| Luftflöde | 50-200 m³/h |
| Värmeåtervinning | 75-90% |
| Ljudnivå | 25-40 dB |
| Elförbrukning | 5-20 W |
| Årlig besparing | 2000-4000 kr |

---

## Bygginstruktioner (Sammanfattning)

1. **Bygg kanal-system**
   - Tilluft: Utomhus → Filter → Fläkt → Värmeväxlare → Inomhus
   - Frånluft: Inomhus → Filter → Fläkt → Värmeväxlare → Utomhus

2. **Montera värmeväxlare**
   - Motströms är viktigt!
   - Tätning med silikontätning

3. **Installera ESP32**
   - Koppla sensorer (I2C)
   - Koppla fläktar (PWM)
   - Lägg in i vattentätt kabinett

4. **Konfigurera programvara**
   - Flasha ThermoFlow-FTX
   - Ställ in fläktstorlek (max m³/h)
   - Justera värmeväxlareffektivitet

5. **Anslut till Home Assistant**
   - MQTT-broker inställningar
   - Dashboard-konfiguration

---

## Felsökning

### Problem: Låg effektivitet (<70%)

**Orsaker:**
- Läckage i kanaler
- Felaktigt monterad värmeväxlare
- Filter är smutsiga

**Lösning:**
- Kontrollera täthet
- Verifiera motströms (inte korsströms)
- Byt filter

### Problem: Kondens i värmeväxlare

**Orsak:**
- För hög luftfuktighet
- För låg utetemperatur

**Lösning:**
- Minska luftflöde vid risk
- Aktivera frostskydd (föruppvärmning)
- Kontrollera att drain fungerar

### Problem: Oljud från fläktar

**Orsaker:**
- Fläktarna vibrerar mot hölje
- Ojämn spänning till PWM

**Lösning:**
- Använd vibrationsdämpare
- Lägg till kondensator på fläktmatning
- Sänk max RPM i mjukvara

---

## Utvecklingsplan

### Fas 1: Grundläggande FTX (Denna PR)
- [x] Värmeåtervinningsberäkningar
- [ ] MQTT-topics för FTX
- [ ] Webb-UI vy
- [ ] Dokumentation

### Fas 2: Avancerad styrning (Framtida)
- [ ] Bypass-ventil styrning
- [ ] Filtertrycksövervakning
- [ ] CO2-sensor integration
- [ ] Maskininlärning för optimal drift

### Fas 3: Kommersiell version (Valfritt)
- [ ] PCB-design
- [ ] CE-märkning
- [ ] Monteringskit

---

## Referenser

- [SP Sveriges Tekniska Forskningsinstitut - FTX](https://www.sp.se)
- [Boverkets byggregler](https://www.boverket.se)
- [Home Assistant MQTT Climate](https://www.home-assistant.io/integrations/climate.mqtt/)

---

**Författare:** Ola Andersson  
**Version:** 1.0.0  
**Datum:** 2026-04-03
