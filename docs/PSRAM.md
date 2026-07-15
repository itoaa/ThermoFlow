# PSRAM policy (ThermoFlow)

## Goals

1. **Works without PSRAM** — boards without external RAM, or failed PSRAM init, still boot and run all features.
2. **Uses PSRAM only for non-critical bulk data** — never for DMA, WiFi control, task stacks, or crypto keys.
3. **No silent hijack of `malloc()`** — IDF is configured with capability allocator only so FreeRTOS/WiFi/LWIP keep using internal RAM unless they opt in.

## Configuration

| Setting | Value | Why |
|---------|--------|-----|
| `CONFIG_SPIRAM` | yes | Enable driver |
| `CONFIG_SPIRAM_MODE_OCT` | yes | ESP32-S3 modules with 8 MB embedded PSRAM (e.g. “Embedded PSRAM 8MB”) |
| `CONFIG_SPIRAM_SPEED_80M` | yes | Stable default; 120 MHz octal is experimental |
| `CONFIG_SPIRAM_USE_CAPS_ALLOC` | yes | Only `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` — **not** default `malloc()` |
| `CONFIG_SPIRAM_IGNORE_NOTFOUND` | yes | Boot continues if PSRAM missing/misconfigured |
| `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` | no | Keep network stacks on internal RAM |
| BSS/rodata in PSRAM | no | Avoid stability edge cases |

## API: `tf_mem`

```c
#include "tf_mem.h"

// Bulk buffer: prefer PSRAM, fall back to internal
void *buf = tf_mem_calloc(n, sizeof(item_t), TF_MEM_PREFER_PSRAM);
// ...
tf_mem_free(buf);

// Always internal (DMA-adjacent, hot path, secrets)
void *dma = tf_mem_malloc(size, TF_MEM_INTERNAL);
```

Rules encoded in the helper:

- Prefer PSRAM only if initialized and size ≥ 1 KB.
- On PSRAM failure → internal → default `malloc()` last resort.
- `tf_mem_suggested_log_capacity()` returns a larger ring when PSRAM is up.

## What uses PSRAM today

| Consumer | Why safe |
|----------|----------|
| `log_manager` ring (`tf_log_entry_t[]`) | Large, mutex-protected, not DMA, not ISR-critical |
| Temporary log export / API snapshot buffers | Short-lived bulk, heap only |
| Future: large JSON export, optional SD cache | Same pattern via `TF_MEM_PREFER_PSRAM` |

## What must stay internal

- FreeRTOS task stacks and TCBs  
- WiFi / LWIP / HTTP server control structures  
- DMA-capable buffers  
- Rate limiter / fan / sensor hot state  
- Crypto keys and WiFi credential plaintext  
- Anything touched with cache disabled (flash write paths)

## Runtime visibility

`GET /api/device/info` → `memory.psram_available`, `free_psram`, `total_psram`.

UI: **Inställningar → Enhetsinformation → PSRAM**.

## If PSRAM init fails

With `IGNORE_NOTFOUND`, boot completes; `tf_mem_psram_available()` is false; all prefer-PSRAM allocs use internal RAM. Feature set unchanged (log capacity may be smaller).

## Future options (not enabled)

- `SPIRAM_USE_MALLOC` + reserve internal pool — more automatic, higher risk of wrong placement  
- WiFi/LWIP in PSRAM — can free internal heap but harder to reason about under load  
- ECC on octal PSRAM — trades capacity for integrity on harsh environments  
- Stacks in PSRAM — **do not** for control/WiFi tasks  
