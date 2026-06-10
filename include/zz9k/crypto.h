/*
 * Header-only crypto descriptor helpers for SDK callers.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZ9K_CRYPTO_H
#define ZZ9K_CRYPTO_H

#include "zz9k/abi.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZZ9K_CRYPTO_POLY1305_KEY_BYTES 32U
#define ZZ9K_CRYPTO_CHACHA20_KEY_BYTES 32U
#define ZZ9K_CRYPTO_CHACHA20_NONCE_BYTES 12U
#define ZZ9K_CRYPTO_CHACHA20_POLY1305_TAG_BYTES 16U

static inline uint32_t zz9k_crypto_digest_length(uint32_t algorithm)
{
  switch (algorithm) {
  case ZZ9K_CRYPTO_HASH_SHA1:
    return 20U;
  case ZZ9K_CRYPTO_HASH_SHA256:
  case ZZ9K_CRYPTO_HASH_BLAKE2S:
    return 32U;
  case ZZ9K_CRYPTO_HASH_SHA384:
    return 48U;
  case ZZ9K_CRYPTO_HASH_SHA512:
    return 64U;
  case ZZ9K_CRYPTO_HASH_POLY1305:
    return 16U;
  default:
    return 0U;
  }
}

static inline int zz9k_crypto_is_hash_algorithm(uint32_t algorithm)
{
  return zz9k_crypto_digest_length(algorithm) != 0U &&
         algorithm != ZZ9K_CRYPTO_HASH_POLY1305;
}

static inline int zz9k_crypto_build_hash_desc(ZZ9KCryptoHashDesc *desc,
                                              uint32_t algorithm,
                                              uint32_t src_handle,
                                              uint32_t src_offset,
                                              uint32_t src_length,
                                              uint32_t dst_handle,
                                              uint32_t dst_offset)
{
  if (!desc || !zz9k_crypto_is_hash_algorithm(algorithm) ||
      src_handle == ZZ9K_INVALID_HANDLE || src_length == 0U ||
      dst_handle == ZZ9K_INVALID_HANDLE) {
    return 0;
  }

  memset(desc, 0, sizeof(*desc));
  desc->src_handle = src_handle;
  desc->src_offset = src_offset;
  desc->src_length = src_length;
  desc->dst_handle = dst_handle;
  desc->dst_offset = dst_offset;
  desc->algorithm = algorithm;
  return 1;
}

static inline int zz9k_crypto_build_hmac_desc(ZZ9KCryptoHashDesc *desc,
                                              uint32_t algorithm,
                                              uint32_t src_handle,
                                              uint32_t src_offset,
                                              uint32_t src_length,
                                              uint32_t dst_handle,
                                              uint32_t dst_offset,
                                              uint32_t key_handle,
                                              uint32_t key_offset,
                                              uint32_t key_length)
{
  if (!zz9k_crypto_build_hash_desc(desc, algorithm, src_handle, src_offset,
                                   src_length, dst_handle, dst_offset)) {
    return 0;
  }
  if (key_handle == ZZ9K_INVALID_HANDLE || key_length == 0U) {
    memset(desc, 0, sizeof(*desc));
    return 0;
  }

  desc->key_handle = key_handle;
  desc->key_offset = key_offset;
  desc->key_length = key_length;
  desc->flags = ZZ9K_CRYPTO_HASH_FLAG_HMAC;
  return 1;
}

static inline int zz9k_crypto_build_poly1305_desc(ZZ9KCryptoHashDesc *desc,
                                                  uint32_t src_handle,
                                                  uint32_t src_offset,
                                                  uint32_t src_length,
                                                  uint32_t dst_handle,
                                                  uint32_t dst_offset,
                                                  uint32_t key_handle,
                                                  uint32_t key_offset)
{
  if (!desc || src_handle == ZZ9K_INVALID_HANDLE || src_length == 0U ||
      dst_handle == ZZ9K_INVALID_HANDLE ||
      key_handle == ZZ9K_INVALID_HANDLE) {
    return 0;
  }

  memset(desc, 0, sizeof(*desc));
  desc->src_handle = src_handle;
  desc->src_offset = src_offset;
  desc->src_length = src_length;
  desc->dst_handle = dst_handle;
  desc->dst_offset = dst_offset;
  desc->key_handle = key_handle;
  desc->key_offset = key_offset;
  desc->key_length = ZZ9K_CRYPTO_POLY1305_KEY_BYTES;
  desc->algorithm = ZZ9K_CRYPTO_HASH_POLY1305;
  return 1;
}

static inline int zz9k_crypto_build_stream_desc(ZZ9KCryptoStreamDesc *desc,
                                                uint32_t algorithm,
                                                uint32_t src_handle,
                                                uint32_t src_offset,
                                                uint32_t src_length,
                                                uint32_t dst_handle,
                                                uint32_t dst_offset,
                                                uint32_t key_handle,
                                                uint32_t key_offset,
                                                uint32_t nonce_handle,
                                                uint32_t nonce_offset,
                                                uint32_t counter,
                                                uint32_t flags)
{
  if (!desc || algorithm != ZZ9K_CRYPTO_STREAM_CHACHA20 ||
      src_handle == ZZ9K_INVALID_HANDLE || src_length == 0U ||
      dst_handle == ZZ9K_INVALID_HANDLE ||
      key_handle == ZZ9K_INVALID_HANDLE ||
      nonce_handle == ZZ9K_INVALID_HANDLE) {
    return 0;
  }

  memset(desc, 0, sizeof(*desc));
  desc->src_handle = src_handle;
  desc->src_offset = src_offset;
  desc->src_length = src_length;
  desc->dst_handle = dst_handle;
  desc->dst_offset = dst_offset;
  desc->key_handle = key_handle;
  desc->key_offset = key_offset;
  desc->nonce_handle = nonce_handle;
  desc->nonce_offset = nonce_offset;
  desc->counter = counter;
  desc->algorithm = algorithm;
  desc->flags = flags;
  return 1;
}

static inline int zz9k_crypto_build_chacha20_desc(
    ZZ9KCryptoStreamDesc *desc,
    uint32_t src_handle,
    uint32_t src_offset,
    uint32_t src_length,
    uint32_t dst_handle,
    uint32_t dst_offset,
    uint32_t key_handle,
    uint32_t key_offset,
    uint32_t nonce_handle,
    uint32_t nonce_offset,
    uint32_t counter)
{
  return zz9k_crypto_build_stream_desc(
      desc, ZZ9K_CRYPTO_STREAM_CHACHA20, src_handle, src_offset,
      src_length, dst_handle, dst_offset, key_handle, key_offset,
      nonce_handle, nonce_offset, counter, 0U);
}

static inline int zz9k_crypto_build_chacha20_poly1305_desc(
    ZZ9KCryptoAeadDesc *desc,
    uint32_t src_handle,
    uint32_t src_offset,
    uint32_t src_length,
    uint32_t dst_handle,
    uint32_t dst_offset,
    uint32_t aad_handle,
    uint32_t aad_offset,
    uint32_t aad_length,
    uint32_t key_handle,
    uint32_t key_offset,
    uint32_t nonce_handle,
    uint32_t flags)
{
  if (!desc || src_handle == ZZ9K_INVALID_HANDLE || src_length == 0U ||
      dst_handle == ZZ9K_INVALID_HANDLE ||
      key_handle == ZZ9K_INVALID_HANDLE ||
      nonce_handle == ZZ9K_INVALID_HANDLE ||
      (flags & ~ZZ9K_CRYPTO_AEAD_FLAG_DECRYPT) != 0U ||
      (aad_length != 0U && aad_handle == ZZ9K_INVALID_HANDLE)) {
    return 0;
  }

  memset(desc, 0, sizeof(*desc));
  desc->src_handle = src_handle;
  desc->src_offset = src_offset;
  desc->src_length = src_length;
  desc->dst_handle = dst_handle;
  desc->dst_offset = dst_offset;
  if (aad_length != 0U) {
    desc->aad_handle = aad_handle;
    desc->aad_offset = aad_offset;
    desc->aad_length = aad_length;
  }
  desc->key_handle = key_handle;
  desc->key_offset = key_offset;
  desc->nonce_handle = nonce_handle;
  desc->flags = flags;
  return 1;
}

typedef struct ZZ9KCryptoKxDesc {
  uint32_t scalar_handle;
  uint32_t scalar_offset;
  uint32_t point_handle;
  uint32_t point_offset;
  uint32_t dst_handle;
  uint32_t dst_offset;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KCryptoKxDesc;

static inline int zz9k_crypto_build_x25519_desc(
    ZZ9KCryptoKxDesc *desc,
    uint32_t scalar_handle, uint32_t scalar_offset,
    uint32_t point_handle, uint32_t point_offset,
    uint32_t dst_handle, uint32_t dst_offset)
{
  if (scalar_handle == ZZ9K_INVALID_HANDLE ||
      point_handle  == ZZ9K_INVALID_HANDLE ||
      dst_handle    == ZZ9K_INVALID_HANDLE)
    return 0;
  memset(desc, 0, sizeof(*desc));
  desc->scalar_handle = scalar_handle;
  desc->scalar_offset = scalar_offset;
  desc->point_handle  = point_handle;
  desc->point_offset  = point_offset;
  desc->dst_handle    = dst_handle;
  desc->dst_offset    = dst_offset;
  desc->algorithm     = ZZ9K_CRYPTO_KX_X25519;
  desc->flags         = 0U;
  return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* ZZ9K_CRYPTO_H */
