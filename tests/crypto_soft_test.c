/*
 * Known-answer tests for the portable software ChaCha20-Poly1305 reference.
 * Vectors are from RFC 8439 (sections 2.5.2 and 2.8.2).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k-crypto-soft.h"
#include "zz9k-crypto-soft.c"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int bytes_equal(const uint8_t *a, const uint8_t *b, uint32_t length)
{
  uint32_t i;

  for (i = 0; i < length; i++) {
    if (a[i] != b[i]) {
      return 0;
    }
  }
  return 1;
}

/* RFC 8439 section 2.5.2: Poly1305 one-time authenticator. */
static int test_poly1305_rfc8439(void)
{
  static const uint8_t key[32] = {
    0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33,
    0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8,
    0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd,
    0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b
  };
  static const uint8_t message[] =
    "Cryptographic Forum Research Group";
  static const uint8_t expected[16] = {
    0xa8, 0x06, 0x1d, 0xc1, 0x30, 0x51, 0x36, 0xc6,
    0xc2, 0x2b, 0x8b, 0xaf, 0x0c, 0x01, 0x27, 0xa9
  };
  uint8_t tag[16];

  zz9k_soft_poly1305(tag, message, (uint32_t)(sizeof(message) - 1U), key);
  return bytes_equal(tag, expected, 16U) ? 0 : 1;
}

/* RFC 8439 section 2.8.2: ChaCha20-Poly1305 AEAD. */
static const uint8_t aead_key[32] = {
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
  0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
};
static const uint8_t aead_nonce[12] = {
  0x07, 0x00, 0x00, 0x00, 0x40, 0x41,
  0x42, 0x43, 0x44, 0x45, 0x46, 0x47
};
static const uint8_t aead_aad[12] = {
  0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1,
  0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7
};
static const uint8_t aead_plaintext[] =
  "Ladies and Gentlemen of the class of '99: If I could offer "
  "you only one tip for the future, sunscreen would be it.";
static const uint8_t aead_ciphertext[114] = {
  0xd3, 0x1a, 0x8d, 0x34, 0x64, 0x8e, 0x60, 0xdb,
  0x7b, 0x86, 0xaf, 0xbc, 0x53, 0xef, 0x7e, 0xc2,
  0xa4, 0xad, 0xed, 0x51, 0x29, 0x6e, 0x08, 0xfe,
  0xa9, 0xe2, 0xb5, 0xa7, 0x36, 0xee, 0x62, 0xd6,
  0x3d, 0xbe, 0xa4, 0x5e, 0x8c, 0xa9, 0x67, 0x12,
  0x82, 0xfa, 0xfb, 0x69, 0xda, 0x92, 0x72, 0x8b,
  0x1a, 0x71, 0xde, 0x0a, 0x9e, 0x06, 0x0b, 0x29,
  0x05, 0xd6, 0xa5, 0xb6, 0x7e, 0xcd, 0x3b, 0x36,
  0x92, 0xdd, 0xbd, 0x7f, 0x2d, 0x77, 0x8b, 0x8c,
  0x98, 0x03, 0xae, 0xe3, 0x28, 0x09, 0x1b, 0x58,
  0xfa, 0xb3, 0x24, 0xe4, 0xfa, 0xd6, 0x75, 0x94,
  0x55, 0x85, 0x80, 0x8b, 0x48, 0x31, 0xd7, 0xbc,
  0x3f, 0xf4, 0xde, 0xf0, 0x8e, 0x4b, 0x7a, 0x9d,
  0xe5, 0x76, 0xd2, 0x65, 0x86, 0xce, 0xc6, 0x4b,
  0x61, 0x16
};
static const uint8_t aead_tag[16] = {
  0x1a, 0xe1, 0x0b, 0x59, 0x4f, 0x09, 0xe2, 0x6a,
  0x7e, 0x90, 0x2e, 0xcb, 0xd0, 0x60, 0x06, 0x91
};

static int test_aead_encrypt_rfc8439(void)
{
  uint8_t ciphertext[114];
  uint8_t tag[16];

  zz9k_soft_chacha20_poly1305_encrypt(
      ciphertext, tag, aead_plaintext,
      (uint32_t)(sizeof(aead_plaintext) - 1U), aead_aad,
      (uint32_t)sizeof(aead_aad), aead_key, aead_nonce);

  if (!bytes_equal(ciphertext, aead_ciphertext, 114U)) {
    return 1;
  }
  if (!bytes_equal(tag, aead_tag, 16U)) {
    return 2;
  }
  return 0;
}

static int test_aead_decrypt_rfc8439(void)
{
  uint8_t plaintext[114];
  int ok;

  ok = zz9k_soft_chacha20_poly1305_decrypt(
      plaintext, aead_ciphertext, 114U, aead_aad,
      (uint32_t)sizeof(aead_aad), aead_tag, aead_key, aead_nonce);
  if (!ok) {
    return 1;
  }
  if (!bytes_equal(plaintext, aead_plaintext, 114U)) {
    return 2;
  }
  return 0;
}

static int test_aead_decrypt_rejects_tampered_tag(void)
{
  uint8_t plaintext[114];
  uint8_t tampered[16];
  int ok;

  memcpy(tampered, aead_tag, 16U);
  tampered[0] ^= 0x01U;
  ok = zz9k_soft_chacha20_poly1305_decrypt(
      plaintext, aead_ciphertext, 114U, aead_aad,
      (uint32_t)sizeof(aead_aad), tampered, aead_key, aead_nonce);
  return ok ? 1 : 0;
}

static int test_aead_roundtrip_no_aad(void)
{
  static const uint8_t key[32] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x0f, 0x1e, 0x2d, 0x3c, 0x4b, 0x5a, 0x69, 0x78,
    0x87, 0x96, 0xa5, 0xb4, 0xc3, 0xd2, 0xe1, 0xf0
  };
  static const uint8_t nonce[12] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c
  };
  uint8_t plaintext[200];
  uint8_t ciphertext[200];
  uint8_t recovered[200];
  uint8_t tag[16];
  uint32_t i;
  int ok;

  for (i = 0; i < sizeof(plaintext); i++) {
    plaintext[i] = (uint8_t)((i * 7U + 3U) & 0xffU);
  }

  zz9k_soft_chacha20_poly1305_encrypt(
      ciphertext, tag, plaintext, (uint32_t)sizeof(plaintext), 0, 0U,
      key, nonce);
  ok = zz9k_soft_chacha20_poly1305_decrypt(
      recovered, ciphertext, (uint32_t)sizeof(ciphertext), 0, 0U, tag,
      key, nonce);
  if (!ok) {
    return 1;
  }
  if (!bytes_equal(recovered, plaintext, (uint32_t)sizeof(plaintext))) {
    return 2;
  }
  return 0;
}

int main(void)
{
  int rc;

  rc = test_poly1305_rfc8439();
  if (rc) {
    printf("test_poly1305_rfc8439 failed: %d\n", rc);
    return 10 + rc;
  }
  rc = test_aead_encrypt_rfc8439();
  if (rc) {
    printf("test_aead_encrypt_rfc8439 failed: %d\n", rc);
    return 20 + rc;
  }
  rc = test_aead_decrypt_rfc8439();
  if (rc) {
    printf("test_aead_decrypt_rfc8439 failed: %d\n", rc);
    return 30 + rc;
  }
  rc = test_aead_decrypt_rejects_tampered_tag();
  if (rc) {
    printf("test_aead_decrypt_rejects_tampered_tag failed: %d\n", rc);
    return 40 + rc;
  }
  rc = test_aead_roundtrip_no_aad();
  if (rc) {
    printf("test_aead_roundtrip_no_aad failed: %d\n", rc);
    return 50 + rc;
  }

  printf("crypto_soft_test: all known-answer tests passed\n");
  return 0;
}
