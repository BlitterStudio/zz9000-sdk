/*
 * ZZ9000 OpenSSL provider — X25519 key management and key exchange.
 *
 * A minimal KEYMGMT holds a 32-byte X25519 key (private scalar and/or public
 * u-coordinate), and a KEYEXCH performs the ECDH derive. The derive routes
 * through zz9k_prov_x25519(), which offloads to the ZZ9000 when available and
 * otherwise uses the SDK's portable software reference (the same code the
 * firmware was validated against). Public keys are derived from the private
 * scalar on import so key objects are self-consistent.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_prov_local.h"

#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/crypto.h>

#include "zz9k-crypto-soft.h"

#include <string.h>

#define ZZ9K_X25519_KEYLEN 32

/* The Curve25519 base point u = 9. */
static const unsigned char zz9k_x25519_basepoint[ZZ9K_X25519_KEYLEN] = { 9 };

int zz9k_prov_x25519(unsigned char out[32], const unsigned char scalar[32],
                     const unsigned char point[32], ZZ9K_PROV_CTX *provctx)
{
  /* Hardware offload hook: on the Amiga, when provctx->sdk_ctx is an open
   * ZZ9000 context and the X25519 service flag is set, this is where the
   * zz9k_crypto_kx() call is wired in (Phase 4.5). Until then, and always on
   * the host, use the portable software reference. */
  (void)provctx;
  return zz9k_soft_x25519(out, scalar, point);
}

/* ---- KEYMGMT ---- */

typedef struct {
  ZZ9K_PROV_CTX *provctx;
  unsigned char priv[ZZ9K_X25519_KEYLEN];
  unsigned char pub[ZZ9K_X25519_KEYLEN];
  int has_priv;
  int has_pub;
} ZZ9K_X25519_KEY;

static void *zz9k_x25519_keymgmt_new(void *provctx)
{
  ZZ9K_X25519_KEY *key = OPENSSL_zalloc(sizeof(*key));
  if (key != NULL) {
    key->provctx = provctx;
  }
  return key;
}

static void zz9k_x25519_keymgmt_free(void *keydata)
{
  if (keydata != NULL) {
    OPENSSL_cleanse(keydata, sizeof(ZZ9K_X25519_KEY));
    OPENSSL_free(keydata);
  }
}

static int zz9k_x25519_keymgmt_has(const void *keydata, int selection)
{
  const ZZ9K_X25519_KEY *key = keydata;
  int ok = 1;

  if (key == NULL) {
    return 0;
  }
  if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
    ok = ok && key->has_pub;
  }
  if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0) {
    ok = ok && key->has_priv;
  }
  return ok;
}

static int zz9k_x25519_keymgmt_import(void *keydata, int selection,
                                      const OSSL_PARAM params[])
{
  ZZ9K_X25519_KEY *key = keydata;
  const OSSL_PARAM *p;
  int imported = 0;

  if (key == NULL) {
    return 0;
  }
  if ((selection & OSSL_KEYMGMT_SELECT_PRIVATE_KEY) != 0) {
    p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PRIV_KEY);
    if (p != NULL) {
      void *out = key->priv;
      size_t len = 0;
      if (!OSSL_PARAM_get_octet_string(p, &out, ZZ9K_X25519_KEYLEN, &len) ||
          len != ZZ9K_X25519_KEYLEN) {
        return 0;
      }
      key->has_priv = 1;
      /* Derive the public key so the object is a self-consistent keypair. */
      if (zz9k_prov_x25519(key->pub, key->priv, zz9k_x25519_basepoint,
                           key->provctx)) {
        key->has_pub = 1;
      }
      imported = 1;
    }
  }
  if ((selection & OSSL_KEYMGMT_SELECT_PUBLIC_KEY) != 0) {
    p = OSSL_PARAM_locate_const(params, OSSL_PKEY_PARAM_PUB_KEY);
    if (p != NULL) {
      void *out = key->pub;
      size_t len = 0;
      if (!OSSL_PARAM_get_octet_string(p, &out, ZZ9K_X25519_KEYLEN, &len) ||
          len != ZZ9K_X25519_KEYLEN) {
        return 0;
      }
      key->has_pub = 1;
      imported = 1;
    }
  }
  return imported;
}

static const OSSL_PARAM *zz9k_x25519_keymgmt_import_types(int selection)
{
  static const OSSL_PARAM types[] = {
    OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PRIV_KEY, NULL, 0),
    OSSL_PARAM_octet_string(OSSL_PKEY_PARAM_PUB_KEY, NULL, 0),
    OSSL_PARAM_END
  };
  if ((selection & OSSL_KEYMGMT_SELECT_KEYPAIR) != 0) {
    return types;
  }
  return NULL;
}

static int zz9k_x25519_keymgmt_get_params(void *keydata, OSSL_PARAM params[])
{
  OSSL_PARAM *p;

  (void)keydata;
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_BITS);
  if (p != NULL && !OSSL_PARAM_set_int(p, 253)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_SECURITY_BITS);
  if (p != NULL && !OSSL_PARAM_set_int(p, 128)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PKEY_PARAM_MAX_SIZE);
  if (p != NULL && !OSSL_PARAM_set_int(p, ZZ9K_X25519_KEYLEN)) {
    return 0;
  }
  return 1;
}

static const OSSL_PARAM *zz9k_x25519_keymgmt_gettable_params(void *provctx)
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

static const char *zz9k_x25519_keymgmt_query_operation_name(int operation_id)
{
  if (operation_id == OSSL_OP_KEYEXCH) {
    return "X25519";
  }
  return NULL;
}

static const OSSL_DISPATCH zz9k_x25519_keymgmt_functions[] = {
  { OSSL_FUNC_KEYMGMT_NEW, (void (*)(void))zz9k_x25519_keymgmt_new },
  { OSSL_FUNC_KEYMGMT_FREE, (void (*)(void))zz9k_x25519_keymgmt_free },
  { OSSL_FUNC_KEYMGMT_HAS, (void (*)(void))zz9k_x25519_keymgmt_has },
  { OSSL_FUNC_KEYMGMT_IMPORT, (void (*)(void))zz9k_x25519_keymgmt_import },
  { OSSL_FUNC_KEYMGMT_IMPORT_TYPES,
    (void (*)(void))zz9k_x25519_keymgmt_import_types },
  { OSSL_FUNC_KEYMGMT_GET_PARAMS,
    (void (*)(void))zz9k_x25519_keymgmt_get_params },
  { OSSL_FUNC_KEYMGMT_GETTABLE_PARAMS,
    (void (*)(void))zz9k_x25519_keymgmt_gettable_params },
  { OSSL_FUNC_KEYMGMT_QUERY_OPERATION_NAME,
    (void (*)(void))zz9k_x25519_keymgmt_query_operation_name },
  { 0, NULL }
};

/* ---- KEYEXCH ---- */

typedef struct {
  ZZ9K_PROV_CTX *provctx;
  ZZ9K_X25519_KEY *key;   /* our key (provides the private scalar) */
  ZZ9K_X25519_KEY *peer;  /* peer key (provides the public point) */
} ZZ9K_X25519_EXCH;

static void *zz9k_x25519_exch_newctx(void *provctx)
{
  ZZ9K_X25519_EXCH *ctx = OPENSSL_zalloc(sizeof(*ctx));
  if (ctx != NULL) {
    ctx->provctx = provctx;
  }
  return ctx;
}

static int zz9k_x25519_exch_init(void *vctx, void *provkey,
                                 const OSSL_PARAM params[])
{
  ZZ9K_X25519_EXCH *ctx = vctx;

  (void)params;
  if (ctx == NULL || provkey == NULL) {
    return 0;
  }
  ctx->key = provkey;
  return 1;
}

static int zz9k_x25519_exch_set_peer(void *vctx, void *provkey)
{
  ZZ9K_X25519_EXCH *ctx = vctx;

  if (ctx == NULL || provkey == NULL) {
    return 0;
  }
  ctx->peer = provkey;
  return 1;
}

static int zz9k_x25519_exch_derive(void *vctx, unsigned char *secret,
                                   size_t *secretlen, size_t outlen)
{
  ZZ9K_X25519_EXCH *ctx = vctx;

  if (ctx == NULL || ctx->key == NULL || ctx->peer == NULL) {
    return 0;
  }
  if (!ctx->key->has_priv || !ctx->peer->has_pub) {
    return 0;
  }
  if (secret == NULL) {
    *secretlen = ZZ9K_X25519_KEYLEN;
    return 1;
  }
  if (outlen < ZZ9K_X25519_KEYLEN) {
    return 0;
  }
  if (!zz9k_prov_x25519(secret, ctx->key->priv, ctx->peer->pub, ctx->provctx)) {
    return 0;
  }
  *secretlen = ZZ9K_X25519_KEYLEN;
  return 1;
}

static void zz9k_x25519_exch_freectx(void *vctx)
{
  OPENSSL_free(vctx);
}

static const OSSL_DISPATCH zz9k_x25519_keyexch_functions[] = {
  { OSSL_FUNC_KEYEXCH_NEWCTX, (void (*)(void))zz9k_x25519_exch_newctx },
  { OSSL_FUNC_KEYEXCH_INIT, (void (*)(void))zz9k_x25519_exch_init },
  { OSSL_FUNC_KEYEXCH_SET_PEER, (void (*)(void))zz9k_x25519_exch_set_peer },
  { OSSL_FUNC_KEYEXCH_DERIVE, (void (*)(void))zz9k_x25519_exch_derive },
  { OSSL_FUNC_KEYEXCH_FREECTX, (void (*)(void))zz9k_x25519_exch_freectx },
  { 0, NULL }
};

/* ---- Algorithm tables ---- */

const OSSL_ALGORITHM zz9k_keymgmt_algorithms[] = {
  { "X25519", "provider=zz9000", zz9k_x25519_keymgmt_functions,
    "ZZ9000 X25519 key management" },
  { NULL, NULL, NULL, NULL }
};

const OSSL_ALGORITHM zz9k_keyexch_algorithms[] = {
  { "X25519", "provider=zz9000", zz9k_x25519_keyexch_functions,
    "ZZ9000 X25519 key exchange" },
  { NULL, NULL, NULL, NULL }
};
