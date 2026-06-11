/*
 * ZZ9000 OpenSSL provider — AmiSSL registration helpers.
 *
 * Glue that registers the built-in provider (zz9k_provider_init) with AmiSSL's
 * default library context and makes it the preferred backend. The provider
 * sources themselves are portable OpenSSL 3 code; this file is the small bit of
 * application-side wiring an AmiSSL program performs after InitAmiSSL().
 *
 * Build this together with the provider objects (zz9k_provider.c,
 * zz9k_algorithms.c, zz9k_x25519.c, zz9k_aead.c, zz9k_ecdsa.c, zz9k_rsa.c,
 * zz9k_offload.c and tools/zz9k-crypto-soft.c) and define ZZ9K_PROVIDER_OFFLOAD
 * so the operations route to the hardware.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_amissl.h"
#include "zz9k_provider.h"

#include <openssl/provider.h>
#include <openssl/evp.h>

/* Provider handles are kept so unregister() can unload them. The default
 * provider is loaded explicitly to guarantee a software fallback for the
 * operations the ZZ9000 does not implement (or declines at runtime). */
static OSSL_PROVIDER *zz9k_provider_handle = NULL;
static OSSL_PROVIDER *zz9k_default_handle = NULL;

int zz9k_amissl_register(void)
{
  if (zz9k_provider_handle != NULL) {
    return 1;   /* already registered */
  }
  if (!OSSL_PROVIDER_add_builtin(NULL, ZZ9K_PROVIDER_NAME,
                                 zz9k_provider_init)) {
    return 0;
  }
  /* Keep the default provider available for fallback. On a fresh library
   * context it is not loaded automatically once another provider is loaded. */
  if (zz9k_default_handle == NULL) {
    zz9k_default_handle = OSSL_PROVIDER_load(NULL, "default");
    if (zz9k_default_handle == NULL) {
      return 0;
    }
  }
  zz9k_provider_handle = OSSL_PROVIDER_load(NULL, ZZ9K_PROVIDER_NAME);
  if (zz9k_provider_handle == NULL) {
    OSSL_PROVIDER_unload(zz9k_default_handle);
    zz9k_default_handle = NULL;
    return 0;
  }
  /* "?provider=zz9000" prefers our provider but allows fallback to any other
   * loaded provider for operations we do not advertise. */
  if (!EVP_set_default_properties(NULL, "?provider=" ZZ9K_PROVIDER_NAME)) {
    zz9k_amissl_unregister();
    return 0;
  }
  return 1;
}

void zz9k_amissl_unregister(void)
{
  /* Drop the property preference first so nothing fetches against an
   * about-to-be-unloaded provider. */
  (void)EVP_set_default_properties(NULL, "");
  if (zz9k_provider_handle != NULL) {
    OSSL_PROVIDER_unload(zz9k_provider_handle);
    zz9k_provider_handle = NULL;
  }
  if (zz9k_default_handle != NULL) {
    OSSL_PROVIDER_unload(zz9k_default_handle);
    zz9k_default_handle = NULL;
  }
}
