/*
 * Phase 4.4a host parity test: ECDSA-P256 signature verification through the
 * ZZ9000 provider. A P-256 key and a signature over a fixed 32-byte digest are
 * produced with OpenSSL's default provider; the public point is then imported
 * into the ZZ9000 provider's EC KEYMGMT and the signature verified through the
 * provider's ECDSA SIGNATURE op (which decodes the DER r||s and calls the
 * software reference here, the ZZ9000 offload on the Amiga). A valid signature
 * must verify, a tampered digest must not, and the result must agree with the
 * default provider.
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
                  const unsigned char *digest, size_t digestlen)
{
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_pkey(libctx, key, prop);
  int rc = -1;

  if (ctx != NULL && EVP_PKEY_verify_init(ctx) > 0) {
    rc = EVP_PKEY_verify(ctx, sig, siglen, digest, digestlen);
  }
  EVP_PKEY_CTX_free(ctx);
  return rc;   /* 1 = valid, 0 = invalid, <0 = error */
}

int main(void)
{
  OSSL_LIB_CTX *libctx = OSSL_LIB_CTX_new();
  EVP_PKEY *genkey = NULL;
  EVP_PKEY *ourkey = NULL;
  EVP_PKEY_CTX *sctx = NULL;
  EVP_PKEY_CTX *kctx = NULL;
  unsigned char digest[32];
  unsigned char sig[80];
  size_t siglen = sizeof(sig);
  unsigned char point[65];
  size_t plen = 0;
  OSSL_PARAM params[3];
  int rc = 1;
  unsigned int i;

  for (i = 0; i < sizeof(digest); i++) {
    digest[i] = (unsigned char)(0x20 + i);
  }

  if (libctx == NULL ||
      !OSSL_PROVIDER_add_builtin(libctx, ZZ9K_PROVIDER_NAME,
                                 zz9k_provider_init) ||
      OSSL_PROVIDER_load(libctx, ZZ9K_PROVIDER_NAME) == NULL ||
      OSSL_PROVIDER_load(libctx, "default") == NULL) {
    printf("FAIL: provider registration\n");
    goto done;
  }

  /* Generate a P-256 key and sign the digest with the default provider. Our
   * EC KEYMGMT is verify-only (no keygen), so keygen is pinned to default. */
  genkey = EVP_PKEY_Q_keygen(libctx, "provider=default", "EC", "P-256");
  if (genkey == NULL) {
    printf("FAIL: keygen\n");
    goto done;
  }
  sctx = EVP_PKEY_CTX_new_from_pkey(libctx, genkey, "provider=default");
  if (sctx == NULL || EVP_PKEY_sign_init(sctx) <= 0 ||
      EVP_PKEY_sign(sctx, sig, &siglen, digest, sizeof(digest)) <= 0) {
    printf("FAIL: sign\n");
    goto done;
  }
  if (!EVP_PKEY_get_octet_string_param(genkey, OSSL_PKEY_PARAM_PUB_KEY, point,
                                       sizeof(point), &plen) ||
      plen != sizeof(point)) {
    printf("FAIL: extract public point\n");
    goto done;
  }

  /* Import the public point into the ZZ9000 provider's EC KEYMGMT. */
  kctx = EVP_PKEY_CTX_new_from_name(libctx, "EC", "provider=zz9000");
  params[0] = OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                               (char *)"P-256", 5);
  params[1] = OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY,
                                                point, plen);
  params[2] = OSSL_PARAM_construct_end();
  if (kctx == NULL || EVP_PKEY_fromdata_init(kctx) <= 0 ||
      EVP_PKEY_fromdata(kctx, &ourkey, EVP_PKEY_PUBLIC_KEY, params) <= 0) {
    printf("FAIL: EC public key import via provider=zz9000\n");
    goto done;
  }

  /* Valid signature must verify through our provider and the default one. */
  if (verify(libctx, "provider=zz9000", ourkey, sig, siglen, digest, 32) != 1) {
    printf("FAIL: valid signature not accepted by provider=zz9000\n");
    goto done;
  }
  if (verify(libctx, "provider=default", genkey, sig, siglen, digest, 32) != 1) {
    printf("FAIL: valid signature not accepted by provider=default\n");
    goto done;
  }

  /* A non-SHA-256-sized digest (48 bytes, as an ECDSA-P256-with-SHA-384 chain
   * signature presents) must NOT be rejected by the accelerated 32-byte-only
   * path: our EC verify delegates it to the default provider. Sign a 48-byte
   * "digest" with the same P-256 key and verify through provider=zz9000. */
  {
    unsigned char digest48[48];
    unsigned char sig48[80];
    size_t sig48len = sizeof(sig48);
    EVP_PKEY_CTX *s48 = EVP_PKEY_CTX_new_from_pkey(libctx, genkey,
                                                   "provider=default");

    for (i = 0; i < sizeof(digest48); i++) {
      digest48[i] = (unsigned char)(0x40 + i);
    }
    if (s48 == NULL || EVP_PKEY_sign_init(s48) <= 0 ||
        EVP_PKEY_sign(s48, sig48, &sig48len, digest48, sizeof(digest48)) <= 0) {
      EVP_PKEY_CTX_free(s48);
      printf("FAIL: sign 48-byte digest\n");
      goto done;
    }
    EVP_PKEY_CTX_free(s48);
    if (verify(libctx, "provider=zz9000", ourkey, sig48, sig48len, digest48,
               48) != 1) {
      printf("FAIL: 48-byte-digest signature rejected by provider=zz9000 "
             "(delegation fallback)\n");
      goto done;
    }
    digest48[0] ^= 0x01;
    if (verify(libctx, "provider=zz9000", ourkey, sig48, sig48len, digest48,
               48) == 1) {
      printf("FAIL: tampered 48-byte digest accepted by provider=zz9000\n");
      goto done;
    }
  }

  /* A tampered digest must be rejected. */
  digest[0] ^= 0x01;
  if (verify(libctx, "provider=zz9000", ourkey, sig, siglen, digest, 32) == 1) {
    printf("FAIL: tampered digest accepted by provider=zz9000\n");
    goto done;
  }

  printf("provider_ecdsa_test: passed (verify matches default, tamper "
         "rejected)\n");
  rc = 0;

done:
  EVP_PKEY_CTX_free(kctx);
  EVP_PKEY_CTX_free(sctx);
  EVP_PKEY_free(ourkey);
  EVP_PKEY_free(genkey);
  OSSL_LIB_CTX_free(libctx);
  return rc;
}
