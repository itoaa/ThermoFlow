# MQTT API for Mini-FTX

## Topic Structure

```
thermoflow/ftx/
├── status              # Current status flags (JSON)
├── sensors             # Sensor readings (JSON)
├── efficiency          # Efficiency calculations (JSON)
├── energy              # Energy statistics (JSON)
├── stats/daily         # Daily statistics (JSON, retained)
├── state               # Full state (JSON)
├── alerts              # Alerts and warnings (JSON)
├── control/            # Control commands (sub-topics)
│   ├── fan_speed       # Set fan speed 0-100
│   ├── mode            # auto/manual/summer/winter
│   ├── reset_filter    # Reset filter timer
│   └── emergency_stop  # Emergency stop
└── config              # Device configuration
```

---

## Publish Topics (Device → MQTT)

### thermoflow/ftx/status

Status flags (published every 30s):

```json
{
  "frost_risk": false,
  "frost_protection_active": false,
  "bypass_active": false,
  "filter_warning": false,
  "filter_critical": false,
  "high_humidity_alert": false,
  "low_efficiency_alert": false
}
```

### thermoflow/ftx/sensors

Sensor readings (published every 10s):

```json
{
  "outdoor_temp": 0.5,
  "outdoor_rh": 65.0,
  "supply_temp": 18.7,
  "supply_rh": 42.0,
  "exhaust_temp": 22.0,
  "exhaust_rh": 48.0,
  "extract_temp": 21.5,
  "extract_rh": 50.0
}
```

### thermoflow/ftx/efficiency

Efficiency calculations (published every 60s):

```json
{
  "efficiency_percent": 84.7,
  "power_recovered_w": 942.3,
  "airflow_m3h": 150.0,
  "temp_diff_in_out": 18.2,
  "temp_diff_exhaust_supply": 3.3
}
```

### thermoflow/ftx/stats/daily

Daily statistics (published at midnight, retained):

```json
{
  "energy_kwh_day": 22.6,
  "cost_saving_sek": 45.2,
  "avg_efficiency": 83.2,
  "runtime_hours": 24,
  "filter_hours_remaining": 2160
}
```

### thermoflow/ftx/state

Complete state (published every 60s):

```json
{
  "timestamp": "2026-04-03T16:00:00Z",
  "sensors": {
    "outdoor_temp": 0.5,
    "outdoor_rh": 65.0,
    "supply_temp": 18.7,
    "supply_rh": 42.0,
    "exhaust_temp": 22.0,
    "exhaust_rh": 48.0
  },
  "efficiency": {
    "percent": 84.7,
    "power_w": 942.3,
    "airflow_m3h": 150.0
  },
  "status": {
    "frost_risk": false,
    "bypass": false,
    "filter_warning": false
  }
}
```

### thermoflow/ftx/alerts

Alerts (published on event, QoS 2):

```json
{
  "type": "frost_risk",
  "message": "Outdoor temperature below 2°C with high humidity",
  "timestamp": 1712150400
}
```

**Alert types:**
- `frost_risk` - Frost protection activated
- `filter_warning` - Filter needs cleaning
- `filter_critical` - Filter blocked
- `high_humidity` - Humidity >90%
- `low_efficiency` - Efficiency <70%
- `sensor_error` - Sensor communication failure
- `emergency_stop` - Emergency stop activated

---

## Subscribe Topics (MQTT → Device)

### thermoflow/ftx/control/fan_speed

Set fan speed percentage:

```
Payload: "75"  (0-100)
```

### thermoflow/ftx/control/mode

Set operating mode:

```
Payload: "auto"     # Automatic control
Payload: "manual"   # Manual fan speed
Payload: "summer"   # Summer mode (bypass active)
Payload: "winter"   # Winter mode (heat recovery)
```

### thermoflow/ftx/control/reset_filter

Reset filter timer:

```
Payload: "1"
```

### thermoflow/ftx/control/emergency_stop

Emergency stop (fans OFF):

```
Payload: "1"
```

---

## Home Assistant Configuration

### MQTT Sensors (configuration.yaml)

```yaml
mqtt:
  sensor:
    # Temperature sensors
    - name: "FTX Outdoor Temperature"
      unique_id: thermoflow_ftx_outdoor_temp
      state_topic: "thermoflow/ftx/sensors"
      unit_of_measurement: "°C"
      value_template: "{{ value_json.outdoor_temp }}"
      device_class: temperature
      
    - name: "FTX Supply Temperature"
      unique_id: thermoflow_ftx_supply_temp
      state_topic: "thermoflow/ftx/sensors"
      unit_of_measurement: "°C"
      value_template: "{{ value_json.supply_temp }}"
      device_class: temperature
      
    - name: "FTX Exhaust Temperature"
      unique_id: thermoflow_ftx_exhaust_temp
      state_topic: "thermoflow/ftx/sensors"
      unit_of_measurement: "°C"
      value_template: "{{ value_json.exhaust_temp }}"
      device_class: temperature

    # Humidity sensors
    - name: "FTX Outdoor Humidity"
      unique_id: thermoflow_ftx_outdoor_rh
      state_topic: "thermoflow/ftx/sensors"
      unit_of_measurement: "%"
      value_template: "{{ value_json.outdoor_rh }}"
      device_class: humidity
      
    - name: "FTX Supply Humidity"
      unique_id: thermoflow_ftx_supply_rh
      state_topic: "thermoflow/ftx/sensors"
      unit_of_measurement: "%"
      value_template: "{{ value_json.supply_rh }}"
      device_class: humidity

    # Efficiency
    - name: "FTX Efficiency"
      unique_id: thermoflow_ftx_efficiency
      state_topic: "thermoflow/ftx/efficiency"
      unit_of_measurement: "%"
      value_template: "{{ value_json.efficiency_percent }}"
      icon: mdi:gauge
      
    - name: "FTX Power Recovered"
      unique_id: thermoflow_ftx_power
      state_topic: "thermoflow/ftx/efficiency"
      unit_of_measurement: "W"
      value_template: "{{ value_json.power_recovered_w }}"
      device_class: power
      
    - name: "FTX Airflow"
      unique_id: thermoflow_ftx_airflow
      state_topic: "thermoflow/ftx/efficiency"
      unit_of_measurement: "m³/h"
      value_template: "{{ value_json.airflow_m3h }}"
      icon: mdi:fan

    # Daily statistics
    - name: "FTX Daily Energy"
      unique_id: thermoflow_ftx_daily_energy
      state_topic: "thermoflow/ftx/stats/daily"
      unit_of_measurement: "kWh"
      value_template: "{{ value_json.energy_kwh_day }}"
      device_class: energy
      
    - name: "FTX Cost Saving"
      unique_id: thermoflow_ftx_cost_saving
      state_topic: "thermoflow/ftx/stats/daily"
      unit_of_measurement: "SEK"
      value_template: "{{ value_json.cost_saving_sek }}"
      icon: mdi:cash

  # Binary sensors for status
  binary_sensor:
    - name: "FTX Frost Risk"
      unique_id: thermoflow_ftx_frost_risk
      state_topic: "thermoflow/ftx/status"
      value_template: "{{ value_json.frost_risk }}"
      device_class: cold
      
    - name: "FTX Frost Protection"
      unique_id: thermoflow_ftx_frost_protection
      state_topic: "thermoflow/ftx/status"
      value_template: "{{ value_json.frost_protection_active }}"
      
    - name: "FTX Bypass"
      unique_id: thermoflow_ftx_bypass
      state_topic: "thermoflow/ftx/status"
      value_template: "{{ value_json.bypass_active }}"
      
    - name: "FTX Filter Warning"
      unique_id: thermoflow_ftx_filter_warning
      state_topic: "thermoflow/ftx/status"
      value_template: "{{ value_json.filter_warning }}"
      device_class: problem

  # Number entity for fan speed control
  number:
    - name: "FTX Fan Speed"
      unique_id: thermoflow_ftx_fan_speed
      command_topic: "thermoflow/ftx/control/fan_speed"
      state_topic: "thermoflow/ftx/efficiency"
      value_template: "{{ value_json.fan_speed | default(50) }}"
      min: 20
      max: 100
      step: 5
      unit_of_measurement: "%"
      icon: mdi:fan-speed-1

  # Select entity for mode control
  select:
    - name: "FTX Mode"
      unique_id: thermoflow_ftx_mode
      command_topic: "thermoflow/ftx/control/mode"
      state_topic: "thermoflow/ftx/state"
      value_template: "{{ value_json.mode | default('auto') }}"
      options:
        - "auto"
        - "manual"
        - "summer"
        - "winter"

  # Button for filter reset
  button:
    - name: "FTX Reset Filter"
      unique_id: thermoflow_ftx_reset_filter
      command_topic: "thermoflow/ftx/control/reset_filter"
      payload_press: "1"
      icon: mdi:air-filter

  # Switch for emergency stop
  switch:
    - name: "FTX Emergency Stop"
      unique_id: thermoflow_ftx_emergency
      command_topic: "thermoflow/ftx/control/emergency_stop"
      state_topic: "thermoflow/ftx/status"
      value_template: "{{ value_json.emergency_stop | default(false) }}"
      icon: mdi:alert-octagon
```

---

## Dashboard Card (Lovelace YAML)

```yaml
type: vertical-stack
cards:
  - type: entities
ntities:
      - entity: sensor.ftx_outdoor_temperature
        name: Outdoor
      - entity: sensor.ftx_supply_temperature
        name: Supply
      - entity: sensor.ftx_exhaust_temperature
        name: Exhaust
    title: FTX Temperatures

  - type: gauge
    entity: sensor.ftx_efficiency
    name: Heat Recovery
    min: 0
    max: 100
    severity:
      green: 80
      yellow: 60
      red: 40

  - type: entity
    entity: sensor.ftx_power_recovered
    name: Power Recovered
    unit: W

  - type: statistics-graph
    entities:
      - sensor.ftx_daily_energy
    days_to_show: 7
    stat_types:
      - sum
    title: Energy Savings (kWh/day)

  - type: button
    tap_action:
      action: call-service
      service: mqtt.publish
      service_data:
        topic: thermoflow/ftx/control/reset_filter
        payload: "1"
    name: Reset Filter Timer
    icon: mdi:air-filter
```

---

## QoS Levels

| Topic | QoS | Notes |
|-------|-----|-------|
| sensors | 1 | Important readings |
| status | 1 | Status flags |
| efficiency | 1 | Calculated values |
| stats/daily | 1 | Retained for persistence |
| alerts | 2 | Critical, must arrive |
| control/* | 1 | Commands |

---

## Update Intervals

| Data | Interval |
|------|----------|
| Sensors | 10 seconds |
| Status | 30 seconds |
| Efficiency | 60 seconds |
| Full state | 60 seconds |
| Daily stats | Once per day at midnight |
| Alerts | Immediate (on event) |

---

## Example: Python Script for Testing

```python
import paho.mqtt.client as mqtt
import json
import time

client = mqtt.Client()
client.connect("homeassistant.local", 1883)

# Publish sensor data
sensors = {
    "outdoor_temp": 0.5,
    "outdoor_rh": 65.0,
    "supply_temp": 18.7,
    "supply_rh": 42.0,
    "exhaust_temp": 22.0,
    "exhaust_rh": 48.0
}

client.publish("thermoflow/ftx/sensors", json.dumps(sensors))
client.disconnect()
```

---

## Troubleshooting

### No data in Home Assistant
1. Check MQTT broker connection
2. Verify topic names match exactly
3. Check value_template syntax
4. Enable MQTT debug logging in HA

### Delayed updates
- Home Assistant updates on state change
- Add `force_update: true` to sensors for frequent updates

### High CPU usage
- Reduce update frequency in code
- Use throttle in Home Assistant

---

## See Also

- [FTX_EXTENSION.md](FTX_EXTENSION.md) - Hardware and build guide
- [PROJECT_FRAMEWORK.md](../PROJECT_FRAMEWORK.md) - Security requirements
- [Home Assistant MQTT Documentation](https://www.home-assistant.io/integrations/mqtt/)
