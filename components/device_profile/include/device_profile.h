/**
 * @file device_profile.h
 * @brief Application mode (profile), capabilities, and control preferences
 *
 * See docs/APPLICATION_MODES.md for product definitions.
 */

#ifndef DEVICE_PROFILE_H
#define DEVICE_PROFILE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "thermoflow_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef thermoflow_mode_t device_profile_t;

#define DEVICE_PROFILE_COUNT  TF_MODE_COUNT

/** How optional non-PWM actuation is requested (Mobil AC) */
typedef enum {
    TF_CTRL_METHOD_NONE = 0,
    TF_CTRL_METHOD_PWM,         /**< Built-in LEDC fans */
    TF_CTRL_METHOD_IR,          /**< IR blaster (stub until hardware) */
    TF_CTRL_METHOD_ELECTRICAL,  /**< Relay/GPIO (stub until mapped) */
} tf_control_method_t;

/** Mobil AC optional integrations (bitmask). Sensing is always on. */
#define TF_AC_MOD_IR_REMOTE     (1u << 0)  /**< IR-fjärr */
#define TF_AC_MOD_LINE_CONTROL  (1u << 1)  /**< Linjestyrning (relä/GPIO) */
#define TF_AC_MOD_ASSIST_FANS   (1u << 2)  /**< Hjälpfläktar (PWM) */

/**
 * What ThermoFlow does when cold-side condensation risk is high (Mobil AC).
 * Inspired by HVAC: alert-only vs increase exhaust assist vs request milder AC mode.
 */
typedef enum {
    TF_AC_COND_OBSERVE = 0,         /**< Only log + UI badge — no actuator change */
    TF_AC_COND_BOOST_ASSIST,        /**< Raise help-fan duty to clear moist air path */
    TF_AC_COND_REQUEST_FAN_ONLY,    /**< Ask AC via IR/line for fan-only (stub until HW) */
} tf_ac_cond_action_t;

typedef enum {
    TF_FAN_MODE_OFF = 0,
    TF_FAN_MODE_MANUAL,
    TF_FAN_MODE_AUTO,
} tf_fan_run_mode_t;

typedef enum {
    TF_CYCLE_PHASE_IDLE = 0,
    TF_CYCLE_PHASE_INTAKE,   /**< Air into room / core discharge */
    TF_CYCLE_PHASE_EXHAUST,  /**< Air out of room / core charge */
} tf_cycle_phase_t;

typedef struct {
    bool control_enabled;
    tf_control_method_t control_method;
    tf_fan_run_mode_t fan_mode;
    uint8_t fan1_speed;       /**< 0–100 supply / primary */
    uint8_t fan2_speed;       /**< 0–100 exhaust / secondary */
    uint16_t cycle_period_s;  /**< Mini-FTX full cycle (in+out), seconds */
    tf_cycle_phase_t cycle_phase; /**< Runtime status (not always persisted) */
    uint8_t ac_modules;       /**< TF_AC_MOD_* bitmask (Mobil AC) */
    tf_ac_cond_action_t ac_cond_action; /**< Condensation response policy */
    /** Last remote intent for IR/line (stub telemetry for UI) */
    char ac_last_command[24];
    uint32_t ac_last_command_ms;
} device_control_state_t;

typedef struct {
    const char *id;
    const char *label;
    const char *description;
    const char *control_nav_label; /**< Nav text for control view, or NULL if no control view */
    bool has_control_view;
    bool control_optional;         /**< May run with control off */
    bool control_default_on;
    uint8_t fan_count;
    bool dual_fan_independent;
    bool alternating_cycle;
    bool heat_recovery_stats;
    bool ir_control;
    bool electrical_control;
    bool pwm_control;
} device_profile_capabilities_t;

esp_err_t device_profile_init(void);

device_profile_t device_profile_get(void);
esp_err_t device_profile_set(device_profile_t profile);
bool device_profile_is_valid(device_profile_t profile);
const char *device_profile_to_id(device_profile_t profile);
const char *device_profile_label(device_profile_t profile);
const char *device_profile_description(device_profile_t profile);
device_profile_t device_profile_from_id(const char *id);
device_profile_t device_profile_get_default(void);

const device_profile_capabilities_t *device_profile_get_capabilities(device_profile_t profile);
const device_profile_capabilities_t *device_profile_get_active_capabilities(void);

/** Full control snapshot (copy). */
void device_profile_get_control(device_control_state_t *out);

/**
 * Update control fields. NULL pointers = leave unchanged.
 * Speeds clamped 0–100. Invalid method for profile → ESP_ERR_INVALID_ARG.
 */
esp_err_t device_profile_set_control(const bool *control_enabled,
                                     const tf_control_method_t *method,
                                     const tf_fan_run_mode_t *fan_mode,
                                     const uint8_t *fan1_speed,
                                     const uint8_t *fan2_speed,
                                     const uint16_t *cycle_period_s);

/** Set Mobil AC integration modules (sensing always implied). */
esp_err_t device_profile_set_ac_modules(uint8_t modules_mask);

esp_err_t device_profile_set_ac_cond_action(tf_ac_cond_action_t action);
void device_profile_note_ac_command(const char *command);

bool device_profile_is_control_active(void);
bool device_profile_ac_has_actuation(void);

const char *device_profile_control_method_str(tf_control_method_t m);
const char *device_profile_fan_mode_str(tf_fan_run_mode_t m);
const char *device_profile_cycle_phase_str(tf_cycle_phase_t p);
const char *device_profile_ac_cond_action_str(tf_ac_cond_action_t a);
tf_control_method_t device_profile_control_method_from_str(const char *s);
tf_fan_run_mode_t device_profile_fan_mode_from_str(const char *s);
tf_ac_cond_action_t device_profile_ac_cond_action_from_str(const char *s);

/** Runtime phase for Mini-FTX UI (not required for PWM-only HX). */
void device_profile_set_cycle_phase(tf_cycle_phase_t phase);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_PROFILE_H */
