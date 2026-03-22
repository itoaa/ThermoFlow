/**
 * @file ed25519_impl.h
 * @brief Minimal Ed25519 Implementation Header
 * 
 * Based on ref10 from SUPERCOP, simplified for embedded use
 * 
 * @version 1.0.0
 * @date 2026-03-22
 */

#ifndef ED25519_IMPL_H
#define ED25519_IMPL_H

#include <stdint.h>
#include <stddef.h>

#define ED25519_PUBLIC_KEY_LEN  32
#define ED25519_PRIVATE_KEY_LEN 32
#define ED25519_SIGNATURE_LEN   64

/**
 * @brief Generate Ed25519 keypair
 * 
 * @param[out] public_key 32 bytes
 * @param[out] private_key 32 bytes
 * @return 0 on success
 */
int ed25519_generate_keypair(uint8_t *public_key, uint8_t *private_key);

/**
 * @brief Sign message with Ed25519
 * 
 * @param[out] signature 64 bytes
 * @param message Message to sign
 * @param message_len Message length
 * @param private_key 32 bytes private key
 * @return 0 on success
 */
int ed25519_sign(uint8_t *signature, const uint8_t *message, size_t message_len,
                  const uint8_t *private_key);

/**
 * @brief Verify Ed25519 signature
 * 
 * @param signature 64 bytes
 * @param message Original message
 * @param message_len Message length
 * @param public_key 32 bytes public key
 * @return 0 if valid, non-zero otherwise
 */
int ed25519_verify(const uint8_t *signature, const uint8_t *message, size_t message_len,
                    const uint8_t *public_key);

/**
 * @brief Derive public key from private key
 * 
 * @param[out] public_key 32 bytes
 * @param private_key 32 bytes
 * @return 0 on success
 */
int ed25519_public_from_private(uint8_t *public_key, const uint8_t *private_key);

#endif /* ED25519_IMPL_H */