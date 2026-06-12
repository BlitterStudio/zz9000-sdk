/*
 * ZZ9000 OpenSSL provider — hardware offload backend.
 *
 * These helpers marshal a provider operation into ZZ9000 SDK shared buffers,
 * run it through the SDK (zz9k_crypto_kx / _aead / _verify), and copy the
 * result back. They are compiled into the provider only for the Amiga target
 * (where the SDK and an open ZZ9KContext exist); the host build leaves
 * ZZ9K_PROVIDER_OFFLOAD undefined and the provider uses the software reference
 * directly. Each operation returns 1 when the offload ran and produced a
 * result, or -1 when it could not run (allocation/mailbox error) so the caller
 * can fall back to software. Verify helpers set *valid on success.
 *
 * Shared-buffer allocation is a synchronous mailbox round trip (~6 ms,
 * docs/zz9k-crypto-acceleration.md), so the context keeps one persistent
 * scratch buffer per role and only the crypto call itself touches the mailbox
 * on a warm path. The buffers grow geometrically when a larger record arrives
 * and are released by zz9k_offload_close.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_offload.h"

#include "zz9k/host.h"
#include "zz9k/crypto.h"
#include "zz9k/abi.h"

#include <stdlib.h>
#include <string.h>

#define ZZ9K_OFFLOAD_ALIGN 16U

/* The provider-local verify ids mirror the ABI enum so the operation files can
 * stay free of SDK headers; these compile-time checks (portable to gcc 2.95,
 * which builds this file inside amissl.library) keep the two in lockstep. */
typedef char zz9k_offload_assert_ecdsa_id[
    (ZZ9K_OFFLOAD_VERIFY_ECDSA_P256 == ZZ9K_CRYPTO_VERIFY_ECDSA_P256_SHA256)
        ? 1 : -1];
typedef char zz9k_offload_assert_rsa_id[
    (ZZ9K_OFFLOAD_VERIFY_RSA_PKCS1 == ZZ9K_CRYPTO_VERIFY_RSA_PKCS1_2048_SHA256)
        ? 1 : -1];

/* Persistent scratch slots. `key` is sized for the widest key marshalling
 * (RSA-4096 modulus + 4-byte exponent); `small` carries 32-byte inputs
 * (digest, X25519 point); `src`/`dst`/`aad` grow with the data. */
typedef struct {
  ZZ9KContext *sdk;
  int in_use;               /* reentrancy guard; provider contexts are
                               per-task (AmiSSL initialises per task), so a
                               plain flag suffices */
  ZZ9KSharedBuffer key;     /* up to 516 bytes */
  ZZ9KSharedBuffer small;   /* 32 bytes */
  ZZ9KSharedBuffer nonce;   /* 12 bytes */
  ZZ9KSharedBuffer aad;
  ZZ9KSharedBuffer src;
  ZZ9KSharedBuffer dst;
} ZZ9KOffloadCtx;

/* Make sure `b` exists and holds at least `need` bytes, growing geometrically
 * so a long TLS session settles into zero allocation round trips. */
static int zz9k_offload_ensure(ZZ9KOffloadCtx *o, ZZ9KSharedBuffer *b,
                               uint32_t need)
{
  uint32_t cap;

  if (need == 0U) {
    need = 1U;
  }
  if (b->handle != 0U && b->length >= need) {
    return 1;
  }
  cap = b->length != 0U ? b->length : 32U;
  while (cap < need) {
    cap *= 2U;
  }
  if (b->handle != 0U) {
    zz9k_free_shared(o->sdk, b->handle);
    memset(b, 0, sizeof(*b));
  }
  if (zz9k_alloc_shared(o->sdk, cap, ZZ9K_OFFLOAD_ALIGN, 0, b) !=
      ZZ9K_STATUS_OK) {
    memset(b, 0, sizeof(*b));
    return 0;
  }
  return 1;
}

static ZZ9KOffloadCtx *zz9k_offload_acquire(void *vctx)
{
  ZZ9KOffloadCtx *o = (ZZ9KOffloadCtx *)vctx;
  if (o == NULL || o->in_use) {
    return NULL;
  }
  o->in_use = 1;
  return o;
}

void *zz9k_offload_open(unsigned int *service_flags)
{
  ZZ9KContext *sdk = NULL;
  ZZ9KServiceInfo svc;
  ZZ9KOffloadCtx *o;

  if (service_flags != NULL) {
    *service_flags = 0U;
  }
  /* Diagnostic kill switch: with ENV:ZZ9K_DISABLE_OFFLOAD set, the provider
   * still loads and is preferred, but never touches the board — every
   * operation runs in the portable software reference. Lets a failure be
   * bisected on hardware between the firmware-offload marshalling and the
   * provider/software glue without rebuilding. */
  if (getenv("ZZ9K_DISABLE_OFFLOAD") != NULL) {
    return NULL;
  }
  if (zz9k_open(&sdk) != ZZ9K_STATUS_OK) {
    return NULL;
  }
  /* A board without the crypto service must behave exactly like an absent
   * board, or every operation would pay a failing mailbox round trip before
   * its software fallback. */
  if (zz9k_query_service(sdk, ZZ9K_SERVICE_CRYPTO, &svc) != ZZ9K_STATUS_OK) {
    zz9k_close(sdk);
    return NULL;
  }
  o = (ZZ9KOffloadCtx *)malloc(sizeof(*o));
  if (o == NULL) {
    zz9k_close(sdk);
    return NULL;
  }
  memset(o, 0, sizeof(*o));
  o->sdk = sdk;
  if (service_flags != NULL) {
    *service_flags = svc.flags;
  }
  return o;
}

void zz9k_offload_close(void *vctx)
{
  ZZ9KOffloadCtx *o = (ZZ9KOffloadCtx *)vctx;
  if (o == NULL) {
    return;
  }
  if (o->key.handle)   zz9k_free_shared(o->sdk, o->key.handle);
  if (o->small.handle) zz9k_free_shared(o->sdk, o->small.handle);
  if (o->nonce.handle) zz9k_free_shared(o->sdk, o->nonce.handle);
  if (o->aad.handle)   zz9k_free_shared(o->sdk, o->aad.handle);
  if (o->src.handle)   zz9k_free_shared(o->sdk, o->src.handle);
  if (o->dst.handle)   zz9k_free_shared(o->sdk, o->dst.handle);
  zz9k_close(o->sdk);
  free(o);
}

int zz9k_offload_x25519(void *vctx, unsigned char out[32],
                        const unsigned char scalar[32],
                        const unsigned char point[32])
{
  ZZ9KOffloadCtx *o = zz9k_offload_acquire(vctx);
  ZZ9KCryptoKxDesc desc;
  ZZ9KCryptoResult result;
  int rc = -1;

  if (o == NULL) {
    return -1;
  }
  if (!zz9k_offload_ensure(o, &o->key, 32U) ||
      !zz9k_offload_ensure(o, &o->small, 32U) ||
      !zz9k_offload_ensure(o, &o->dst, 32U)) {
    goto out;
  }
  memcpy((void *)o->key.data, scalar, 32);
  memcpy((void *)o->small.data, point, 32);
  if (!zz9k_crypto_build_x25519_desc(&desc, o->key.handle, 0, o->small.handle,
                                     0, o->dst.handle, 0)) {
    goto out;
  }
  memset(&result, 0, sizeof(result));
  if (zz9k_crypto_kx(o->sdk, &desc, &result) == ZZ9K_STATUS_OK) {
    memcpy(out, (const void *)o->dst.data, 32);
    rc = 1;
  }

out:
  o->in_use = 0;
  return rc;
}

int zz9k_offload_aead(void *vctx, int aes, unsigned int keylen, int decrypt,
                      const unsigned char *key, const unsigned char *iv,
                      const unsigned char *aad, unsigned int aadlen,
                      const unsigned char *in, unsigned int inlen,
                      unsigned char *out, unsigned char *tag)
{
  ZZ9KOffloadCtx *o = zz9k_offload_acquire(vctx);
  ZZ9KCryptoAeadDesc desc;
  ZZ9KCryptoResult result;
  unsigned int src_len = inlen + (decrypt ? 16U : 0U);
  unsigned int dst_len = inlen + (decrypt ? 0U : 16U);
  uint32_t flags = decrypt ? ZZ9K_CRYPTO_AEAD_FLAG_DECRYPT : 0U;
  int built;
  int rc = -1;

  if (o == NULL) {
    return -1;
  }
  if (!zz9k_offload_ensure(o, &o->src, src_len) ||
      !zz9k_offload_ensure(o, &o->dst, dst_len) ||
      !zz9k_offload_ensure(o, &o->key, keylen) ||
      !zz9k_offload_ensure(o, &o->nonce, 12U) ||
      (aadlen > 0U && !zz9k_offload_ensure(o, &o->aad, aadlen))) {
    goto out;
  }
  memcpy((void *)o->key.data, key, keylen);
  memcpy((void *)o->nonce.data, iv, 12);
  if (aadlen > 0U) {
    memcpy((void *)o->aad.data, aad, aadlen);
  }
  memcpy((void *)o->src.data, in, inlen);
  if (decrypt) {
    memcpy((unsigned char *)o->src.data + inlen, tag, 16);
  }

  if (aes) {
    built = zz9k_crypto_build_aes_gcm_desc(
        &desc, o->src.handle, 0, inlen, o->dst.handle, 0,
        aadlen ? o->aad.handle : ZZ9K_INVALID_HANDLE, 0, aadlen,
        o->key.handle, 0, keylen, o->nonce.handle, flags);
  } else {
    built = zz9k_crypto_build_chacha20_poly1305_desc(
        &desc, o->src.handle, 0, inlen, o->dst.handle, 0,
        aadlen ? o->aad.handle : ZZ9K_INVALID_HANDLE, 0, aadlen,
        o->key.handle, 0, o->nonce.handle, flags);
  }
  if (!built) {
    goto out;
  }
  memset(&result, 0, sizeof(result));
  if (zz9k_crypto_aead(o->sdk, &desc, &result) == ZZ9K_STATUS_OK) {
    memcpy(out, (const void *)o->dst.data, inlen);
    if (!decrypt) {
      memcpy(tag, (const unsigned char *)o->dst.data + inlen, 16);
    }
    rc = 1;
  }

out:
  o->in_use = 0;
  return rc;
}

int zz9k_offload_verify(void *vctx, unsigned int algorithm,
                        const unsigned char *hash, const unsigned char *sig,
                        unsigned int siglen, const unsigned char *key,
                        unsigned int keylen, int *valid)
{
  ZZ9KOffloadCtx *o = zz9k_offload_acquire(vctx);
  ZZ9KCryptoVerifyDesc desc;
  int rc = -1;

  if (o == NULL) {
    return -1;
  }
  if (!zz9k_offload_ensure(o, &o->small, 32U) ||
      !zz9k_offload_ensure(o, &o->src, siglen) ||
      !zz9k_offload_ensure(o, &o->key, keylen)) {
    goto out;
  }
  memcpy((void *)o->small.data, hash, 32);
  memcpy((void *)o->src.data, sig, siglen);
  memcpy((void *)o->key.data, key, keylen);
  if (!zz9k_crypto_build_verify_desc(&desc, algorithm, o->small.handle, 0, 32U,
                                     o->src.handle, 0, siglen, o->key.handle,
                                     0, keylen)) {
    goto out;
  }
  if (zz9k_crypto_verify(o->sdk, &desc, valid) == ZZ9K_STATUS_OK) {
    rc = 1;
  }

out:
  o->in_use = 0;
  return rc;
}
