/*
 * Portable software reference for ChaCha20, Poly1305, and the
 * ChaCha20-Poly1305 AEAD (RFC 8439).
 *
 * This is the m68k software baseline the crypto benchmark measures the
 * ZZ9000 offload against, and the verification oracle for the planned
 * AmiSSL provider. It is plain C99 with no AmigaOS or SDK dependencies so
 * it compiles unchanged on the host (for CTest) and on m68k.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZ9K_CRYPTO_SOFT_H
#define ZZ9K_CRYPTO_SOFT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZZ9K_SOFT_CHACHA20_KEY_BYTES 32U
#define ZZ9K_SOFT_CHACHA20_NONCE_BYTES 12U
#define ZZ9K_SOFT_POLY1305_KEY_BYTES 32U
#define ZZ9K_SOFT_POLY1305_TAG_BYTES 16U

/*
 * ChaCha20 keystream XOR (RFC 8439 section 2.4). Encrypts or decrypts
 * `length` bytes from `in` into `out` (may alias). `counter` is the initial
 * 32-bit block counter.
 */
void zz9k_soft_chacha20_xor(uint8_t *out, const uint8_t *in, uint32_t length,
                            const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
                            const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES],
                            uint32_t counter);

/*
 * Poly1305 one-time authenticator (RFC 8439 section 2.5). `key` is the
 * 32-byte one-time key; the 16-byte tag is written to `tag`.
 */
void zz9k_soft_poly1305(uint8_t tag[ZZ9K_SOFT_POLY1305_TAG_BYTES],
                        const uint8_t *message, uint32_t length,
                        const uint8_t key[ZZ9K_SOFT_POLY1305_KEY_BYTES]);

/*
 * ChaCha20-Poly1305 AEAD encrypt (RFC 8439 section 2.8). Writes `length`
 * ciphertext bytes to `ciphertext` and the 16-byte tag to `tag`.
 */
void zz9k_soft_chacha20_poly1305_encrypt(
    uint8_t *ciphertext, uint8_t tag[ZZ9K_SOFT_POLY1305_TAG_BYTES],
    const uint8_t *plaintext, uint32_t length,
    const uint8_t *aad, uint32_t aad_length,
    const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
    const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES]);

/*
 * ChaCha20-Poly1305 AEAD decrypt (RFC 8439 section 2.8). Verifies `tag` in
 * constant time and, only on success, writes `length` plaintext bytes to
 * `plaintext`. Returns 1 when the tag is valid, 0 otherwise.
 */
int zz9k_soft_chacha20_poly1305_decrypt(
    uint8_t *plaintext,
    const uint8_t *ciphertext, uint32_t length,
    const uint8_t *aad, uint32_t aad_length,
    const uint8_t tag[ZZ9K_SOFT_POLY1305_TAG_BYTES],
    const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
    const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES]);

/*
 * X25519 Diffie-Hellman scalar multiplication (RFC 7748).
 * Returns 1 on success, 0 if result is the all-zero point (must be rejected).
 */
int zz9k_soft_x25519(uint8_t out[32], const uint8_t scalar[32],
                     const uint8_t point[32]);

#ifdef __cplusplus
}
#endif

#endif /* ZZ9K_CRYPTO_SOFT_H */
