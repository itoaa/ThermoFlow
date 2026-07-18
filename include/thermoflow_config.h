/**
 * @file thermoflow_config.h
 * @brief ThermoFlow configuration and constants
 * 
 * Security Requirements:
 * - SR-001: Input validation ranges
 * - SR-004: Fail-safe defaults
 * - SR-010: Environmental limits
 */

#ifndef THERMOFLOW_CONFIG_H
#define THERMOFLOW_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "thermoflow_version.h"

/**
 * @brief Application operating mode (see docs/APPLICATION_MODES.md)
 *
 * Affects UI, sensor roles, and whether PWM/IR/electrical control is active.
 * SR-010 humidity policy depends on mode when control is enabled.
 */
typedef enum {
    TF_MODE_AC_MONITOR = 0,      /**< Mobil AC — cold/hot side monitor; optional IR/el */
    TF_MODE_HEAT_EXCHANGER,      /**< Värmeväxlare — dual continuous fans in/out */
    TF_MODE_MINI_FTX,            /**< Mini-FTX — regenerative ceramic, alternating flow */
    TF_MODE_SENSOR_ONLY,         /**< Sensors/logging only, no actuators */
    TF_MODE_COUNT
} thermoflow_mode_t;

#ifndef THERMOFLOW_DEFAULT_MODE
#define THERMOFLOW_DEFAULT_MODE   TF_MODE_HEAT_EXCHANGER
#endif

// Hardware configuration
#define THERMOFLOW_USE_DISPLAY        1   // Set to 0 to disable OLED
#define THERMOFLOW_NUM_SENSORS        4   // Max number of SHT40 sensors
#define THERMOFLOW_NUM_FANS           2   // Number of PWM fans

// I2C configuration
#define I2C_MASTER_NUM                I2C_NUM_0
#define I2C_MASTER_SDA_IO             8
#define I2C_MASTER_SCL_IO             9
#define I2C_MASTER_FREQ_HZ            400000
#define I2C_MASTER_TX_BUF_DISABLE     0
#define I2C_MASTER_RX_BUF_DISABLE     0

// SHT40 sensor addresses (selectable via ADDR pin)
#define SHT40_ADDR_A                  0x44  // ADDR pin to GND
#define SHT40_ADDR_B                  0x45  // ADDR pin to VDD
#define SHT40_ADDR_C                  0x46  // Custom address
#define SHT40_ADDR_D                  0x47  // Custom address

// PWM fan configuration (SR-009)
#define FAN_1_GPIO                    10
#define FAN_2_GPIO                    11
#define FAN_PWM_FREQ_HZ               25000
#define FAN_PWM_RESOLUTION            8
#define FAN_PWM_MAX_DUTY              255

// Display configuration
#define DISPLAY_SDA_PIN               I2C_MASTER_SDA_IO
#define DISPLAY_SCL_PIN               I2C_MASTER_SCL_IO
#define DISPLAY_I2C_ADDR              0x3C
#define DISPLAY_WIDTH                 128
#define DISPLAY_HEIGHT                64

// Sensor reading configuration
#define SENSOR_READ_INTERVAL_MS       5000    // 5 seconds
#define SENSOR_MAX_FAIL_COUNT         3       // Fail-safe after 3 errors

// Anti-condensation protection (SR-010)
#define ANTI_CONDENSATION_RH_THRESHOLD     90.0f   // %
#define ANTI_CONDENSATION_RH_HYSTERESIS    5.0f    // % (resume at 85%)
#define ANTI_CONDENSATION_CHECK_INTERVAL   1000    // ms

// Temperature limits (physical constraints)
#define TEMP_MIN_VALID                -40.0f  // °C
#define TEMP_MAX_VALID                125.0f  // °C
#define HUMIDITY_MIN_VALID            0.0f    // %
#define HUMIDITY_MAX_VALID            100.0f  // %

// MQTT configuration (SR-003)
#define MQTT_BROKER_DEFAULT           ""
#define MQTT_PORT_DEFAULT             8883    // TLS
#define MQTT_KEEP_ALIVE             60
#define MQTT_QOS_DEFAULT              1
#define MQTT_BUFFER_SIZE            1024
#define MQTT_RECONNECT_INTERVAL_MS    5000

// Web server configuration
#define WEB_SERVER_PORT               443     // HTTPS
#define WEB_SERVER_STACK_SIZE         8192
#define WEB_SERVER_MAX_CLIENTS        4
#define WEB_SERVER_SESSION_TIMEOUT    1800    // seconds

// Public documentation site (MkDocs → GitHub Pages). Used by web UI help links.
// Override at build time if you host docs elsewhere.
#ifndef THERMOFLOW_DOCS_BASE_URL
#define THERMOFLOW_DOCS_BASE_URL      "https://itoaa.github.io/ThermoFlow"
#endif

// OTA configuration (SR-011)
#define OTA_SERVER_URL                ""
#define OTA_CHECK_INTERVAL_MS         3600000 // 1 hour
#define OTA_SIGNATURE_ALGORITHM       "Ed25519"
#define OTA_PUBLIC_KEY_SIZE           32
#define OTA_SIGNATURE_SIZE            64

// Security configuration (SR-002, SR-003)
#define SECURITY_SESSION_TIMEOUT      1800    // seconds
#define SECURITY_MAX_LOGIN_ATTEMPTS     5
#define SECURITY_LOCKOUT_DURATION     300     // seconds
#define SECURITY_REQUIRE_HTTPS        1

// Rate limiting defaults (SR-006) — names prefixed to avoid collision with rate_limiter.h enum
#define TF_CFG_RATE_LIMIT_WEB_RPS       10      // per second
#define TF_CFG_RATE_LIMIT_MQTT_RPS      100     // per second
#define TF_CFG_RATE_LIMIT_LOGIN_RPM       5       // per minute

// Logging configuration (SR-005) — see docs/LOGGING.md
#define TF_LOG_RAM_CAPACITY           100
#define TF_LOG_NVS_CAPACITY           32
#define TF_LOG_MQTT_TOPIC_DEFAULT     "thermoflow/logs"
#define LOG_BUFFER_SIZE               4096
#define LOG_FILE_PATH                 "/sd/logs/thermoflow.log"
#define LOG_ROTATION_SIZE             1048576 // 1 MB
#define LOG_MAX_FILES                 10

// Error codes
typedef enum {
    TF_OK = 0,
    TF_ERR_SENSOR_FAIL = -1,
    TF_ERR_FAN_FAULT = -2,
    TF_ERR_WIFI_FAIL = -3,
    TF_ERR_MQTT_FAIL = -4,
    TF_ERR_CONDENSATION_ALERT = -5,
    TF_ERR_SECURITY_VIOLATION = -6,
    TF_ERR_MEMORY = -7,
} thermoflow_err_t;

#endif // THERMOFLOW_CONFIG_H