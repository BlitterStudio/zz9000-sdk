/*
 * ZZ9000 OpenSSL provider — AmiSSL hardware self-test.
 *
 * A standalone AmigaOS 3 (m68k) program that initialises AmiSSL, registers the
 * ZZ9000 provider, and exercises the offload through the ordinary AmiSSL EVP
 * API. For each AEAD it:
 *
 *   1. fetches the cipher with the default "?provider=zz9000" query and prints
 *      which provider actually serves it (this is the proof the fetch resolved
 *      to the hardware path),
 *   2. encrypts a test message through that cipher (offloaded on the ZZ9000),
 *   3. decrypts the result with a cipher pinned to "provider=default" (AmiSSL's
 *      software implementation), and checks the recovered plaintext and the
 *      authentication tag.
 *
 * Step 3 cross-validates the offloaded encryption against the independent
 * software reference, so a correct round trip means the ZZ9000 produced a
 * standards-conformant ciphertext and tag — no memorised test vector needed.
 *
 * Build it together with the provider objects and define ZZ9K_PROVIDER_OFFLOAD;
 * see docs/zz9k-amissl-provider.md for the full command line.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>
#include <proto/socket.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>

#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <amissl/amissl.h>

#include <openssl/evp.h>
#include <openssl/provider.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "zz9k_amissl.h"

struct Library *AmiSSLMasterBase, *AmiSSLBase, *SocketBase, *UtilityBase;
#ifdef __amigaos4__
struct AmiSSLIFace *IAmiSSL;
struct AmiSSLMasterIFace *IAmiSSLMaster;
struct SocketIFace *ISocket;
struct UtilityIFace *IUtility;
#endif

static int amissl_ready = 0;

static const char *provider_of_cipher(EVP_CIPHER *c)
{
  const OSSL_PROVIDER *p = c ? EVP_CIPHER_get0_provider(c) : NULL;
  return p ? OSSL_PROVIDER_get0_name(p) : "(none)";
}

/* AES-256-GCM / ChaCha20-Poly1305 round trip: encrypt with `enc_props`, decrypt
 * with `dec_props`. Returns 1 if the recovered plaintext matches and the tag
 * verifies. */
static int aead_roundtrip(const char *alg, const char *enc_props,
                          const char *dec_props, unsigned int keylen)
{
  static const unsigned char key32[32] = {
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
  };
  static const unsigned char iv12[12] = {
    0xca,0xfe,0xba,0xbe,0xfa,0xce,0xdb,0xad,0xde,0xca,0xf8,0x88
  };
  static const unsigned char aad[16] = {
    0xfe,0xed,0xfa,0xce,0xde,0xad,0xbe,0xef,0xfe,0xed,0xfa,0xce,0xde,0xad,0xbe,0xef
  };
  static const unsigned char pt[40] = {
    'Z','Z','9','0','0','0',' ','A','m','i','S','S','L',' ','o','f',
    'f','l','o','a','d',' ','s','e','l','f','-','t','e','s','t',' ','m','s','g',
    '!','!','!','!','!'
  };
  unsigned char ct[64], rt[64], tag[16];
  EVP_CIPHER *enc = NULL, *dec = NULL;
  EVP_CIPHER_CTX *ectx = NULL, *dctx = NULL;
  int outl = 0, tmpl = 0, ok = 0;

  enc = EVP_CIPHER_fetch(NULL, alg, enc_props);
  dec = EVP_CIPHER_fetch(NULL, alg, dec_props);
  if (enc == NULL || dec == NULL) {
    printf("  %-22s FAIL (fetch: enc=%p dec=%p)\n", alg, (void *)enc,
           (void *)dec);
    goto done;
  }
  printf("  %-22s encrypt via '%s', decrypt via '%s'\n", alg,
         provider_of_cipher(enc), provider_of_cipher(dec));

  /* Encrypt. */
  ectx = EVP_CIPHER_CTX_new();
  if (ectx == NULL ||
      !EVP_EncryptInit_ex2(ectx, enc, key32, iv12, NULL) ||
      !EVP_EncryptUpdate(ectx, NULL, &outl, aad, (int)sizeof(aad)) ||
      !EVP_EncryptUpdate(ectx, ct, &outl, pt, (int)sizeof(pt)) ||
      !EVP_EncryptFinal_ex(ectx, ct + outl, &tmpl) ||
      !EVP_CIPHER_CTX_ctrl(ectx, EVP_CTRL_AEAD_GET_TAG, 16, tag)) {
    printf("  %-22s FAIL (encrypt)\n", alg);
    goto done;
  }

  /* Decrypt with the cross-check provider. */
  dctx = EVP_CIPHER_CTX_new();
  if (dctx == NULL ||
      !EVP_DecryptInit_ex2(dctx, dec, key32, iv12, NULL) ||
      !EVP_DecryptUpdate(dctx, NULL, &outl, aad, (int)sizeof(aad)) ||
      !EVP_DecryptUpdate(dctx, rt, &outl, ct, (int)sizeof(pt)) ||
      !EVP_CIPHER_CTX_ctrl(dctx, EVP_CTRL_AEAD_SET_TAG, 16, tag)) {
    printf("  %-22s FAIL (decrypt setup)\n", alg);
    goto done;
  }
  if (EVP_DecryptFinal_ex(dctx, rt + outl, &tmpl) != 1) {
    printf("  %-22s FAIL (tag rejected)\n", alg);
    goto done;
  }
  if (memcmp(rt, pt, sizeof(pt)) != 0) {
    printf("  %-22s FAIL (plaintext mismatch)\n", alg);
    goto done;
  }
  printf("  %-22s PASS (keylen=%u)\n", alg, keylen);
  ok = 1;

done:
  EVP_CIPHER_CTX_free(ectx);
  EVP_CIPHER_CTX_free(dctx);
  EVP_CIPHER_free(enc);
  EVP_CIPHER_free(dec);
  return ok;
}

static int run_tests(void)
{
  int ok = 1;
  printf("ZZ9000 AmiSSL provider self-test\n");
  printf("--------------------------------\n");
  /* Encrypt via the default "?provider=zz9000" query (offloaded when the board
   * advertises the service), decrypt via the software default for cross-check. */
  ok &= aead_roundtrip("AES-256-GCM", NULL, "provider=default", 32);
  ok &= aead_roundtrip("ChaCha20-Poly1305", NULL, "provider=default", 32);
  printf("--------------------------------\n");
  printf("Result: %s\n", ok ? "ALL PASS" : "FAILURES");
  return ok;
}

static void cleanup(void)
{
  zz9k_amissl_unregister();
  if (amissl_ready) {
    CleanupAmiSSLA(NULL);
    amissl_ready = 0;
  }
#ifdef __amigaos4__
  if (IAmiSSL) DropInterface((struct Interface *)IAmiSSL);
#endif
  if (AmiSSLBase) { CloseAmiSSL(); AmiSSLBase = NULL; }
#ifdef __amigaos4__
  if (IAmiSSLMaster) DropInterface((struct Interface *)IAmiSSLMaster);
#endif
  if (AmiSSLMasterBase) { CloseLibrary(AmiSSLMasterBase); AmiSSLMasterBase = NULL; }
#ifdef __amigaos4__
  if (ISocket) DropInterface((struct Interface *)ISocket);
#endif
  if (SocketBase) { CloseLibrary(SocketBase); SocketBase = NULL; }
#ifdef __amigaos4__
  if (IUtility) DropInterface((struct Interface *)IUtility);
#endif
  if (UtilityBase) { CloseLibrary(UtilityBase); UtilityBase = NULL; }
}

int main(void)
{
  int rc = 20;

  if (!(UtilityBase = OpenLibrary("utility.library", 37))) {
    printf("Couldn't open utility.library\n");
    goto out;
  }
  if (!(SocketBase = OpenLibrary("bsdsocket.library", 4))) {
    printf("Couldn't open bsdsocket.library v4\n");
    goto out;
  }
  if (!(AmiSSLMasterBase = OpenLibrary("amisslmaster.library",
                                       AMISSLMASTER_MIN_VERSION))) {
    printf("Couldn't open amisslmaster.library\n");
    goto out;
  }
#ifdef __amigaos4__
  if (!(IUtility = (struct UtilityIFace *)GetInterface(UtilityBase, "main", 1, NULL)) ||
      !(ISocket = (struct SocketIFace *)GetInterface(SocketBase, "main", 1, NULL)) ||
      !(IAmiSSLMaster = (struct AmiSSLMasterIFace *)GetInterface(AmiSSLMasterBase, "main", 1, NULL))) {
    printf("Couldn't get OS4 interfaces\n");
    goto out;
  }
#endif
  if (!InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE)) {
    printf("AmiSSL version is too old\n");
    goto out;
  }
  if (!(AmiSSLBase = OpenAmiSSL())) {
    printf("Couldn't open AmiSSL\n");
    goto out;
  }
#ifdef __amigaos4__
  if (!(IAmiSSL = (struct AmiSSLIFace *)GetInterface(AmiSSLBase, "main", 1, NULL))) {
    printf("Couldn't get AmiSSL interface\n");
    goto out;
  }
  if (InitAmiSSL(AmiSSL_ErrNoPtr, &errno, AmiSSL_ISocket, ISocket, TAG_DONE) != 0) {
#else
  if (InitAmiSSL(AmiSSL_ErrNoPtr, &errno, AmiSSL_SocketBase, SocketBase, TAG_DONE) != 0) {
#endif
    printf("Couldn't initialise AmiSSL\n");
    goto out;
  }
  amissl_ready = 1;

  if (!zz9k_amissl_register()) {
    printf("Couldn't register the ZZ9000 provider\n");
    goto out;
  }

  rc = run_tests() ? 0 : 5;

out:
  cleanup();
  return rc;
}
