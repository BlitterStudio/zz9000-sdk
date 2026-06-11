/*
 * ZZ9000 OpenSSL provider — hardware offload backend.
 *
 * These helpers marshal a provider operation into ZZ9000 SDK shared buffers,
 * run it through the SDK (zz9k_crypto_kx / _aead / _verify), and copy the
 * result back. They are compiled into the provider only for the Amiga target
 * (where the SDK and an open ZZ9KContext exist); the host build leaves
 * ZZ9K_PROVIDER_OFFLOAD undefined and the provider uses the software reference
 * directly. Each function returns 1 when the offload ran and produced a result,
 * or -1 when it could not run (allocation/mailbox error) so the caller can fall
 * back to software. Verify helpers set *valid on success.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_offload.h"

#include "zz9k/host.h"
#include "zz9k/crypto.h"
#include "zz9k/abi.h"

#include <string.h>

#define ZZ9K_OFFLOAD_ALIGN 16U

int zz9k_offload_x25519(void *vctx, unsigned char out[32],
                        const unsigned char scalar[32],
                        const unsigned char point[32])
{
  ZZ9KContext *ctx = (ZZ9KContext *)vctx;
  ZZ9KSharedBuffer sb, pb, ob;
  ZZ9KCryptoKxDesc desc;
  ZZ9KCryptoResult result;
  int rc = -1;

  memset(&sb, 0, sizeof(sb));
  memset(&pb, 0, sizeof(pb));
  memset(&ob, 0, sizeof(ob));

  if (zz9k_alloc_shared(ctx, 32U, ZZ9K_OFFLOAD_ALIGN, 0, &sb) != ZZ9K_STATUS_OK ||
      zz9k_alloc_shared(ctx, 32U, ZZ9K_OFFLOAD_ALIGN, 0, &pb) != ZZ9K_STATUS_OK ||
      zz9k_alloc_shared(ctx, 32U, ZZ9K_OFFLOAD_ALIGN, 0, &ob) != ZZ9K_STATUS_OK) {
    goto out;
  }
  memcpy((void *)sb.data, scalar, 32);
  memcpy((void *)pb.data, point, 32);
  if (!zz9k_crypto_build_x25519_desc(&desc, sb.handle, 0, pb.handle, 0,
                                     ob.handle, 0)) {
    goto out;
  }
  memset(&result, 0, sizeof(result));
  if (zz9k_crypto_kx(ctx, &desc, &result) == ZZ9K_STATUS_OK) {
    memcpy(out, (const void *)ob.data, 32);
    rc = 1;
  }

out:
  if (ob.handle) zz9k_free_shared(ctx, ob.handle);
  if (pb.handle) zz9k_free_shared(ctx, pb.handle);
  if (sb.handle) zz9k_free_shared(ctx, sb.handle);
  return rc;
}

int zz9k_offload_aead(void *vctx, int aes, unsigned int keylen, int decrypt,
                      const unsigned char *key, const unsigned char *iv,
                      const unsigned char *aad, unsigned int aadlen,
                      const unsigned char *in, unsigned int inlen,
                      unsigned char *out, unsigned char *tag)
{
  ZZ9KContext *ctx = (ZZ9KContext *)vctx;
  ZZ9KSharedBuffer src, dst, kb, nb, ab;
  ZZ9KCryptoAeadDesc desc;
  ZZ9KCryptoResult result;
  unsigned int src_len = inlen + (decrypt ? 16U : 0U);
  unsigned int dst_len = inlen + (decrypt ? 0U : 16U);
  uint32_t flags = decrypt ? ZZ9K_CRYPTO_AEAD_FLAG_DECRYPT : 0U;
  int built;
  int rc = -1;

  memset(&src, 0, sizeof(src));
  memset(&dst, 0, sizeof(dst));
  memset(&kb, 0, sizeof(kb));
  memset(&nb, 0, sizeof(nb));
  memset(&ab, 0, sizeof(ab));

  if (zz9k_alloc_shared(ctx, src_len ? src_len : 1U, ZZ9K_OFFLOAD_ALIGN, 0,
                        &src) != ZZ9K_STATUS_OK ||
      zz9k_alloc_shared(ctx, dst_len ? dst_len : 1U, ZZ9K_OFFLOAD_ALIGN, 0,
                        &dst) != ZZ9K_STATUS_OK ||
      zz9k_alloc_shared(ctx, keylen, ZZ9K_OFFLOAD_ALIGN, 0, &kb) !=
          ZZ9K_STATUS_OK ||
      zz9k_alloc_shared(ctx, 12U, ZZ9K_OFFLOAD_ALIGN, 0, &nb) !=
          ZZ9K_STATUS_OK) {
    goto out;
  }
  if (aadlen > 0U &&
      zz9k_alloc_shared(ctx, aadlen, ZZ9K_OFFLOAD_ALIGN, 0, &ab) !=
          ZZ9K_STATUS_OK) {
    goto out;
  }
  memcpy((void *)kb.data, key, keylen);
  memcpy((void *)nb.data, iv, 12);
  if (aadlen > 0U) {
    memcpy((void *)ab.data, aad, aadlen);
  }
  memcpy((void *)src.data, in, inlen);
  if (decrypt) {
    memcpy((unsigned char *)src.data + inlen, tag, 16);
  }

  if (aes) {
    built = zz9k_crypto_build_aes_gcm_desc(
        &desc, src.handle, 0, inlen, dst.handle, 0,
        aadlen ? ab.handle : ZZ9K_INVALID_HANDLE, 0, aadlen,
        kb.handle, 0, keylen, nb.handle, flags);
  } else {
    built = zz9k_crypto_build_chacha20_poly1305_desc(
        &desc, src.handle, 0, inlen, dst.handle, 0,
        aadlen ? ab.handle : ZZ9K_INVALID_HANDLE, 0, aadlen,
        kb.handle, 0, nb.handle, flags);
  }
  if (!built) {
    goto out;
  }
  memset(&result, 0, sizeof(result));
  if (zz9k_crypto_aead(ctx, &desc, &result) == ZZ9K_STATUS_OK) {
    memcpy(out, (const void *)dst.data, inlen);
    if (!decrypt) {
      memcpy(tag, (const unsigned char *)dst.data + inlen, 16);
    }
    rc = 1;
  }

out:
  if (ab.handle) zz9k_free_shared(ctx, ab.handle);
  if (nb.handle) zz9k_free_shared(ctx, nb.handle);
  if (kb.handle) zz9k_free_shared(ctx, kb.handle);
  if (dst.handle) zz9k_free_shared(ctx, dst.handle);
  if (src.handle) zz9k_free_shared(ctx, src.handle);
  return rc;
}

int zz9k_offload_verify(void *vctx, unsigned int algorithm,
                        const unsigned char *hash, const unsigned char *sig,
                        unsigned int siglen, const unsigned char *key,
                        unsigned int keylen, int *valid)
{
  ZZ9KContext *ctx = (ZZ9KContext *)vctx;
  ZZ9KSharedBuffer hb, sb, kb;
  ZZ9KCryptoVerifyDesc desc;
  int rc = -1;

  memset(&hb, 0, sizeof(hb));
  memset(&sb, 0, sizeof(sb));
  memset(&kb, 0, sizeof(kb));

  if (zz9k_alloc_shared(ctx, 32U, ZZ9K_OFFLOAD_ALIGN, 0, &hb) !=
          ZZ9K_STATUS_OK ||
      zz9k_alloc_shared(ctx, siglen, ZZ9K_OFFLOAD_ALIGN, 0, &sb) !=
          ZZ9K_STATUS_OK ||
      zz9k_alloc_shared(ctx, keylen, ZZ9K_OFFLOAD_ALIGN, 0, &kb) !=
          ZZ9K_STATUS_OK) {
    goto out;
  }
  memcpy((void *)hb.data, hash, 32);
  memcpy((void *)sb.data, sig, siglen);
  memcpy((void *)kb.data, key, keylen);
  if (!zz9k_crypto_build_verify_desc(&desc, algorithm, hb.handle, 0, 32U,
                                     sb.handle, 0, siglen, kb.handle, 0,
                                     keylen)) {
    goto out;
  }
  if (zz9k_crypto_verify(ctx, &desc, valid) == ZZ9K_STATUS_OK) {
    rc = 1;
  }

out:
  if (kb.handle) zz9k_free_shared(ctx, kb.handle);
  if (sb.handle) zz9k_free_shared(ctx, sb.handle);
  if (hb.handle) zz9k_free_shared(ctx, hb.handle);
  return rc;
}
