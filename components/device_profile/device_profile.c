/**
 * @file device_profile.c
 * @brief NVS-backed application profile selection
 */

#include "device_profile.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "DEVICE_PROFILE";
static const char *NVS_NAMESPACE = "device_cfg";
static const char *NVS_KEY_PROFILE = "profile";

static device_profile_t s_profile = THERMOFLOW_DEFAULT_MODE;
static bool s_initialized = false;

typedef struct {
    const char *id;
    const char *label;
    const char *description;
} profile_meta_t;

static const profile_meta_t s_meta[] = {
    [TF_MODE_AC_MONITOR] = {
        .id = "ac_monitor",
        .label = "Mobil AC",
        .description = "Overvaka kall och varm luft fran portabel AC",
    },
    [TF_MODE_HEAT_EXCHANGER] = {
        .id = "heat_exchanger",
        .label = "Varmevaxlare",
        .description = "DIY varmevaxlare med 1-2 flaktar",
    },
    [TF_MODE_MINI_FTX] = {
        .id = "mini_ftx",
        .label = "Mini-FTX",
        .description = "Franluftsventilation med varmeatervinning",
    },
    [TF_MODE_SENSOR_ONLY] = {
        .id = "sensor_only",
        .label = "Endast sensorer",
        .description = "Temperatur och luftfuktighet utan flaktstyrning",
    },
};

bool device_profile_is_valid(device_profile_t profile)
{
    return profile >= 0 && profile < TF_MODE_COUNT;
}

device_profile_t device_profile_get_default(void)
{
    return THERMOFLOW_DEFAULT_MODE;
}

const char *device_profile_to_id(device_profile_t profile)
{
    if (!device_profile_is_valid(profile)) {
        return "unknown";
    }
    return s_meta[profile].id;
}

const char *device_profile_label(device_profile_t profile)
{
    if (!device_profile_is_valid(profile)) {
        return "Okand";
    }
    return s_meta[profile].label;
}

const char *device_profile_description(device_profile_t profile)
{
    if (!device_profile_is_valid(profile)) {
        return "";
    }
    return s_meta[profile].description;
}

device_profile_t device_profile_from_id(const char *id)
{
    if (!id) {
        return device_profile_get_default();
    }

    for (int i = 0; i < TF_MODE_COUNT; i++) {
        if (strcmp(id, s_meta[i].id) == 0) {
            return (device_profile_t)i;
        }
    }

    return device_profile_get_default();
}

static esp_err_t load_profile(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t value = (uint8_t)THERMOFLOW_DEFAULT_MODE;
    err = nvs_get_u8(handle, NVS_KEY_PROFILE, &value);
    nvs_close(handle);

    if (err == ESP_OK && device_profile_is_valid((device_profile_t)value)) {
        s_profile = (device_profile_t)value;
    }

    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
}

static esp_err_t save_profile(device_profile_t profile)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_PROFILE, (uint8_t)profile);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t device_profile_init(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = load_profile();
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not load profile (%s), using default",
                 esp_err_to_name(err));
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Application profile: %s (%s)",
             device_profile_label(s_profile),
             device_profile_to_id(s_profile));
    return ESP_OK;
}

device_profile_t device_profile_get(void)
{
    return s_profile;
}

esp_err_t device_profile_set(device_profile_t profile)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!device_profile_is_valid(profile)) {
        return ESP_ERR_INVALID_ARG;
    }

    s_profile = profile;
    esp_err_t err = save_profile(profile);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save profile: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Application profile set to %s", device_profile_to_id(profile));
    return ESP_OK;
}