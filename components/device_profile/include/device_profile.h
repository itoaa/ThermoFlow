/**
 * @file device_profile.h
 * @brief Application profile (use case) selection and NVS persistence
 */

#ifndef DEVICE_PROFILE_H
#define DEVICE_PROFILE_H

#include "esp_err.h"
#include "thermoflow_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef thermoflow_mode_t device_profile_t;

#define DEVICE_PROFILE_COUNT  TF_MODE_COUNT

esp_err_t device_profile_init(void);
device_profile_t device_profile_get(void);
esp_err_t device_profile_set(device_profile_t profile);
bool device_profile_is_valid(device_profile_t profile);
const char *device_profile_to_id(device_profile_t profile);
const char *device_profile_label(device_profile_t profile);
const char *device_profile_description(device_profile_t profile);
device_profile_t device_profile_from_id(const char *id);
device_profile_t device_profile_get_default(void);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_PROFILE_H */