/*
 * ZZ9000 OpenSSL 3.x provider — skeleton.
 *
 * This first cut implements only the provider-level dispatch (parameters,
 * teardown, and an empty operation query). It registers and loads cleanly but
 * advertises no algorithms yet; the key-exchange, cipher and signature
 * operations are layered on in later steps. Keeping this stage isolated lets
 * the registration boundary be validated on its own against host OpenSSL 3,
 * which is the same provider API AmiSSL 5.x (OpenSSL 3.6) exposes on the Amiga.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_provider.h"
#include "zz9k_prov_local.h"

#include <openssl/core_dispatch.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/crypto.h>

#ifdef ZZ9K_PROVIDER_OFFLOAD
#include "zz9k/host.h"
#include "zz9k/abi.h"
#endif

#define ZZ9K_PROVIDER_VERSION "0.1.0"

static const OSSL_PARAM zz9k_param_types[] = {
  OSSL_PARAM_DEFN(OSSL_PROV_PARAM_NAME, OSSL_PARAM_UTF8_PTR, NULL, 0),
  OSSL_PARAM_DEFN(OSSL_PROV_PARAM_VERSION, OSSL_PARAM_UTF8_PTR, NULL, 0),
  OSSL_PARAM_DEFN(OSSL_PROV_PARAM_BUILDINFO, OSSL_PARAM_UTF8_PTR, NULL, 0),
  OSSL_PARAM_DEFN(OSSL_PROV_PARAM_STATUS, OSSL_PARAM_INTEGER, NULL, 0),
  OSSL_PARAM_END
};

static const OSSL_PARAM *zz9k_gettable_params(void *provctx)
{
  (void)provctx;
  return zz9k_param_types;
}

static int zz9k_get_params(void *provctx, OSSL_PARAM params[])
{
  OSSL_PARAM *p;

  (void)provctx;
  p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_NAME);
  if (p != NULL &&
      !OSSL_PARAM_set_utf8_ptr(p, "ZZ9000 crypto offload provider")) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_VERSION);
  if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, ZZ9K_PROVIDER_VERSION)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_BUILDINFO);
  if (p != NULL && !OSSL_PARAM_set_utf8_ptr(p, ZZ9K_PROVIDER_VERSION)) {
    return 0;
  }
  p = OSSL_PARAM_locate(params, OSSL_PROV_PARAM_STATUS);
  if (p != NULL && !OSSL_PARAM_set_int(p, 1)) {   /* 1 = operational */
    return 0;
  }
  return 1;
}

/* Advertise the operations the provider implements. Anything not listed here
 * falls through to the default (software) provider via the "?provider=zz9000"
 * property query. */
static const OSSL_ALGORITHM *zz9k_query_operation(void *provctx,
                                                  int operation_id,
                                                  int *no_store)
{
  (void)provctx;
  *no_store = 0;
  switch (operation_id) {
  case OSSL_OP_KEYMGMT:
    return zz9k_keymgmt_algorithms;
  case OSSL_OP_KEYEXCH:
    return zz9k_keyexch_algorithms;
  case OSSL_OP_CIPHER:
    return zz9k_cipher_algorithms;
  case OSSL_OP_SIGNATURE:
    return zz9k_signature_algorithms;
  default:
    return NULL;
  }
}

static void zz9k_teardown(void *provctx)
{
#ifdef ZZ9K_PROVIDER_OFFLOAD
  ZZ9K_PROV_CTX *ctx = (ZZ9K_PROV_CTX *)provctx;
  if (ctx != NULL && ctx->sdk_ctx != NULL) {
    zz9k_close((ZZ9KContext *)ctx->sdk_ctx);
    ctx->sdk_ctx = NULL;
  }
#endif
  OPENSSL_free(provctx);
}

static const OSSL_DISPATCH zz9k_dispatch_table[] = {
  { OSSL_FUNC_PROVIDER_TEARDOWN, (void (*)(void))zz9k_teardown },
  { OSSL_FUNC_PROVIDER_GETTABLE_PARAMS, (void (*)(void))zz9k_gettable_params },
  { OSSL_FUNC_PROVIDER_GET_PARAMS, (void (*)(void))zz9k_get_params },
  { OSSL_FUNC_PROVIDER_QUERY_OPERATION, (void (*)(void))zz9k_query_operation },
  { 0, NULL }   /* OSSL_DISPATCH terminator (portable across OpenSSL 3.0-3.6) */
};

int zz9k_provider_init(const OSSL_CORE_HANDLE *handle, const OSSL_DISPATCH *in,
                       const OSSL_DISPATCH **out, void **provctx)
{
  ZZ9K_PROV_CTX *ctx;

  (void)in;
  ctx = OPENSSL_zalloc(sizeof(*ctx));
  if (ctx == NULL) {
    return 0;
  }
  ctx->handle = handle;
#ifdef ZZ9K_PROVIDER_OFFLOAD
  /* Open the ZZ9000 once for the provider's lifetime and remember which crypto
   * services the firmware advertises. If the board is absent or unsupported the
   * provider still loads and every operation transparently uses its software
   * reference (the per-op hooks check sdk_ctx before offloading). */
  {
    ZZ9KContext *sdk = NULL;
    if (zz9k_open(&sdk) == ZZ9K_STATUS_OK) {
      ZZ9KServiceInfo svc;
      ctx->sdk_ctx = sdk;
      if (zz9k_query_service(sdk, ZZ9K_SERVICE_CRYPTO, &svc) == ZZ9K_STATUS_OK) {
        ctx->service_flags = svc.flags;
      }
    }
  }
#endif
  *provctx = ctx;
  *out = zz9k_dispatch_table;
  return 1;
}
