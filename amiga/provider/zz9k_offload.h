/*
 * ZZ9000 OpenSSL provider — hardware offload backend interface.
 *
 * Implemented in zz9k_offload.c, which is linked into the provider only for the
 * Amiga target (ZZ9K_PROVIDER_OFFLOAD defined). The context is passed as an
 * opaque pointer (the SDK's ZZ9KContext) so the provider operation files need
 * not include the SDK headers. Each call returns 1 if the offload ran and
 * produced a result, or -1 if it could not run so the caller falls back to the
 * software reference. Verify calls set *valid (1 = valid signature).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZ9K_OFFLOAD_H
#define ZZ9K_OFFLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Verify algorithm ids, mirroring ZZ9K_CRYPTO_VERIFY_* in zz9k/abi.h (kept
 * local so the OpenSSL provider files need not include the SDK ABI header). */
#define ZZ9K_OFFLOAD_VERIFY_ECDSA_P256 1U
#define ZZ9K_OFFLOAD_VERIFY_RSA_PKCS1  2U

int zz9k_offload_x25519(void *ctx, unsigned char out[32],
                        const unsigned char scalar[32],
                        const unsigned char point[32]);

/* aes != 0 selects AES-GCM (keylen 16 or 32); aes == 0 selects
 * ChaCha20-Poly1305 (keylen 32). The 16-byte tag is produced (encrypt) or
 * consumed (decrypt) via `tag`. */
int zz9k_offload_aead(void *ctx, int aes, unsigned int keylen, int decrypt,
                      const unsigned char *key, const unsigned char *iv,
                      const unsigned char *aad, unsigned int aadlen,
                      const unsigned char *in, unsigned int inlen,
                      unsigned char *out, unsigned char *tag);

/* `algorithm` is a ZZ9K_CRYPTO_VERIFY_* value. For ECDSA-P256 the key is the
 * 65-byte point and sig is r||s (64). For RSA the key is modulus||exponent and
 * sig is the modulus-width signature. The digest is always 32 bytes. */
int zz9k_offload_verify(void *ctx, unsigned int algorithm,
                        const unsigned char *hash, const unsigned char *sig,
                        unsigned int siglen, const unsigned char *key,
                        unsigned int keylen, int *valid);

#ifdef __cplusplus
}
#endif

#endif /* ZZ9K_OFFLOAD_H */
