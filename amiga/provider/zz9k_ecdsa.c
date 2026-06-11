/*
 * ZZ9000 OpenSSL provider — EC (P-256) public-key management and ECDSA
 * signature verification.
 *
 * The KEYMGMT holds a 65-byte uncompressed P-256 public point. The SIGNATURE
 * verify decodes the DER ECDSA-Sig-Value into raw r||s and calls
 * zz9k_prov_ecdsa_verify() (the offload hook; software reference here and on
 * the host). Only P-256 with a 32-byte (SHA-256) digest is supported; anything
 * else falls through to the default provider via the property query.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_prov_local.h"

#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/crypto.h>

#include "zz9k-crypto-soft.h"
#include "zz9k_offload.h"

#include <string.h>

#define ZZ9K_P256_POINT_LEN 65
#define ZZ9K_P256_SCALAR_LEN 32

static int zz9k_prov_ecdsa_verify(const unsigned char r[32],
                                  const unsigned char s[32],
                                  const unsigned char *hash,
                                  const unsigned char point[65],
                                  ZZ9K_PROV_CTX *provctx)
{
#ifdef ZZ9K_PROVIDER_OFFLOAD
  /* Verify on the ZZ9000 (zz9k_crypto_verify) when provctx carries a live
   * context. The signature is passed as r||s (64 bytes) and the key as the
   * 65-byte uncompressed point. A negative return falls through to the
   * software reference. */
  if (provctx != NULL && provctx->sdk_ctx != NULL) {
    unsigned char sig[64];
    int valid = 0;
    memcpy(sig, r, 32);
    memcpy(sig + 32, s, 32);
    if (zz9k_offload_verify(provctx->sdk_ctx, ZZ9K_OFFLOAD_VERIFY_ECDSA_P256,
                            hash, sig, sizeof(sig), point, ZZ9K_P256_POINT_LEN,
                            &valid) >= 0) {
      return valid;
    }
  }
#else
  (void)provctx;
#endif
  return zz9k_soft_ecdsa_verify_p256(r, s, hash, point);
}

/* Copy a DER INTEGER body (big-endian, possibly with a leading sign 0x00 or
 * shorter than 32 bytes) right-aligned into out[32]. Returns 1 on success. */
static int zz9k_der_int_to_32(const unsigned char *p, size_t len,
                              unsigned char out[32])
{
  while (len > 0 && p[0] == 0x00) {
    p++;
    len--;
  }
  if (len > 32) {
    return 0;
  }
  memset(out, 0, 32);
  memcpy(out + (32 - len), p, len);
  return 1;
}

/* Decode a DER ECDSA-Sig-Value (SEQUENCE { INTEGER r, INTEGER s }) into raw
 * 32-byte r and s. Only the short-form lengths used by P-256 are accepted. */
static int zz9k_decode_ecdsa_sig(const unsigned char *der, size_t derlen,
                                 unsigned char r[32], unsigned char s[32])
{
  size_t pos = 0;
  size_t seqlen, ilen;

  if (derlen < 2 || der[pos++] != 0x30) {
    return 0;
  }
  seqlen = der[pos++];
  if ((seqlen & 0x80) != 0 || pos + seqlen != derlen) {
    return 0;
  }
  if (pos >= derlen || der[pos++] != 0x02) {        /* INTEGER r */
    return 0;
  }
  ilen = der[pos++];
  if (ilen == 0 || pos + ilen > derlen || !zz9k_der_int_to_32(der + pos, ilen, r)) {
    return 0;
  }
  pos += ilen;
  if (pos >= derlen || der[pos++] != 0x02) {        /* INTEGER s */
    return 0;
  }
  ilen = der[pos++];
  if (ilen == 0 || pos + ilen != derlen || !zz9k_der_int_to_32(der + pos, ilen, s)) {
    return 0;
  }
  return 1;
}

/* ---- EC KEYMGMT ---- */

typedef struct {
  ZZ9K_PROV_CTX *provctx;
  unsigned char point[ZZ9K_P256_POINT_LEN];
  int has_pub;
} ZZ9K_EC_KEY;

static void *zz9k_ec_keymgmt_new(void *provctx)
{
  ZZ9K_EC_KEY *key = OPENSSL_zalloc(sizeof(*key));
  if (key != NULL) {
    key->provctx = provctx;
  }
  return key;
}

static void zz9k_ec_keymgmt_free(void *keydata)
{
  if (keydata != NULL) {
    OPENSSL_free(keydata);
  }
}

static int zz9k_ec_keymgmt_has(const void *keydata, int selection)
{
  const ZZ9K_EC_KEY *key = keydata;
  if (key == NULL) {
    return 0;
  }
  if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
    return key->has_pub;
  }
  return 1;
}

static int zz9k_ec_keymgmt_import(void *keydata, int selection,
                                  const OSSL_PARAM params[])
{
  ZZ9K_EC_KEY *key = keydata;
  const OSSL_PARAM *p;

  if (key == NULL) {
    return 0;
  }
  /* Only the P-256 group is handled here; reject others so they fall back. */
  p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_GROUP_NAME);
  if (p != NULL) {
    char group[32];
    char *gp = group;
    if (!OSSL_PARAM_get_utf8_string(p, &gp, sizeof(group))) {
      return 0;
    }
    if (strcmp(group, "prime256v1") != 0 && strcmp(group, "P-256") != 0) {
      return 0;
    }
  }
  if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
    p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PUB_KEY);
    if (p != NULL) {
      void *out = key->point;
      size_t len = 0;
      if (!OSSL_PARAM_get_octet_string(p, &out, ZZ9K_P256_POINT_LEN, &len) ||
          len != ZZ9K_P256_POINT_LEN || key->point[0] != 0x04) {
        return 0;
      }
      key->has_pub = 1;
      return 1;
    }
  }
  return 0;
}

static const OSSL_PARAM *zz9k_ec_keymgmt_import_types(int selection)
{
  static const OSSL_PARAM types[] = {
    OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, NULL, 0),
    OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PUB_KEY, NULL, 0),
    OSSL_PARAM_END
  };
  if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
    return types;
  }
  return NULL;
}

static int zz9k_ec_keymgmt_get_params(void *keydata, OSSL_PARAM params[])
{
  OSSL_PARAM *p;

  (void)keydata;
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_BITS);
  if (p != NULL && !OSSL_PARAM_set_int(p, 256)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_SECURITY_BITS);
  if (p != NULL && !OSSL_PARAM_set_int(p, 128)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_MAX_SIZE);
  if (p != NULL && !OSSL_PARAM_set_int(p, 72)) {   /* max DER ECDSA-Sig size */
    return 0;
  }
  return 1;
}

static const OSSL_PARAM *zz9k_ec_keymgmt_gettable_params(void *provctx)
{
  static const OSSL_PARAM types[] = {
    OSSL_PARAM_int(OSSL_PKEY_PARAM_BITS, NULL),
    OSSL_PARAM_int(OSSL_PKEY_PARAM_SECURITY_BITS, NULL),
    OSSL_PARAM_int(OSSL_PKEY_PARAM_MAX_SIZE, NULL),
    OSSL_PARAM_END
  };
  (void)provctx;
  return types;
}

static const char *zz9k_ec_keymgmt_query_operation_name(int operation_id)
{
  if (operation_id == OSSL_OP_SIGNATURE) {
    return "ECDSA";
  }
  return NULL;
}

const OSSL_DISPATCH zz9k_ec_keymgmt_functions[] = {
  { OSSL_FUNC_KEYMGMT_NEW, (void (*)(void))zz9k_ec_keymgmt_new },
  { OSSL_FUNC_KEYMGMT_FREE, (void (*)(void))zz9k_ec_keymgmt_free },
  { OSSL_FUNC_KEYMGMT_HAS, (void (*)(void))zz9k_ec_keymgmt_has },
  { OSSL_FUNC_KEYMGMT_IMPORT, (void (*)(void))zz9k_ec_keymgmt_import },
  { OSSL_FUNC_KEYMGMT_IMPORT_TYPES,
    (void (*)(void))zz9k_ec_keymgmt_import_types },
  { OSSL_FUNC_KEYMGMT_GET_PARAMS,
    (void (*)(void))zz9k_ec_keymgmt_get_params },
  { OSSL_FUNC_KEYMGMT_GETTABLE_PARAMS,
    (void (*)(void))zz9k_ec_keymgmt_gettable_params },
  { OSSL_FUNC_KEYMGMT_QUERY_OPERATION_NAME,
    (void (*)(void))zz9k_ec_keymgmt_query_operation_name },
  { 0, NULL }
};

/* ---- ECDSA SIGNATURE (verify) ---- */

typedef struct {
  ZZ9K_PROV_CTX *provctx;
  ZZ9K_EC_KEY *key;
} ZZ9K_ECDSA_CTX;

static void *zz9k_ecdsa_newctx(void *provctx, const char *propq)
{
  ZZ9K_ECDSA_CTX *ctx = OPENSSL_zalloc(sizeof(*ctx));
  (void)propq;
  if (ctx != NULL) {
    ctx->provctx = provctx;
  }
  return ctx;
}

static void zz9k_ecdsa_freectx(void *vctx)
{
  OPENSSL_free(vctx);
}

static int zz9k_ecdsa_verify_init(void *vctx, void *provkey,
                                  const OSSL_PARAM params[])
{
  ZZ9K_ECDSA_CTX *ctx = vctx;

  (void)params;
  if (ctx == NULL || provkey == NULL) {
    return 0;
  }
  ctx->key = provkey;
  return 1;
}

static int zz9k_ecdsa_verify(void *vctx, const unsigned char *sig,
                             size_t siglen, const unsigned char *tbs,
                             size_t tbslen)
{
  ZZ9K_ECDSA_CTX *ctx = vctx;
  unsigned char r[32];
  unsigned char s[32];

  if (ctx == NULL || ctx->key == NULL || !ctx->key->has_pub) {
    return 0;
  }
  if (tbslen != 32) {              /* P-256 + SHA-256 only */
    return 0;
  }
  if (!zz9k_decode_ecdsa_sig(sig, siglen, r, s)) {
    return 0;
  }
  return zz9k_prov_ecdsa_verify(r, s, tbs, ctx->key->point, ctx->provctx);
}

static int zz9k_ecdsa_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
  (void)vctx;
  (void)params;             /* digest is fixed to SHA-256 (tbslen checked) */
  return 1;
}

static const OSSL_PARAM *zz9k_ecdsa_settable_ctx_params(void *vctx,
                                                        void *provctx)
{
  static const OSSL_PARAM types[] = { OSSL_PARAM_END };
  (void)vctx;
  (void)provctx;
  return types;
}

const OSSL_DISPATCH zz9k_ecdsa_signature_functions[] = {
  { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))zz9k_ecdsa_newctx },
  { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))zz9k_ecdsa_freectx },
  { OSSL_FUNC_SIGNATURE_VERIFY_INIT, (void (*)(void))zz9k_ecdsa_verify_init },
  { OSSL_FUNC_SIGNATURE_VERIFY, (void (*)(void))zz9k_ecdsa_verify },
  { OSSL_FUNC_SIGNATURE_SET_CTX_PARAMS,
    (void (*)(void))zz9k_ecdsa_set_ctx_params },
  { OSSL_FUNC_SIGNATURE_SETTABLE_CTX_PARAMS,
    (void (*)(void))zz9k_ecdsa_settable_ctx_params },
  { 0, NULL }
};
/* The SIGNATURE OSSL_ALGORITHM table (ECDSA + RSA) is assembled in
 * zz9k_algorithms.c. */
