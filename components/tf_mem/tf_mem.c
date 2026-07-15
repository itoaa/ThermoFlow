/**
 * @file tf_mem.c
 * @brief Optional PSRAM-aware allocation with internal fallback
 */

#include "tf_mem.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if CONFIG_SPIRAM
#include "esp_psram.h"
#endif

static const char *TAG = "TF_MEM";

/* Prefer PSRAM only for reasonably large buffers — tiny allocs stay internal. */
#define TF_MEM_PSRAM_MIN_SIZE   1024u

void *tf_mem_malloc(size_t size, tf_mem_prefer_t prefer)
{
    if (size == 0) {
        return NULL;
    }

    void *ptr = NULL;

    if (prefer == TF_MEM_PREFER_PSRAM &&
        size >= TF_MEM_PSRAM_MIN_SIZE &&
        tf_mem_psram_available()) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) {
            return ptr;
        }
        ESP_LOGD(TAG, "PSRAM alloc %u failed, falling back to internal", (unsigned)size);
    }

    /* Always allow pure internal, or fallback after PSRAM miss */
    ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!ptr) {
        /* Last resort: default heap (still internal when CAPS_ALLOC-only SPIRAM) */
        ptr = malloc(size);
    }
    return ptr;
}

void *tf_mem_calloc(size_t count, size_t size, tf_mem_prefer_t prefer)
{
    if (count == 0 || size == 0) {
        return NULL;
    }
    if (count > (SIZE_MAX / size)) {
        return NULL;
    }

    size_t total = count * size;
    void *ptr = tf_mem_malloc(total, prefer);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void tf_mem_free(void *ptr)
{
    free(ptr);
}

bool tf_mem_psram_available(void)
{
#if CONFIG_SPIRAM
    if (!esp_psram_is_initialized()) {
        return false;
    }
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
#else
    return false;
#endif
}

void tf_mem_get_stats(size_t *free_internal, size_t *total_internal,
                      size_t *free_psram, size_t *total_psram,
                      bool *psram_available)
{
    if (free_internal) {
        *free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    }
    if (total_internal) {
        *total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    }
    if (free_psram) {
        *free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    }
    if (total_psram) {
        *total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    }
    if (psram_available) {
        *psram_available = tf_mem_psram_available();
    }
}

uint16_t tf_mem_suggested_log_capacity(uint16_t default_capacity)
{
    if (default_capacity == 0) {
        default_capacity = 100;
    }
    /* With PSRAM, keep a larger ring for web Logg without pressuring internal SRAM */
    if (tf_mem_psram_available()) {
        const uint16_t boosted = 400;
        return default_capacity > boosted ? default_capacity : boosted;
    }
    return default_capacity;
}
