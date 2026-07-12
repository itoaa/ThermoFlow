/**
 * @file ed25519_impl.c
 * @brief Ed25519 wrapper using Monocypher
 */

#include "ed25519_impl.h"
#include "monocypher.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>

static const char *TAG = "ED25519";

static void derive_keypair(uint8_t secret_key[64], uint8_t public_key[32],
                           const uint8_t seed[32])
{
    crypto_eddsa_key_pair(secret_key, public_key, (uint8_t *)seed);
}

int ed25519_generate_keypair(uint8_t *public_key, uint8_t *private_key)
{
    if (!public_key || !private_key) {
        return -1;
    }

    uint8_t seed[32];
    esp_fill_random(seed, sizeof(seed));

    uint8_t secret_key[64];
    derive_keypair(secret_key, public_key, seed);
    memcpy(private_key, seed, ED25519_PRIVATE_KEY_LEN);
    return 0;
}

int ed25519_public_from_private(uint8_t *public_key, const uint8_t *private_key)
{
    if (!public_key || !private_key) {
        return -1;
    }

    uint8_t secret_key[64];
    derive_keypair(secret_key, public_key, private_key);
    return 0;
}

int ed25519_sign(uint8_t *signature, const uint8_t *message, size_t message_len,
                 const uint8_t *private_key)
{
    if (!signature || !private_key) {
        return -1;
    }

    uint8_t public_key[32];
    uint8_t secret_key[64];
    derive_keypair(secret_key, public_key, private_key);

    crypto_eddsa_sign(signature, secret_key, message, message_len);
    return 0;
}

int ed25519_verify(const uint8_t *signature, const uint8_t *message, size_t message_len,
                   const uint8_t *public_key)
{
    if (!signature || !public_key) {
        return -1;
    }

    int result = crypto_eddsa_check(signature, public_key, message, message_len);
    if (result != 0) {
        ESP_LOGW(TAG, "Ed25519 signature verification failed");
    }
    return result;
}