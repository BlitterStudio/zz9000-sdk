/*
 * ZZ9000 OpenSSL provider — RSA public-key management and RSA PKCS#1 v1.5
 * (SHA-256) signature verification.
 *
 * The KEYMGMT imports the modulus and public exponent (rejecting keys larger
 * than 4096 bits or exponents wider than 32 bits, so they fall back to the
 * default provider). The SIGNATURE verify handles only PKCS#1 v1.5 with
 * SHA-256 and declines other padding (e.g. PSS) or digests so those fall back
 * too. zz9k_prov_rsa_verify() is the offload hook: the ZZ9000 firmware verifies
 * RSA-2048; larger moduli and the host path use the software reference.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_prov_local.h"

#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/crypto.h>
#include <openssl/bn.h>

#include "zz9k-crypto-soft.h"

#include <string.h>

#define ZZ9K_RSA_MAX_BYTES 512        /* 4096-bit modulus */
#define ZZ9K_RSA_PKCS1_PADDING 1      /* mirrors RSA_PKCS1_PADDING */

static int zz9k_prov_rsa_verify(const unsigned char *sig, uint32_t siglen,
                                const unsigned char *hash,
                                const unsigned char *n, uint32_t nbits,
                                uint32_t e, ZZ9K_PROV_CTX *provctx)
{
  /* Offload hook (Phase 4.5): route RSA-2048 to zz9k_crypto_verify when a
   * ZZ9000 context is present; larger sizes stay in software. */
  (void)provctx;
  return zz9k_soft_rsa_verify_pkcs1_sha256(sig, siglen, hash, n, nbits, e);
}

/* ---- RSA KEYMGMT ---- */

typedef struct {
  ZZ9K_PROV_CTX *provctx;
  unsigned char n[ZZ9K_RSA_MAX_BYTES];
  uint32_t nbytes;
  uint32_t e;
  int has_pub;
} ZZ9K_RSA_KEY;

static void *zz9k_rsa_keymgmt_new(void *provctx)
{
  ZZ9K_RSA_KEY *key = OPENSSL_zalloc(sizeof(*key));
  if (key != NULL) {
    key->provctx = provctx;
  }
  return key;
}

static void zz9k_rsa_keymgmt_free(void *keydata)
{
  if (keydata != NULL) {
    OPENSSL_free(keydata);
  }
}

static int zz9k_rsa_keymgmt_has(const void *keydata, int selection)
{
  const ZZ9K_RSA_KEY *key = keydata;
  if (key == NULL) {
    return 0;
  }
  if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
    return key->has_pub;
  }
  return 1;
}

static int zz9k_rsa_keymgmt_import(void *keydata, int selection,
                                   const OSSL_PARAM params[])
{
  ZZ9K_RSA_KEY *key = keydata;
  const OSSL_PARAM *pn;
  const OSSL_PARAM *pe;
  BIGNUM *n = NULL;
  BIGNUM *e = NULL;
  int nbytes;
  int ok = 0;

  if (key == NULL || (selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) == 0) {
    return 0;
  }
  pn = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_RSA_N);
  pe = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_RSA_E);
  if (pn == NULL || pe == NULL) {
    return 0;
  }
  if (!OSSL_PARAM_get_BN(pn, &n) || !OSSL_PARAM_get_BN(pe, &e)) {
    goto end;
  }
  nbytes = BN_num_bytes(n);
  /* Only sizes the software/firmware can verify: <= 4096-bit modulus that is a
   * whole number of 32-bit limbs, and a public exponent that fits in 32 bits.
   * Anything else is left to the default provider. */
  if (nbytes <= 0 || nbytes > ZZ9K_RSA_MAX_BYTES || (nbytes % 4) != 0 ||
      BN_num_bits(e) > 32) {
    goto end;
  }
  if (BN_bn2binpad(n, key->n, nbytes) != nbytes) {
    goto end;
  }
  key->nbytes = (uint32_t)nbytes;
  key->e = (uint32_t)BN_get_word(e);
  key->has_pub = 1;
  ok = 1;

end:
  BN_free(n);
  BN_free(e);
  return ok;
}

static const OSSL_PARAM *zz9k_rsa_keymgmt_import_types(int selection)
{
  static const OSSL_PARAM types[] = {
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_N, NULL, 0),
    OSSL_PARAM_BN(OSSL_PKEY_PARAM_RSA_E, NULL, 0),
    OSSL_PARAM_END
  };
  if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
    return types;
  }
  return NULL;
}

static int zz9k_rsa_keymgmt_get_params(void *keydata, OSSL_PARAM params[])
{
  ZZ9K_RSA_KEY *key = keydata;
  int bits = key != NULL ? (int)(key->nbytes * 8U) : 0;
  OSSL_PARAM *p;

  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_BITS);
  if (p != NULL && !OSSL_PARAM_set_int(p, bits)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_SECURITY_BITS);
  if (p != NULL && !OSSL_PARAM_set_int(p, bits >= 3072 ? 128 : 112)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_MAX_SIZE);
  if (p != NULL && !OSSL_PARAM_set_int(p, (int)(key ? key->nbytes : 0))) {
    return 0;
  }
  return 1;
}

static const OSSL_PARAM *zz9k_rsa_keymgmt_gettable_params(void *provctx)
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

static const char *zz9k_rsa_keymgmt_query_operation_name(int operation_id)
{
  if (operation_id == OSSL_OP_SIGNATURE) {
    return "RSA";
  }
  return NULL;
}

const OSSL_DISPATCH zz9k_rsa_keymgmt_functions[] = {
  { OSSL_FUNC_KEYMGMT_NEW, (void (*)(void))zz9k_rsa_keymgmt_new },
  { OSSL_FUNC_KEYMGMT_FREE, (void (*)(void))zz9k_rsa_keymgmt_free },
  { OSSL_FUNC_KEYMGMT_HAS, (void (*)(void))zz9k_rsa_keymgmt_has },
  { OSSL_FUNC_KEYMGMT_IMPORT, (void (*)(void))zz9k_rsa_keymgmt_import },
  { OSSL_FUNC_KEYMGMT_IMPORT_TYPES,
    (void (*)(void))zz9k_rsa_keymgmt_import_types },
  { OSSL_FUNC_KEYMGMT_GET_PARAMS,
    (void (*)(void))zz9k_rsa_keymgmt_get_params },
  { OSSL_FUNC_KEYMGMT_GETTABLE_PARAMS,
    (void (*)(void))zz9k_rsa_keymgmt_gettable_params },
  { OSSL_FUNC_KEYMGMT_QUERY_OPERATION_NAME,
    (void (*)(void))zz9k_rsa_keymgmt_query_operation_name },
  { 0, NULL }
};

/* ---- RSA SIGNATURE (verify) ---- */

typedef struct {
  ZZ9K_PROV_CTX *provctx;
  ZZ9K_RSA_KEY *key;
} ZZ9K_RSA_SIG_CTX;

static void *zz9k_rsa_sig_newctx(void *provctx, const char *propq)
{
  ZZ9K_RSA_SIG_CTX *ctx = OPENSSL_zalloc(sizeof(*ctx));
  (void)propq;
  if (ctx != NULL) {
    ctx->provctx = provctx;
  }
  return ctx;
}

static void zz9k_rsa_sig_freectx(void *vctx)
{
  OPENSSL_free(vctx);
}

static int zz9k_rsa_sig_set_ctx_params(void *vctx, const OSSL_PARAM params[]);

static int zz9k_rsa_sig_verify_init(void *vctx, void *provkey,
                                    const OSSL_PARAM params[])
{
  ZZ9K_RSA_SIG_CTX *ctx = vctx;

  if (ctx == NULL || provkey == NULL) {
    return 0;
  }
  ctx->key = provkey;
  return zz9k_rsa_sig_set_ctx_params(ctx, params);
}

static int zz9k_rsa_sig_verify(void *vctx, const unsigned char *sig,
                               size_t siglen, const unsigned char *tbs,
                               size_t tbslen)
{
  ZZ9K_RSA_SIG_CTX *ctx = vctx;

  if (ctx == NULL || ctx->key == NULL || !ctx->key->has_pub) {
    return 0;
  }
  if (tbslen != 32 || siglen != ctx->key->nbytes) {
    return 0;
  }
  return zz9k_prov_rsa_verify(sig, (uint32_t)siglen, tbs, ctx->key->n,
                              ctx->key->nbytes * 8U, ctx->key->e,
                              ctx->provctx);
}

static int zz9k_rsa_sig_set_ctx_params(void *vctx, const OSSL_PARAM params[])
{
  const OSSL_PARAM *p;

  (void)vctx;
  if (params == NULL) {
    return 1;
  }
  /* Only PKCS#1 v1.5 padding is offloadable; decline anything else (e.g. PSS)
   * so the operation falls back to the default provider. */
  p = OSSL_PARAM_locate_const(params, OSSL_SIGNATURE_PARAM_PAD_MODE);
  if (p != NULL) {
    int pad = 0;
    if (p->data_type == OSSL_PARAM_INTEGER) {
      if (!OSSL_PARAM_get_int(p, &pad) || pad != ZZ9K_RSA_PKCS1_PADDING) {
        return 0;
      }
    } else if (p->data_type == OSSL_PARAM_UTF8_STRING) {
      if (p->data == NULL || strcmp((const char *)p->data, "pkcs1") != 0) {
        return 0;
      }
    } else {
      return 0;
    }
  }
  /* Only SHA-256 digests are supported. */
  p = OSSL_PARAM_locate_const(params, OSSL_SIGNATURE_PARAM_DIGEST);
  if (p != NULL) {
    if (p->data_type != OSSL_PARAM_UTF8_STRING || p->data == NULL ||
        (strcmp((const char *)p->data, "SHA256") != 0 &&
         strcmp((const char *)p->data, "SHA2-256") != 0)) {
      return 0;
    }
  }
  return 1;
}

static const OSSL_PARAM *zz9k_rsa_sig_settable_ctx_params(void *vctx,
                                                          void *provctx)
{
  static const OSSL_PARAM types[] = {
    OSSL_PARAM_int(OSSL_SIGNATURE_PARAM_PAD_MODE, NULL),
    OSSL_PARAM_utf8_string(OSSL_SIGNATURE_PARAM_DIGEST, NULL, 0),
    OSSL_PARAM_END
  };
  (void)vctx;
  (void)provctx;
  return types;
}

const OSSL_DISPATCH zz9k_rsa_signature_functions[] = {
  { OSSL_FUNC_SIGNATURE_NEWCTX, (void (*)(void))zz9k_rsa_sig_newctx },
  { OSSL_FUNC_SIGNATURE_FREECTX, (void (*)(void))zz9k_rsa_sig_freectx },
  { OSSL_FUNC_SIGNATURE_VERIFY_INIT,
    (void (*)(void))zz9k_rsa_sig_verify_init },
  { OSSL_FUNC_SIGNATURE_VERIFY, (void (*)(void))zz9k_rsa_sig_verify },
  { OSSL_FUNC_SIGNATURE_SET_CTX_PARAMS,
    (void (*)(void))zz9k_rsa_sig_set_ctx_params },
  { OSSL_FUNC_SIGNATURE_SETTABLE_CTX_PARAMS,
    (void (*)(void))zz9k_rsa_sig_settable_ctx_params },
  { 0, NULL }
};
/* The SIGNATURE OSSL_ALGORITHM table (ECDSA + RSA) is assembled in
 * zz9k_algorithms.c. */
