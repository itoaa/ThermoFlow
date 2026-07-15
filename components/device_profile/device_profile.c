/**
 * @file device_profile.c
 * @brief NVS-backed application mode + control preferences
 */

#include "device_profile.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "DEVICE_PROFILE";
static const char *NVS_NAMESPACE = "device_cfg";
static const char *NVS_KEY_PROFILE = "profile";
static const char *NVS_KEY_CTRL_EN = "ctrl_en";
static const char *NVS_KEY_CTRL_METH = "ctrl_meth";
static const char *NVS_KEY_FAN_MODE = "fan_mode";
static const char *NVS_KEY_FAN1 = "fan1_spd";
static const char *NVS_KEY_FAN2 = "fan2_spd";
static const char *NVS_KEY_CYCLE = "cycle_s";
static const char *NVS_KEY_AC_MOD = "ac_mod";
static const char *NVS_KEY_AC_COND = "ac_cond";

static device_profile_t s_profile = THERMOFLOW_DEFAULT_MODE;
static device_control_state_t s_control = {
    .control_enabled = true,
    .control_method = TF_CTRL_METHOD_PWM,
    .fan_mode = TF_FAN_MODE_AUTO,
    .fan1_speed = 40,
    .fan2_speed = 40,
    .cycle_period_s = 60,
    .cycle_phase = TF_CYCLE_PHASE_IDLE,
    .ac_modules = 0,
    .ac_cond_action = TF_AC_COND_BOOST_ASSIST,
    .ac_last_command = {0},
    .ac_last_command_ms = 0,
};
static bool s_initialized = false;

static const device_profile_capabilities_t s_caps[TF_MODE_COUNT] = {
    [TF_MODE_AC_MONITOR] = {
        .id = "ac_monitor",
        .label = "Mobil AC",
        .description =
            "Overvaka kall- och varmsida pa portabel AC. Valfri styrning via IR eller elektrisk signal.",
        .control_nav_label = "AC-styrning",
        .has_control_view = true,
        .control_optional = true,
        .control_default_on = false,
        .fan_count = 0,
        .dual_fan_independent = false,
        .alternating_cycle = false,
        .heat_recovery_stats = false,
        .ir_control = true,
        .electrical_control = true,
        .pwm_control = false,
    },
    [TF_MODE_HEAT_EXCHANGER] = {
        .id = "heat_exchanger",
        .label = "Varmevaxlare",
        .description =
            "Kontinuerlig tilluft och franluft samtidigt. Oberoende styrning av tva flaktar (in/ut).",
        .control_nav_label = "Flaktar",
        .has_control_view = true,
        .control_optional = true,
        .control_default_on = true,
        .fan_count = 2,
        .dual_fan_independent = true,
        .alternating_cycle = false,
        .heat_recovery_stats = false,
        .ir_control = false,
        .electrical_control = false,
        .pwm_control = true,
    },
    [TF_MODE_MINI_FTX] = {
        .id = "mini_ftx",
        .label = "Mini-FTX",
        .description =
            "Regenerativ varmeatervinning med keramiskt element. Flakt vaxelvis in och ut.",
        .control_nav_label = "FTX",
        .has_control_view = true,
        .control_optional = true,
        .control_default_on = true,
        .fan_count = 1,
        .dual_fan_independent = false,
        .alternating_cycle = true,
        .heat_recovery_stats = true,
        .ir_control = false,
        .electrical_control = false,
        .pwm_control = true,
    },
    [TF_MODE_SENSOR_ONLY] = {
        .id = "sensor_only",
        .label = "Endast sensorer",
        .description =
            "Temperatur och luftfuktighet utan flakt- eller AC-styrning.",
        .control_nav_label = NULL,
        .has_control_view = false,
        .control_optional = false,
        .control_default_on = false,
        .fan_count = 0,
        .dual_fan_independent = false,
        .alternating_cycle = false,
        .heat_recovery_stats = false,
        .ir_control = false,
        .electrical_control = false,
        .pwm_control = false,
    },
};

static uint8_t clamp_speed(int v)
{
    if (v < 0) {
        return 0;
    }
    if (v > 100) {
        return 100;
    }
    return (uint8_t)v;
}

static void apply_profile_control_defaults(device_profile_t profile)
{
    const device_profile_capabilities_t *cap = &s_caps[profile];
    s_control.control_enabled = cap->control_default_on;
    if (cap->pwm_control) {
        s_control.control_method = TF_CTRL_METHOD_PWM;
    } else if (cap->ir_control || cap->electrical_control) {
        s_control.control_method = TF_CTRL_METHOD_NONE;
    } else {
        s_control.control_method = TF_CTRL_METHOD_NONE;
        s_control.control_enabled = false;
    }
    s_control.fan_mode = cap->control_default_on ? TF_FAN_MODE_AUTO : TF_FAN_MODE_OFF;
    s_control.cycle_phase = TF_CYCLE_PHASE_IDLE;
    if (profile == TF_MODE_AC_MONITOR) {
        s_control.ac_modules = 0; /* sensing only until user enables modules */
        s_control.control_enabled = false;
        s_control.control_method = TF_CTRL_METHOD_NONE;
        s_control.fan_mode = TF_FAN_MODE_AUTO;
        s_control.ac_cond_action = TF_AC_COND_BOOST_ASSIST;
        s_control.ac_last_command[0] = '\0';
        s_control.ac_last_command_ms = 0;
    } else {
        s_control.ac_modules = 0;
    }
}

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
    return s_caps[profile].id;
}

const char *device_profile_label(device_profile_t profile)
{
    if (!device_profile_is_valid(profile)) {
        return "Okand";
    }
    return s_caps[profile].label;
}

const char *device_profile_description(device_profile_t profile)
{
    if (!device_profile_is_valid(profile)) {
        return "";
    }
    return s_caps[profile].description;
}

device_profile_t device_profile_from_id(const char *id)
{
    if (!id) {
        return device_profile_get_default();
    }
    for (int i = 0; i < TF_MODE_COUNT; i++) {
        if (strcmp(id, s_caps[i].id) == 0) {
            return (device_profile_t)i;
        }
    }
    return device_profile_get_default();
}

const device_profile_capabilities_t *device_profile_get_capabilities(device_profile_t profile)
{
    if (!device_profile_is_valid(profile)) {
        return &s_caps[THERMOFLOW_DEFAULT_MODE];
    }
    return &s_caps[profile];
}

const device_profile_capabilities_t *device_profile_get_active_capabilities(void)
{
    return device_profile_get_capabilities(s_profile);
}

const char *device_profile_control_method_str(tf_control_method_t m)
{
    switch (m) {
        case TF_CTRL_METHOD_PWM: return "pwm";
        case TF_CTRL_METHOD_IR: return "ir";
        case TF_CTRL_METHOD_ELECTRICAL: return "electrical";
        case TF_CTRL_METHOD_NONE:
        default: return "none";
    }
}

const char *device_profile_fan_mode_str(tf_fan_run_mode_t m)
{
    switch (m) {
        case TF_FAN_MODE_MANUAL: return "manual";
        case TF_FAN_MODE_AUTO: return "auto";
        case TF_FAN_MODE_OFF:
        default: return "off";
    }
}

const char *device_profile_cycle_phase_str(tf_cycle_phase_t p)
{
    switch (p) {
        case TF_CYCLE_PHASE_INTAKE: return "intake";
        case TF_CYCLE_PHASE_EXHAUST: return "exhaust";
        case TF_CYCLE_PHASE_IDLE:
        default: return "idle";
    }
}

tf_control_method_t device_profile_control_method_from_str(const char *s)
{
    if (!s) {
        return TF_CTRL_METHOD_NONE;
    }
    if (strcmp(s, "pwm") == 0) {
        return TF_CTRL_METHOD_PWM;
    }
    if (strcmp(s, "ir") == 0) {
        return TF_CTRL_METHOD_IR;
    }
    if (strcmp(s, "electrical") == 0) {
        return TF_CTRL_METHOD_ELECTRICAL;
    }
    return TF_CTRL_METHOD_NONE;
}

tf_fan_run_mode_t device_profile_fan_mode_from_str(const char *s)
{
    if (!s) {
        return TF_FAN_MODE_OFF;
    }
    if (strcmp(s, "manual") == 0) {
        return TF_FAN_MODE_MANUAL;
    }
    if (strcmp(s, "auto") == 0) {
        return TF_FAN_MODE_AUTO;
    }
    return TF_FAN_MODE_OFF;
}

const char *device_profile_ac_cond_action_str(tf_ac_cond_action_t a)
{
    switch (a) {
        case TF_AC_COND_BOOST_ASSIST: return "boost_assist";
        case TF_AC_COND_REQUEST_FAN_ONLY: return "request_fan_only";
        case TF_AC_COND_OBSERVE:
        default: return "observe";
    }
}

tf_ac_cond_action_t device_profile_ac_cond_action_from_str(const char *s)
{
    if (!s) {
        return TF_AC_COND_OBSERVE;
    }
    if (strcmp(s, "boost_assist") == 0) {
        return TF_AC_COND_BOOST_ASSIST;
    }
    if (strcmp(s, "request_fan_only") == 0) {
        return TF_AC_COND_REQUEST_FAN_ONLY;
    }
    return TF_AC_COND_OBSERVE;
}

static esp_err_t load_all(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t value = (uint8_t)THERMOFLOW_DEFAULT_MODE;
    if (nvs_get_u8(handle, NVS_KEY_PROFILE, &value) == ESP_OK &&
        device_profile_is_valid((device_profile_t)value)) {
        s_profile = (device_profile_t)value;
    }

    uint8_t u8 = 0;
    if (nvs_get_u8(handle, NVS_KEY_CTRL_EN, &u8) == ESP_OK) {
        s_control.control_enabled = u8 != 0;
    } else {
        s_control.control_enabled = s_caps[s_profile].control_default_on;
    }

    if (nvs_get_u8(handle, NVS_KEY_CTRL_METH, &u8) == ESP_OK) {
        s_control.control_method = (tf_control_method_t)u8;
    }

    if (nvs_get_u8(handle, NVS_KEY_FAN_MODE, &u8) == ESP_OK) {
        s_control.fan_mode = (tf_fan_run_mode_t)u8;
    }

    if (nvs_get_u8(handle, NVS_KEY_FAN1, &u8) == ESP_OK) {
        s_control.fan1_speed = clamp_speed(u8);
    }
    if (nvs_get_u8(handle, NVS_KEY_FAN2, &u8) == ESP_OK) {
        s_control.fan2_speed = clamp_speed(u8);
    }

    uint16_t u16 = 0;
    if (nvs_get_u16(handle, NVS_KEY_CYCLE, &u16) == ESP_OK && u16 >= 10 && u16 <= 600) {
        s_control.cycle_period_s = u16;
    }

    if (nvs_get_u8(handle, NVS_KEY_AC_MOD, &u8) == ESP_OK) {
        s_control.ac_modules = u8 & (TF_AC_MOD_IR_REMOTE | TF_AC_MOD_LINE_CONTROL | TF_AC_MOD_ASSIST_FANS);
    }
    if (nvs_get_u8(handle, NVS_KEY_AC_COND, &u8) == ESP_OK && u8 <= TF_AC_COND_REQUEST_FAN_ONLY) {
        s_control.ac_cond_action = (tf_ac_cond_action_t)u8;
    }

    nvs_close(handle);
    return ESP_OK;
}

static esp_err_t save_control(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u8(handle, NVS_KEY_CTRL_EN, s_control.control_enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_CTRL_METH, (uint8_t)s_control.control_method);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_FAN_MODE, (uint8_t)s_control.fan_mode);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_FAN1, s_control.fan1_speed);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_FAN2, s_control.fan2_speed);
    }
    if (err == ESP_OK) {
        err = nvs_set_u16(handle, NVS_KEY_CYCLE, s_control.cycle_period_s);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_AC_MOD, s_control.ac_modules);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, NVS_KEY_AC_COND, (uint8_t)s_control.ac_cond_action);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t save_profile_key(device_profile_t profile)
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

    apply_profile_control_defaults(THERMOFLOW_DEFAULT_MODE);

    esp_err_t err = load_all();
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not load profile/control (%s), using defaults",
                 esp_err_to_name(err));
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Mode %s | control=%s method=%s fan_mode=%s",
             device_profile_to_id(s_profile),
             s_control.control_enabled ? "on" : "off",
             device_profile_control_method_str(s_control.control_method),
             device_profile_fan_mode_str(s_control.fan_mode));
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
    apply_profile_control_defaults(profile);

    esp_err_t err = save_profile_key(profile);
    if (err != ESP_OK) {
        return err;
    }
    err = save_control();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save control defaults: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Application mode set to %s", device_profile_to_id(profile));
    return ESP_OK;
}

void device_profile_get_control(device_control_state_t *out)
{
    if (out) {
        *out = s_control;
    }
}

void device_profile_set_cycle_phase(tf_cycle_phase_t phase)
{
    s_control.cycle_phase = phase;
}

bool device_profile_ac_has_actuation(void)
{
    return (s_control.ac_modules & (TF_AC_MOD_IR_REMOTE | TF_AC_MOD_LINE_CONTROL | TF_AC_MOD_ASSIST_FANS)) != 0;
}

bool device_profile_is_control_active(void)
{
    if (!s_initialized) {
        return false;
    }
    if (s_profile == TF_MODE_SENSOR_ONLY) {
        return false;
    }
    /* Mobil AC: modules chosen in Settings ARE the enable — no separate master switch */
    if (s_profile == TF_MODE_AC_MONITOR) {
        return device_profile_ac_has_actuation();
    }
    const device_profile_capabilities_t *cap = &s_caps[s_profile];
    if (!cap->has_control_view && !cap->pwm_control && !cap->ir_control && !cap->electrical_control) {
        return false;
    }
    return s_control.control_enabled;
}

esp_err_t device_profile_set_ac_modules(uint8_t modules_mask)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_profile != TF_MODE_AC_MONITOR) {
        return ESP_ERR_INVALID_STATE;
    }

    s_control.ac_modules = modules_mask &
        (TF_AC_MOD_IR_REMOTE | TF_AC_MOD_LINE_CONTROL | TF_AC_MOD_ASSIST_FANS);

    /* Modules imply enable for AC; no separate activation toggle in UI */
    s_control.control_enabled = device_profile_ac_has_actuation();

    if (s_control.ac_modules & TF_AC_MOD_ASSIST_FANS) {
        s_control.control_method = TF_CTRL_METHOD_PWM;
        if (s_control.fan_mode == TF_FAN_MODE_OFF) {
            s_control.fan_mode = TF_FAN_MODE_AUTO;
        }
    } else if (s_control.ac_modules & TF_AC_MOD_IR_REMOTE) {
        s_control.control_method = TF_CTRL_METHOD_IR;
    } else if (s_control.ac_modules & TF_AC_MOD_LINE_CONTROL) {
        s_control.control_method = TF_CTRL_METHOD_ELECTRICAL;
    } else {
        s_control.control_method = TF_CTRL_METHOD_NONE;
    }

    return save_control();
}

esp_err_t device_profile_set_ac_cond_action(tf_ac_cond_action_t action)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (action > TF_AC_COND_REQUEST_FAN_ONLY) {
        return ESP_ERR_INVALID_ARG;
    }
    s_control.ac_cond_action = action;
    return save_control();
}

void device_profile_note_ac_command(const char *command)
{
    if (!command) {
        return;
    }
    strncpy(s_control.ac_last_command, command, sizeof(s_control.ac_last_command) - 1);
    s_control.ac_last_command[sizeof(s_control.ac_last_command) - 1] = '\0';
    s_control.ac_last_command_ms = (uint32_t)(esp_log_timestamp());
}

esp_err_t device_profile_set_control(const bool *control_enabled,
                                     const tf_control_method_t *method,
                                     const tf_fan_run_mode_t *fan_mode,
                                     const uint8_t *fan1_speed,
                                     const uint8_t *fan2_speed,
                                     const uint16_t *cycle_period_s)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const device_profile_capabilities_t *cap = &s_caps[s_profile];

    if (method) {
        if (*method == TF_CTRL_METHOD_PWM && !cap->pwm_control) {
            return ESP_ERR_INVALID_ARG;
        }
        if (*method == TF_CTRL_METHOD_IR && !cap->ir_control) {
            return ESP_ERR_INVALID_ARG;
        }
        if (*method == TF_CTRL_METHOD_ELECTRICAL && !cap->electrical_control) {
            return ESP_ERR_INVALID_ARG;
        }
        s_control.control_method = *method;
    }

    if (control_enabled) {
        if (*control_enabled && s_profile == TF_MODE_SENSOR_ONLY) {
            return ESP_ERR_INVALID_ARG;
        }
        s_control.control_enabled = *control_enabled;
    }

    if (fan_mode) {
        s_control.fan_mode = *fan_mode;
    }
    if (fan1_speed) {
        s_control.fan1_speed = clamp_speed(*fan1_speed);
    }
    if (fan2_speed) {
        s_control.fan2_speed = clamp_speed(*fan2_speed);
    }
    if (cycle_period_s) {
        uint16_t p = *cycle_period_s;
        if (p < 10) {
            p = 10;
        }
        if (p > 600) {
            p = 600;
        }
        s_control.cycle_period_s = p;
    }

    return save_control();
}
