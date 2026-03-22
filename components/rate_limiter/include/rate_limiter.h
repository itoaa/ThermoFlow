/**
 * @file rate_limiter.h
 * @brief Rate Limiter Interface
 * 
 * Implements token bucket rate limiting for:
 * - Web requests
 * - MQTT messages
 * - Login attempts
 * 
 * Implements IEC 62443 SR-006: Resource Limits
 * 
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Rate limiter types
 */
typedef enum {
    RATE_LIMIT_WEB_REQUESTS,    /**< HTTP/HTTPS requests per second */
    RATE_LIMIT_MQTT_MESSAGES,     /**< MQTT publish per second */
    RATE_LIMIT_LOGIN_ATTEMPTS,    /**< Login attempts per minute */
    RATE_LIMIT_MAX
} rate_limit_type_t;

/**
 * @brief Rate limit configuration
 */
typedef struct {
    uint32_t tokens_per_second;   /**< Token refill rate */
    uint32_t bucket_size;       /**< Maximum bucket size */
    uint32_t block_duration_ms;   /**< How long to block when exceeded (0 = no block) */
} rate_limit_config_t;

/**
 * @brief Client identifier (IP address or session ID)
 */
typedef struct {
    uint8_t id[16];             /**< Client identifier (e.g., IP address) */
    uint8_t len;                /**< Actual length of identifier */
} rate_limit_client_id_t;

/**
 * @brief Rate limiter handle
 */
typedef struct rate_limiter *rate_limiter_handle_t;

/**
 * @brief Initialize rate limiter
 * 
 * @return ESP_OK on success
 */
esp_err_t rate_limiter_init(void);

/**
 * @brief Configure rate limit for a type
 * 
 * @param type Rate limit type
 * @param config Configuration
 * @return ESP_OK on success
 */
esp_err_t rate_limiter_configure(rate_limit_type_t type, const rate_limit_config_t *config);

/**
 * @brief Check if request is allowed
 * 
 * Token bucket algorithm: consumes 1 token if available
 * 
 * @param type Rate limit type
 * @param client Client identifier (NULL for global limit)
 * @return true if allowed, false if rate limited
 */
bool rate_limiter_check(rate_limit_type_t type, const rate_limit_client_id_t *client);

/**
 * @brief Check with consumption amount
 * 
 * @param type Rate limit type
 * @param client Client identifier (NULL for global)
 * @param tokens Number of tokens to consume
 * @return true if allowed
 */
bool rate_limiter_check_n(rate_limit_type_t type, const rate_limit_client_id_t *client, 
                           uint32_t tokens);

/**
 * @brief Get remaining tokens
 * 
 * @param type Rate limit type
 * @param client Client identifier (NULL for global)
 * @return Number of tokens remaining
 */
uint32_t rate_limiter_get_remaining(rate_limit_type_t type, const rate_limit_client_id_t *client);

/**
 * @brief Check if client is currently blocked
 * 
 * @param type Rate limit type
 * @param client Client identifier
 * @return true if blocked
 */
bool rate_limiter_is_blocked(rate_limit_type_t type, const rate_limit_client_id_t *client);

/**
 * @brief Get time until unblocked (ms)
 * 
 * @param type Rate limit type
 * @param client Client identifier
 * @return Milliseconds until unblocked, 0 if not blocked
 */
uint32_t rate_limiter_get_block_time_ms(rate_limit_type_t type, 
                                          const rate_limit_client_id_t *client);

/**
 * @brief Reset rate limit for a client
 * 
 * @param type Rate limit type
 * @param client Client identifier
 * @return ESP_OK on success
 */
esp_err_t rate_limiter_reset(rate_limit_type_t type, const rate_limit_client_id_t *client);

/**
 * @brief Create client ID from IP address
 * 
 * @param ip_addr IP address string (e.g., "192.168.1.1")
 * @param[out] client Client ID structure
 * @return ESP_OK on success
 */
esp_err_t rate_limiter_client_from_ip(const char *ip_addr, rate_limit_client_id_t *client);

/**
 * @brief Create client ID from session token
 * 
 * @param session_id Session identifier
 * @param len Session ID length
 * @param[out] client Client ID structure
 * @return ESP_OK on success
 */
esp_err_t rate_limiter_client_from_session(const uint8_t *session_id, size_t len, 
                                            rate_limit_client_id_t *client);

/**
 * @brief Clean up expired entries
 * 
 * Call periodically to free memory
 * 
 * @return Number of entries cleaned
 */
uint32_t rate_limiter_cleanup(void);

/**
 * @brief Get statistics
 * 
 * @param type Rate limit type
 * @param[out] allowed Number of allowed requests
 * @param[out] blocked Number of blocked requests
 * @return ESP_OK on success
 */
esp_err_t rate_limiter_get_stats(rate_limit_type_t type, uint64_t *allowed, uint64_t *blocked);

/**
 * @brief Deinitialize rate limiter
 * 
 * @return ESP_OK on success
 */
esp_err_t rate_limiter_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* RATE_LIMITER_H */