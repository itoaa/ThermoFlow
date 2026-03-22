/**
 * @file rate_limiter.c
 * @brief Rate Limiter Implementation (Token Bucket) - ESP-IDF
 *
 * Token bucket rate limiter for protecting system resources.
 * Supports per-client tracking with configurable limits per service type.
 * Implements IEC 62443 SR-006: Resource Limits requirement.
 *
 * Features:
 * - Token bucket algorithm with configurable rate/burst
 * - Per-client tracking (up to 32 clients per service type)
 * - Automatic blocking with configurable duration
 * - Global and per-client limits
 * - Statistics collection
 * - Thread-safe with mutex protection
 *
 * Default limits:
 * - Web requests: 10/sec, burst 20, block 10s
 * - MQTT messages: 100/sec, burst 200, no block
 * - Login attempts: 1/sec, burst 5, block 5min
 *
 * @author Ola Andersson
 * @version 1.0.0
 * @date 2026-03-22
 *
 * @section changelog Change Log
 * - 1.0.0 (2026-03-22): Initial implementation
 *   - Token bucket algorithm
 *   - Per-client hash table (linear probing)
 *   - Three service types with different limits
 *   - Statistics tracking
 */

#include <string.h>                   /* memset, memcpy */
#include <stdlib.h>                   /* calloc, free */
#include <stdio.h>                    /* sscanf */
#include "rate_limiter.h"              /* Public interface */
#include "freertos/FreeRTOS.h"         /* FreeRTOS core */
#include "freertos/semphr.h"           /* Semaphores for thread safety */
#include "freertos/task.h"             /* Task functions */
#include "esp_log.h"                   /* ESP-IDF logging */
#include "esp_timer.h"                 /* High-resolution timer */

/* Logging tag - appears in log messages from this component */
static const char *TAG = "RATE_LIMIT";

/* Maximum number of tracked clients per service type */
#define MAX_CLIENTS_PER_TYPE    32

/* Maximum bytes of client ID to use for hashing */
#define CLIENT_HASH_SIZE        16

/**
 * @brief Per-client token bucket state
 */
typedef struct client_bucket {
    rate_limit_client_id_t client;     /* Client identifier (IP or other) */
    uint32_t tokens;                   /* Current token count */
    uint64_t last_update_us;           /* Last token refill timestamp */
    uint64_t blocked_until_us;         /* Block expiration timestamp */
    bool active;                       /* Slot is occupied */
} client_bucket_t;

/**
 * @brief Per-service-type rate limiter state
 */
typedef struct rate_limit_state {
    rate_limit_config_t config;        /* Configuration for this service */
    uint32_t global_tokens;            /* Global token bucket */
    uint64_t global_last_update_us;    /* Global last refill timestamp */
    client_bucket_t clients[MAX_CLIENTS_PER_TYPE];  /* Per-client buckets */
    uint64_t stats_allowed;              /* Total allowed requests */
    uint64_t stats_blocked;              /* Total blocked requests */
} rate_limit_state_t;

/**
 * @brief Global rate limiter state
 */
static struct {
    bool initialized;                  /* Module initialized flag */
    rate_limit_state_t states[RATE_LIMIT_MAX];  /* One state per service type */
    SemaphoreHandle_t mutex;           /* Thread safety mutex */
} limiter;

/**
 * @brief Calculate minimum of two uint32_t values
 * @param a First value
 * @param b Second value
 * @return Smaller of a and b
 */
static uint32_t min_uint32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

/**
 * @brief Hash function for client identification
 *
 * DJB2 hash algorithm with 5381 seed.
 * Produces uniform distribution for hash table lookup.
 *
 * @param client Client identifier
 * @return 32-bit hash value
 */
static uint32_t hash_client(const rate_limit_client_id_t *client)
{
    if (!client || client->len == 0) {
        return 0;
    }

    uint32_t hash = 5381;
    for (int i = 0; i < client->len && i < CLIENT_HASH_SIZE; i++) {
        hash = ((hash << 5) + hash) + client->id[i];  /* hash * 33 + c */
    }
    return hash;
}

/**
 * @brief Find or allocate client slot in hash table
 *
 * Uses linear probing for collision resolution.
 * Returns existing slot if client already tracked,
 * or first free slot for new clients.
 *
 * @param type Service type
 * @param client Client identifier
 * @return Slot index (0-31), or -1 if table full
 */
static int find_client_slot(rate_limit_type_t type, const rate_limit_client_id_t *client)
{
    if (!client) {
        return -1;
    }

    uint32_t hash = hash_client(client);
    int start_idx = hash % MAX_CLIENTS_PER_TYPE;

    for (int i = 0; i < MAX_CLIENTS_PER_TYPE; i++) {
        int idx = (start_idx + i) % MAX_CLIENTS_PER_TYPE;

        if (!limiter.states[type].clients[idx].active) {
            return idx;  /* Free slot found */
        }

        /* Check if this is the client we're looking for */
        if (limiter.states[type].clients[idx].client.len == client->len &&
            memcmp(limiter.states[type].clients[idx].client.id, client->id, client->len) == 0) {
            return idx;  /* Existing client found */
        }
    }

    return -1;  /* Table full */
}

/**
 * @brief Refill tokens in a client bucket
 *
 * Calculates elapsed time and adds tokens based on rate.
 * Caps at bucket_size (burst limit).
 *
 * @param type Service type
 * @param bucket Client bucket to refill
 */
static void refill_tokens(rate_limit_type_t type, client_bucket_t *bucket)
{
    uint64_t now = esp_timer_get_time();  /* Current time in microseconds */
    uint64_t elapsed_us = now - bucket->last_update_us;

    /* Calculate tokens to add: tokens_per_second * elapsed seconds */
    uint32_t tokens_to_add = (uint32_t)((elapsed_us * limiter.states[type].config.tokens_per_second) / 1000000);

    bucket->tokens = min_uint32(bucket->tokens + tokens_to_add,
                                   limiter.states[type].config.bucket_size);
    bucket->last_update_us = now;
}

/**
 * @brief Initialize rate limiter
 *
 * Sets up mutex and default configurations for all service types.
 * Must be called before using other rate limiter functions.
 *
 * @return ESP_OK on success, ESP_ERR_NO_MEM if mutex creation fails,
 *         ESP_ERR_INVALID_STATE if already initialized
 */
esp_err_t rate_limiter_init(void)
{
    if (limiter.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Clear global state */
    memset(&limiter, 0, sizeof(limiter));

    /* Create mutex for thread safety */
    limiter.mutex = xSemaphoreCreateMutex();
    if (!limiter.mutex) {
        return ESP_ERR_NO_MEM;
    }

    /* Configure default limits:
     * Web requests: 10/sec, burst 20, block 10s on violation
     * MQTT messages: 100/sec, burst 200, no blocking (just drop)
     * Login attempts: 1/sec, burst 5, block 5min on violation
     */
    limiter.states[RATE_LIMIT_WEB_REQUESTS].config.tokens_per_second = 10;
    limiter.states[RATE_LIMIT_WEB_REQUESTS].config.bucket_size = 20;
    limiter.states[RATE_LIMIT_WEB_REQUESTS].config.block_duration_ms = 10000;

    limiter.states[RATE_LIMIT_MQTT_MESSAGES].config.tokens_per_second = 100;
    limiter.states[RATE_LIMIT_MQTT_MESSAGES].config.bucket_size = 200;
    limiter.states[RATE_LIMIT_MQTT_MESSAGES].config.block_duration_ms = 0;

    limiter.states[RATE_LIMIT_LOGIN_ATTEMPTS].config.tokens_per_second = 1;
    limiter.states[RATE_LIMIT_LOGIN_ATTEMPTS].config.bucket_size = 5;
    limiter.states[RATE_LIMIT_LOGIN_ATTEMPTS].config.block_duration_ms = 300000;

    /* Initialize all buckets with full tokens and current timestamp */
    uint64_t now = esp_timer_get_time();
    for (int i = 0; i < RATE_LIMIT_MAX; i++) {
        limiter.states[i].global_tokens = limiter.states[i].config.bucket_size;
        limiter.states[i].global_last_update_us = now;
        for (int j = 0; j < MAX_CLIENTS_PER_TYPE; j++) {
            limiter.states[i].clients[j].last_update_us = now;
        }
    }

    limiter.initialized = true;
    ESP_LOGI(TAG, "Rate limiter initialized");
    return ESP_OK;
}

/**
 * @brief Check if request is allowed (single token)
 *
 * Convenience wrapper for rate_limiter_check_n() with 1 token.
 *
 * @param type Service type being requested
 * @param client Client identifier (can be NULL for global-only check)
 * @return true if request allowed, false if rate limited
 */
bool rate_limiter_check(rate_limit_type_t type, const rate_limit_client_id_t *client)
{
    return rate_limiter_check_n(type, client, 1);
}

/**
 * @brief Check if request with N tokens is allowed
 *
 * Main rate limiting logic:
 * 1. Refill global tokens based on elapsed time
 * 2. Check global limit (affects all clients)
 * 3. If client specified, check per-client limit
 * 4. Update statistics
 *
 * @param type Service type being requested
 * @param client Client identifier (NULL for global check only)
 * @param tokens Number of tokens to consume (usually 1)
 * @return true if request allowed, false if rate limited
 */
bool rate_limiter_check_n(rate_limit_type_t type, const rate_limit_client_id_t *client,
                           uint32_t tokens)
{
    if (!limiter.initialized || type >= RATE_LIMIT_MAX) {
        return false;  /* Deny if not initialized or invalid type */
    }

    xSemaphoreTake(limiter.mutex, portMAX_DELAY);

    uint64_t now = esp_timer_get_time();
    bool allowed = false;

    /* Refill global tokens */
    uint64_t elapsed_us = now - limiter.states[type].global_last_update_us;
    uint32_t tokens_to_add = (uint32_t)((elapsed_us * limiter.states[type].config.tokens_per_second) / 1000000);
    limiter.states[type].global_tokens = min_uint32(limiter.states[type].global_tokens + tokens_to_add,
                                                     limiter.states[type].config.bucket_size);
    limiter.states[type].global_last_update_us = now;

    /* Check global limit first */
    if (limiter.states[type].global_tokens >= tokens) {
        limiter.states[type].global_tokens -= tokens;
        allowed = true;
    }

    /* Check per-client limit if client specified */
    if (client && client->len > 0 && allowed) {
        int slot = find_client_slot(type, client);

        if (slot >= 0) {
            client_bucket_t *bucket = &limiter.states[type].clients[slot];

            if (!bucket->active) {
                /* New client - initialize bucket */
                memcpy(&bucket->client, client, sizeof(rate_limit_client_id_t));
                bucket->tokens = limiter.states[type].config.bucket_size;
                bucket->last_update_us = now;
                bucket->blocked_until_us = 0;
                bucket->active = true;
            }

            /* Check if currently blocked */
            if (bucket->blocked_until_us > now) {
                allowed = false;
            } else {
                refill_tokens(type, bucket);

                if (bucket->tokens >= tokens) {
                    bucket->tokens -= tokens;
                    allowed = true;
                } else {
                    allowed = false;
                    /* Apply block duration if configured */
                    if (limiter.states[type].config.block_duration_ms > 0) {
                        bucket->blocked_until_us = now + (limiter.states[type].config.block_duration_ms * 1000);
                        ESP_LOGW(TAG, "Client blocked for %lu ms",
                                 limiter.states[type].config.block_duration_ms);
                    }
                }
            }
        } else {
            /* Hash table full - allow but log warning */
            ESP_LOGW(TAG, "Client bucket full for type %d", type);
            allowed = true;
        }
    }

    /* Update statistics */
    if (allowed) {
        limiter.states[type].stats_allowed++;
    } else {
        limiter.states[type].stats_blocked++;
        ESP_LOGW(TAG, "Rate limit exceeded for type %d", type);
    }

    xSemaphoreGive(limiter.mutex);
    return allowed;
}

/**
 * @brief Check if client is currently blocked
 *
 * @param type Service type
 * @param client Client identifier
 * @return true if client is blocked, false otherwise
 */
bool rate_limiter_is_blocked(rate_limit_type_t type, const rate_limit_client_id_t *client)
{
    if (!client || !limiter.initialized || type >= RATE_LIMIT_MAX) {
        return false;
    }

    xSemaphoreTake(limiter.mutex, portMAX_DELAY);

    int slot = find_client_slot(type, client);
    bool blocked = (slot >= 0 && limiter.states[type].clients[slot].active &&
                    limiter.states[type].clients[slot].blocked_until_us > esp_timer_get_time());

    xSemaphoreGive(limiter.mutex);
    return blocked;
}

/**
 * @brief Parse IP address string to client ID
 *
 * Supports IPv4 dotted notation (converts to 4 bytes).
 * Falls back to raw string storage for other formats.
 *
 * @param ip_addr IP address string (e.g., "192.168.1.1")
 * @param[out] client Client ID structure to fill
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if inputs NULL
 */
esp_err_t rate_limiter_client_from_ip(const char *ip_addr, rate_limit_client_id_t *client)
{
    if (!ip_addr || !client) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(client, 0, sizeof(*client));

    /* Try parsing as IPv4 */
    int octets[4];
    if (sscanf(ip_addr, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]) == 4) {
        client->id[0] = (uint8_t)octets[0];
        client->id[1] = (uint8_t)octets[1];
        client->id[2] = (uint8_t)octets[2];
        client->id[3] = (uint8_t)octets[3];
        client->len = 4;
    } else {
        /* Store raw string if not IPv4 */
        size_t len = strlen(ip_addr);
        if (len > CLIENT_HASH_SIZE) {
            len = CLIENT_HASH_SIZE;
        }
        memcpy(client->id, ip_addr, len);
        client->len = len;
    }

    return ESP_OK;
}
