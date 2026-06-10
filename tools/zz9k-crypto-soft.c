/*
 * Portable software reference for ChaCha20, Poly1305, and the
 * ChaCha20-Poly1305 AEAD (RFC 8439).
 *
 * The Poly1305 core uses the well-known radix-2^26 "donna" formulation,
 * chosen because it relies only on 32x32->64 multiplies that an m68k
 * compiler can emit, with no 128-bit intermediate.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k-crypto-soft.h"

#include <string.h>

static uint32_t zz9k_soft_load32_le(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void zz9k_soft_store32_le(uint8_t *p, uint32_t value)
{
  p[0] = (uint8_t)(value & 0xffU);
  p[1] = (uint8_t)((value >> 8) & 0xffU);
  p[2] = (uint8_t)((value >> 16) & 0xffU);
  p[3] = (uint8_t)((value >> 24) & 0xffU);
}

static uint32_t zz9k_soft_rotl32(uint32_t value, unsigned int count)
{
  return (value << count) | (value >> (32U - count));
}

#define ZZ9K_SOFT_QUARTERROUND(a, b, c, d) \
  do {                                     \
    a += b; d ^= a; d = zz9k_soft_rotl32(d, 16U); \
    c += d; b ^= c; b = zz9k_soft_rotl32(b, 12U); \
    a += b; d ^= a; d = zz9k_soft_rotl32(d, 8U);  \
    c += d; b ^= c; b = zz9k_soft_rotl32(b, 7U);  \
  } while (0)

static void zz9k_soft_chacha20_block(
    uint8_t out[64],
    const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
    const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES],
    uint32_t counter)
{
  uint32_t state[16];
  uint32_t work[16];
  unsigned int i;

  state[0] = 0x61707865U;
  state[1] = 0x3320646eU;
  state[2] = 0x79622d32U;
  state[3] = 0x6b206574U;
  for (i = 0; i < 8U; i++) {
    state[4U + i] = zz9k_soft_load32_le(key + (i * 4U));
  }
  state[12] = counter;
  for (i = 0; i < 3U; i++) {
    state[13U + i] = zz9k_soft_load32_le(nonce + (i * 4U));
  }

  for (i = 0; i < 16U; i++) {
    work[i] = state[i];
  }

  for (i = 0; i < 10U; i++) {
    ZZ9K_SOFT_QUARTERROUND(work[0], work[4], work[8], work[12]);
    ZZ9K_SOFT_QUARTERROUND(work[1], work[5], work[9], work[13]);
    ZZ9K_SOFT_QUARTERROUND(work[2], work[6], work[10], work[14]);
    ZZ9K_SOFT_QUARTERROUND(work[3], work[7], work[11], work[15]);
    ZZ9K_SOFT_QUARTERROUND(work[0], work[5], work[10], work[15]);
    ZZ9K_SOFT_QUARTERROUND(work[1], work[6], work[11], work[12]);
    ZZ9K_SOFT_QUARTERROUND(work[2], work[7], work[8], work[13]);
    ZZ9K_SOFT_QUARTERROUND(work[3], work[4], work[9], work[14]);
  }

  for (i = 0; i < 16U; i++) {
    zz9k_soft_store32_le(out + (i * 4U), work[i] + state[i]);
  }
}

void zz9k_soft_chacha20_xor(uint8_t *out, const uint8_t *in, uint32_t length,
                            const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
                            const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES],
                            uint32_t counter)
{
  uint8_t block[64];
  uint32_t offset = 0;

  while (offset < length) {
    uint32_t chunk = length - offset;
    uint32_t i;

    if (chunk > 64U) {
      chunk = 64U;
    }
    zz9k_soft_chacha20_block(block, key, nonce, counter);
    for (i = 0; i < chunk; i++) {
      out[offset + i] = (uint8_t)(in[offset + i] ^ block[i]);
    }
    offset += chunk;
    counter++;
  }
}

typedef struct ZZ9KSoftPoly1305 {
  uint32_t r[5];
  uint32_t h[5];
  uint32_t pad[4];
} ZZ9KSoftPoly1305;

static void zz9k_soft_poly1305_init(ZZ9KSoftPoly1305 *st,
                                    const uint8_t key[32])
{
  uint32_t t0 = zz9k_soft_load32_le(key + 0);
  uint32_t t1 = zz9k_soft_load32_le(key + 4);
  uint32_t t2 = zz9k_soft_load32_le(key + 8);
  uint32_t t3 = zz9k_soft_load32_le(key + 12);

  st->r[0] = (t0) & 0x3ffffffU;
  st->r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03U;
  st->r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ffU;
  st->r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fffU;
  st->r[4] = (t3 >> 8) & 0x00fffffU;

  st->h[0] = 0;
  st->h[1] = 0;
  st->h[2] = 0;
  st->h[3] = 0;
  st->h[4] = 0;

  st->pad[0] = zz9k_soft_load32_le(key + 16);
  st->pad[1] = zz9k_soft_load32_le(key + 20);
  st->pad[2] = zz9k_soft_load32_le(key + 24);
  st->pad[3] = zz9k_soft_load32_le(key + 28);
}

/* Processes one 16-byte block. `hibit` adds the 2^128 bit for full blocks. */
static void zz9k_soft_poly1305_block(ZZ9KSoftPoly1305 *st,
                                     const uint8_t block[16],
                                     uint32_t hibit)
{
  uint32_t r0 = st->r[0];
  uint32_t r1 = st->r[1];
  uint32_t r2 = st->r[2];
  uint32_t r3 = st->r[3];
  uint32_t r4 = st->r[4];
  uint32_t s1 = r1 * 5U;
  uint32_t s2 = r2 * 5U;
  uint32_t s3 = r3 * 5U;
  uint32_t s4 = r4 * 5U;
  uint32_t h0 = st->h[0];
  uint32_t h1 = st->h[1];
  uint32_t h2 = st->h[2];
  uint32_t h3 = st->h[3];
  uint32_t h4 = st->h[4];
  uint32_t t0 = zz9k_soft_load32_le(block + 0);
  uint32_t t1 = zz9k_soft_load32_le(block + 4);
  uint32_t t2 = zz9k_soft_load32_le(block + 8);
  uint32_t t3 = zz9k_soft_load32_le(block + 12);
  uint64_t d0;
  uint64_t d1;
  uint64_t d2;
  uint64_t d3;
  uint64_t d4;
  uint32_t c;

  h0 += (t0) & 0x3ffffffU;
  h1 += ((t0 >> 26) | (t1 << 6)) & 0x3ffffffU;
  h2 += ((t1 >> 20) | (t2 << 12)) & 0x3ffffffU;
  h3 += ((t2 >> 14) | (t3 << 18)) & 0x3ffffffU;
  h4 += (t3 >> 8) | hibit;

  d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 +
       (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
  d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 +
       (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
  d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 +
       (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
  d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 +
       (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
  d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 +
       (uint64_t)h3 * r1 + (uint64_t)h4 * r0;

  c = (uint32_t)(d0 >> 26);
  h0 = (uint32_t)d0 & 0x3ffffffU;
  d1 += c;
  c = (uint32_t)(d1 >> 26);
  h1 = (uint32_t)d1 & 0x3ffffffU;
  d2 += c;
  c = (uint32_t)(d2 >> 26);
  h2 = (uint32_t)d2 & 0x3ffffffU;
  d3 += c;
  c = (uint32_t)(d3 >> 26);
  h3 = (uint32_t)d3 & 0x3ffffffU;
  d4 += c;
  c = (uint32_t)(d4 >> 26);
  h4 = (uint32_t)d4 & 0x3ffffffU;
  h0 += c * 5U;
  c = h0 >> 26;
  h0 = h0 & 0x3ffffffU;
  h1 += c;

  st->h[0] = h0;
  st->h[1] = h1;
  st->h[2] = h2;
  st->h[3] = h3;
  st->h[4] = h4;
}

static void zz9k_soft_poly1305_finish(ZZ9KSoftPoly1305 *st, uint8_t tag[16])
{
  uint32_t h0 = st->h[0];
  uint32_t h1 = st->h[1];
  uint32_t h2 = st->h[2];
  uint32_t h3 = st->h[3];
  uint32_t h4 = st->h[4];
  uint32_t g0;
  uint32_t g1;
  uint32_t g2;
  uint32_t g3;
  uint32_t g4;
  uint32_t c;
  uint32_t mask;
  uint64_t f;

  c = h1 >> 26;
  h1 &= 0x3ffffffU;
  h2 += c;
  c = h2 >> 26;
  h2 &= 0x3ffffffU;
  h3 += c;
  c = h3 >> 26;
  h3 &= 0x3ffffffU;
  h4 += c;
  c = h4 >> 26;
  h4 &= 0x3ffffffU;
  h0 += c * 5U;
  c = h0 >> 26;
  h0 &= 0x3ffffffU;
  h1 += c;

  g0 = h0 + 5U;
  c = g0 >> 26;
  g0 &= 0x3ffffffU;
  g1 = h1 + c;
  c = g1 >> 26;
  g1 &= 0x3ffffffU;
  g2 = h2 + c;
  c = g2 >> 26;
  g2 &= 0x3ffffffU;
  g3 = h3 + c;
  c = g3 >> 26;
  g3 &= 0x3ffffffU;
  g4 = h4 + c - (1U << 26);

  mask = (g4 >> 31) - 1U;
  g0 &= mask;
  g1 &= mask;
  g2 &= mask;
  g3 &= mask;
  g4 &= mask;
  mask = ~mask;
  h0 = (h0 & mask) | g0;
  h1 = (h1 & mask) | g1;
  h2 = (h2 & mask) | g2;
  h3 = (h3 & mask) | g3;
  h4 = (h4 & mask) | g4;

  h0 = ((h0) | (h1 << 26)) & 0xffffffffU;
  h1 = ((h1 >> 6) | (h2 << 20)) & 0xffffffffU;
  h2 = ((h2 >> 12) | (h3 << 14)) & 0xffffffffU;
  h3 = ((h3 >> 18) | (h4 << 8)) & 0xffffffffU;

  f = (uint64_t)h0 + st->pad[0];
  h0 = (uint32_t)f;
  f = (uint64_t)h1 + st->pad[1] + (f >> 32);
  h1 = (uint32_t)f;
  f = (uint64_t)h2 + st->pad[2] + (f >> 32);
  h2 = (uint32_t)f;
  f = (uint64_t)h3 + st->pad[3] + (f >> 32);
  h3 = (uint32_t)f;

  zz9k_soft_store32_le(tag + 0, h0);
  zz9k_soft_store32_le(tag + 4, h1);
  zz9k_soft_store32_le(tag + 8, h2);
  zz9k_soft_store32_le(tag + 12, h3);
}

static void zz9k_soft_poly1305_update(ZZ9KSoftPoly1305 *st,
                                      const uint8_t *message, uint32_t length)
{
  while (length >= 16U) {
    zz9k_soft_poly1305_block(st, message, 1U << 24);
    message += 16U;
    length -= 16U;
  }
  if (length > 0U) {
    uint8_t block[16];

    memset(block, 0, sizeof(block));
    memcpy(block, message, length);
    block[length] = 1U;
    zz9k_soft_poly1305_block(st, block, 0U);
  }
}

void zz9k_soft_poly1305(uint8_t tag[ZZ9K_SOFT_POLY1305_TAG_BYTES],
                        const uint8_t *message, uint32_t length,
                        const uint8_t key[ZZ9K_SOFT_POLY1305_KEY_BYTES])
{
  ZZ9KSoftPoly1305 st;

  zz9k_soft_poly1305_init(&st, key);
  zz9k_soft_poly1305_update(&st, message, length);
  zz9k_soft_poly1305_finish(&st, tag);
}

/*
 * Feeds one AEAD segment (aad or ciphertext) to Poly1305 as whole 16-byte
 * blocks, zero-padding the final partial block. Unlike the standalone
 * Poly1305 message convention, every AEAD block is a full block and takes
 * the 2^128 high bit; the zero pad supplies the pad16() of RFC 8439.
 */
static void zz9k_soft_poly1305_aead_segment(ZZ9KSoftPoly1305 *st,
                                            const uint8_t *data,
                                            uint32_t length)
{
  while (length >= 16U) {
    zz9k_soft_poly1305_block(st, data, 1U << 24);
    data += 16U;
    length -= 16U;
  }
  if (length > 0U) {
    uint8_t block[16];

    memset(block, 0, sizeof(block));
    memcpy(block, data, length);
    zz9k_soft_poly1305_block(st, block, 1U << 24);
  }
}

static void zz9k_soft_chacha20_poly1305_tag(
    uint8_t tag[16],
    const uint8_t *ciphertext, uint32_t length,
    const uint8_t *aad, uint32_t aad_length,
    const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
    const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES])
{
  ZZ9KSoftPoly1305 st;
  uint8_t otk[64];
  uint8_t lengths[16];
  static const uint8_t zero_block[64] = { 0 };

  zz9k_soft_chacha20_xor(otk, zero_block, 32U, key, nonce, 0U);
  zz9k_soft_poly1305_init(&st, otk);

  zz9k_soft_poly1305_aead_segment(&st, aad, aad_length);
  zz9k_soft_poly1305_aead_segment(&st, ciphertext, length);

  zz9k_soft_store32_le(lengths + 0, aad_length);
  zz9k_soft_store32_le(lengths + 4, 0U);
  zz9k_soft_store32_le(lengths + 8, length);
  zz9k_soft_store32_le(lengths + 12, 0U);
  zz9k_soft_poly1305_block(&st, lengths, 1U << 24);

  zz9k_soft_poly1305_finish(&st, tag);
}

void zz9k_soft_chacha20_poly1305_encrypt(
    uint8_t *ciphertext, uint8_t tag[ZZ9K_SOFT_POLY1305_TAG_BYTES],
    const uint8_t *plaintext, uint32_t length,
    const uint8_t *aad, uint32_t aad_length,
    const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
    const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES])
{
  zz9k_soft_chacha20_xor(ciphertext, plaintext, length, key, nonce, 1U);
  zz9k_soft_chacha20_poly1305_tag(tag, ciphertext, length, aad, aad_length,
                                  key, nonce);
}

int zz9k_soft_chacha20_poly1305_decrypt(
    uint8_t *plaintext,
    const uint8_t *ciphertext, uint32_t length,
    const uint8_t *aad, uint32_t aad_length,
    const uint8_t tag[ZZ9K_SOFT_POLY1305_TAG_BYTES],
    const uint8_t key[ZZ9K_SOFT_CHACHA20_KEY_BYTES],
    const uint8_t nonce[ZZ9K_SOFT_CHACHA20_NONCE_BYTES])
{
  uint8_t expected[16];
  uint32_t diff = 0;
  uint32_t i;

  zz9k_soft_chacha20_poly1305_tag(expected, ciphertext, length, aad,
                                  aad_length, key, nonce);
  for (i = 0; i < 16U; i++) {
    diff |= (uint32_t)(expected[i] ^ tag[i]);
  }
  if (diff != 0U) {
    return 0;
  }

  zz9k_soft_chacha20_xor(plaintext, ciphertext, length, key, nonce, 1U);
  return 1;
}
