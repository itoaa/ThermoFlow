/**
 * @file wifi_types.h
 * @brief Common WiFi types for ThermoFlow
 * 
 * This header contains type definitions shared between wifi_manager
 * and wifi_secure_storage to avoid circular dependencies.
 * 
 * @version 1.0.0
 * @date 2026-04-12
 */

#ifndef WIFI_TYPES_H
#define WIFI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Encryption key source options
 */
typedef enum {
    WIFI_KEY_SOURCE_EFUSE = 0,     /*!< Use eFuse key (most secure) */
    WIFI_KEY_SOURCE_FLASH,          /*!< Use flash-stored key with tamper detection */
    WIFI_KEY_SOURCE_AUTO            /*!< Auto-select best available */
} wifi_key_source_t;

#ifdef __cplusplus
}
#endif

#endif // WIFI_TYPES_H
