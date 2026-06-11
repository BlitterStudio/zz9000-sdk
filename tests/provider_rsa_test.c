/*
 * Phase 4.4b host parity test: RSA PKCS#1 v1.5 (SHA-256) signature
 * verification through the ZZ9000 provider, for RSA-2048 and RSA-3072. A key
 * and a signature are produced with OpenSSL's default provider; the public key
 * is bridged into the ZZ9000 provider's RSA KEYMGMT (via todata/fromdata) and
 * the signature verified through the provider's RSA SIGNATURE op (software
 * reference here, ZZ9000 offload on the Amiga). RSA-3072 exercises the
 * >2048-bit path. A valid signature must verify, a tampered digest must not,
 * and the result must agree with the default provider.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_provider.h"

#include <openssl/provider.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/params.h>

#include <stdio.h>
#include <string.h>

static int verify(OSSL_LIB_CTX *libctx, const char *prop, EVP_PKEY *key,
                  const unsigned char *sig, size_t siglen,
                  const unsigned char *digest)
{
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_pkey(libctx, key, prop);
  int rc = -1;

  if (ctx != NULL && EVP_PKEY_verify_init(ctx) > 0 &&
      EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) > 0) {
    rc = EVP_PKEY_verify(ctx, sig, siglen, digest, 32);
  }
  EVP_PKEY_CTX_free(ctx);
  return rc;
}

static int test_rsa(OSSL_LIB_CTX *libctx, unsigned int bits)
{
  EVP_PKEY *genkey = NULL;
  EVP_PKEY *ourkey = NULL;
  EVP_PKEY_CTX *sctx = NULL;
  EVP_PKEY_CTX *kctx = NULL;
  OSSL_PARAM *params = NULL;
  unsigned char digest[32];
  unsigned char sig[512];
  size_t siglen = sizeof(sig);
  int rc = 1;
  unsigned int i;

  for (i = 0; i < sizeof(digest); i++) {
    digest[i] = (unsigned char)(0x40 + i);
  }

  genkey = EVP_PKEY_Q_keygen(libctx, "provider=default", "RSA", (size_t)bits);
  if (genkey == NULL) {
    printf("FAIL: RSA-%u keygen\n", bits);
    goto done;
  }
  sctx = EVP_PKEY_CTX_new_from_pkey(libctx, genkey, "provider=default");
  if (sctx == NULL || EVP_PKEY_sign_init(sctx) <= 0 ||
      EVP_PKEY_CTX_set_signature_md(sctx, EVP_sha256()) <= 0 ||
      EVP_PKEY_sign(sctx, sig, &siglen, digest, sizeof(digest)) <= 0) {
    printf("FAIL: RSA-%u sign\n", bits);
    goto done;
  }

  /* Bridge the public key into the ZZ9000 provider's RSA KEYMGMT. */
  if (EVP_PKEY_todata(genkey, EVP_PKEY_PUBLIC_KEY, &params) <= 0) {
    printf("FAIL: RSA-%u todata\n", bits);
    goto done;
  }
  kctx = EVP_PKEY_CTX_new_from_name(libctx, "RSA", "provider=zz9000");
  if (kctx == NULL || EVP_PKEY_fromdata_init(kctx) <= 0 ||
      EVP_PKEY_fromdata(kctx, &ourkey, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
    printf("FAIL: RSA-%u import via provider=zz9000\n", bits);
    goto done;
  }

  if (verify(libctx, "provider=zz9000", ourkey, sig, siglen, digest) != 1) {
    printf("FAIL: RSA-%u valid signature not accepted by provider=zz9000\n",
           bits);
    goto done;
  }
  if (verify(libctx, "provider=default", genkey, sig, siglen, digest) != 1) {
    printf("FAIL: RSA-%u valid signature not accepted by default\n", bits);
    goto done;
  }
  digest[0] ^= 0x01;
  if (verify(libctx, "provider=zz9000", ourkey, sig, siglen, digest) == 1) {
    printf("FAIL: RSA-%u tampered digest accepted by provider=zz9000\n", bits);
    goto done;
  }

  printf("  RSA-%u: ok (verify matches default, tamper rejected)\n", bits);
  rc = 0;

done:
  OSSL_PARAM_free(params);
  EVP_PKEY_CTX_free(kctx);
  EVP_PKEY_CTX_free(sctx);
  EVP_PKEY_free(ourkey);
  EVP_PKEY_free(genkey);
  return rc;
}

int main(void)
{
  OSSL_LIB_CTX *libctx = OSSL_LIB_CTX_new();
  int rc = 0;

  if (libctx == NULL ||
      !OSSL_PROVIDER_add_builtin(libctx, ZZ9K_PROVIDER_NAME,
                                 zz9k_provider_init) ||
      OSSL_PROVIDER_load(libctx, ZZ9K_PROVIDER_NAME) == NULL ||
      OSSL_PROVIDER_load(libctx, "default") == NULL) {
    printf("FAIL: provider registration\n");
    return 1;
  }

  rc |= test_rsa(libctx, 2048);
  rc |= test_rsa(libctx, 3072);

  OSSL_LIB_CTX_free(libctx);
  if (rc == 0) {
    printf("provider_rsa_test: passed\n");
  }
  return rc;
}
