# ThermoFlow Logging Architecture

**Senast uppdaterad:** 2026-07-14

ThermoFlow uses a **unified multi-sink logging hub** (`log_manager`) aligned with modern observability practice: structured fields, categories, correlation IDs, and pluggable outputs.

---

## Architecture

```
                    ┌─────────────────────────────────┐
  Components        │         log_manager             │
  TF_LOG_*()   ───► │  ring buffer · filters · stats  │
  audit_log_*() ──► │  boot_id · correlation_id       │
  ESP_LOG* (drv)    └───────────┬─────────────────────┘
                                │
        ┌───────────┬───────────┼───────────┬──────────┐
        ▼           ▼           ▼           ▼          ▼
     Serial       Web API      NVS        MQTT        SD
     (UART)    /api/logs    (persist)  thermoflow/  (stub)
               export NDJSON            logs
```

| Layer | Purpose |
|-------|---------|
| **ESP_LOG** | Low-level driver/debug output (unchanged) |
| **log_manager** | Structured operational + security logs |
| **audit_log** | IEC 62443 SR-005 facade over `log_manager` |

---

## Log entry format (structured JSON v1)

Each entry contains:

| Field | Description |
|-------|-------------|
| `boot_id` | Unique ID per boot session |
| `seq` | Monotonic sequence number |
| `cid` | Correlation ID (optional trace grouping) |
| `ts` | Timestamp (µs since boot) |
| `lvl` | TRACE / DEBUG / INFO / WARN / ERROR / FATAL |
| `cat` | system, security, network, sensor, fan, mqtt, web, ota, audit |
| `cmp` | Source component tag (e.g. `WIFI_MGR`) |
| `msg` | Human-readable message |

Audit/security events also map to `event` types (BOOT, CONFIG_CHANGE, RATE_LIMIT, …).

---

## Sinks

| Sink | Default | Description |
|------|---------|-------------|
| **serial** | ✅ | Human-readable or JSON lines on UART (`idf.py monitor`) |
| **web** | ✅ | Ring buffer exposed via REST + Logg-tab |
| **nvs** | ✅ | Last 32 entries restored after reboot |
| **mqtt** | ⚪ | NDJSON to `thermoflow/logs` when broker connected |
| **sd** | ⚪ | Stub for future `LOG_FILE_PATH` rotation |

Configure sinks via web UI (Logg → Konfiguration) or `PUT /api/logs/config`.

---

## API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/logs` | GET | Recent entries (structured JSON v1) |
| `/api/logs` | DELETE | Clear buffer + NVS log partition |
| `/api/logs/config` | GET | Current log configuration |
| `/api/logs/config` | PUT | Update level, sinks, serial JSON mode |
| `/api/logs/export?format=ndjson` | GET | Download NDJSON (or `format=json`) |

### Example `GET /api/logs`

```json
{
  "logs": [{
    "sequence": 12,
    "boot_id": 2847193021,
    "correlation_id": 0,
    "severity": "INFO",
    "category": "network",
    "component": "WIFI_MGR",
    "event": "LOG",
    "message": "WiFi connected, IP 192.168.32.241",
    "time": "+00:01:05",
    "age_s": 3.2
  }],
  "count": 1,
  "capacity": 100,
  "sinks": ["serial", "web", "nvs"],
  "format": "structured_json_v1"
}
```

---

## Usage in code

### Structured logging (preferred)

```c
#include "log_manager.h"

TF_LOG_INFO(TF_LOG_CAT_NETWORK, "WIFI_MGR", "Connected to %s", ssid);
TF_LOG_WARN(TF_LOG_CAT_SENSOR, "SENSOR_MGR", "Read failed: %s", esp_err_to_name(err));
```

### Audit / security events (SR-005)

```c
#include "audit_log.h"

audit_log_event(AUDIT_EVENT_CONFIG_CHANGE, AUDIT_SEVERITY_INFO,
                "WiFi credentials updated for SSID %s", ssid);
```

### Correlation IDs

```c
uint32_t cid = log_manager_new_correlation_id();
// Pass cid through related operations for trace grouping
```

---

## Serial logging

Default: `[category][level] message` via component tag.

Enable structured JSON on UART:

- Web UI: Logg → Konfiguration → **Serial JSON**
- API: `PUT /api/logs/config` with `"serial_json": true`

Capture with `serial-capture.ps1` or `idf.py monitor`.

---

## Web UI

**Logg** tab shows: time, level, category, source component, event type, message.

Actions:
- **Uppdatera** — refresh buffer
- **Exportera** — download NDJSON
- **Konfiguration** — sinks and log level
- **Rensa** — clear buffer

---

## MQTT remote logging

When `MQTT_BROKER_DEFAULT` is configured and connected, call `mqtt_ftx_register_log_sink()` (done automatically in `main.c`).

Subscribes/publishes to topic: `thermoflow/logs` (one NDJSON line per message).

---

## NVS persistence

Namespace: `tf_log`

Survives reboot and `app-flash`. Cleared on `DELETE /api/logs` or full `erase-flash`.

---

## Future: SD card sink

`LOG_FILE_PATH`, `LOG_ROTATION_SIZE`, `LOG_MAX_FILES` in `thermoflow_config.h` are reserved for rotating file logs on MicroSD.

---

## Documentation policy

When changing logging behaviour, update:
- [LOGGING.md](LOGGING.md)
- [CHANGELOG.md](../CHANGELOG.md)
- [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)