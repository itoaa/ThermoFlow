/**
 * @file tf_mem.h
 * @brief ThermoFlow memory policy helpers (internal vs optional PSRAM)
 *
 * Design goals:
 * - System must run correctly with zero PSRAM (board without chip, or init fail).
 * - PSRAM is only used when explicitly requested via these helpers.
 * - Default malloc() / FreeRTOS / WiFi / DMA stay on internal RAM
 *   (CONFIG_SPIRAM_USE_CAPS_ALLOC — not SPIRAM_USE_MALLOC).
 *
 * Safe for PSRAM: bulk, non-DMA, non-ISR-critical data (log rings, export buffers).
 * Keep internal: stacks, DMA, WiFi/LWIP control paths, crypto keys, hot ISR state.
 */

#ifndef TF_MEM_H
#define TF_MEM_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /** Prefer internal SRAM; never use PSRAM. */
    TF_MEM_INTERNAL = 0,
    /**
     * Prefer PSRAM when available and size is large enough; fall back to internal.
     * Suitable for bulk, cacheable, non-DMA buffers.
     */
    TF_MEM_PREFER_PSRAM = 1,
} tf_mem_prefer_t;

/**
 * @brief Allocate zeroed memory according to policy.
 * @param size Bytes to allocate (0 returns NULL)
 * @param prefer Allocation preference
 * @return Pointer or NULL on failure
 */
void *tf_mem_calloc(size_t count, size_t size, tf_mem_prefer_t prefer);

/**
 * @brief Allocate uninitialized memory according to policy.
 */
void *tf_mem_malloc(size_t size, tf_mem_prefer_t prefer);

/**
 * @brief Free memory from tf_mem_* or heap_caps_*.
 */
void tf_mem_free(void *ptr);

/**
 * @brief True if SPIRAM was initialized and has free capacity.
 */
bool tf_mem_psram_available(void);

/**
 * @brief Fill runtime stats for diagnostics /api/device/info.
 */
void tf_mem_get_stats(size_t *free_internal, size_t *total_internal,
                      size_t *free_psram, size_t *total_psram,
                      bool *psram_available);

/**
 * @brief Suggested log ring capacity: larger when PSRAM is available.
 */
uint16_t tf_mem_suggested_log_capacity(uint16_t default_capacity);

#ifdef __cplusplus
}
#endif

#endif /* TF_MEM_H */
