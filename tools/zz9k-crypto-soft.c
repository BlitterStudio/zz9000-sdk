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

/* ---- X25519 (Curve25519 Diffie-Hellman, RFC 7748) ---- */
/* Portable C99 implementation. Based on the public-domain work of
 * Daniel J. Bernstein (SUPERCOP ref10). All arithmetic is mod 2^255-19.
 * Limb layout: alternating 26-bit (even) and 25-bit (odd) limbs.
 *   h[0] = bits   0-25, h[1] = bits  26-50, h[2] = bits  51-76,
 *   h[3] = bits  77-101, h[4] = bits 102-127, h[5] = bits 128-152,
 *   h[6] = bits 153-178, h[7] = bits 179-203, h[8] = bits 204-229,
 *   h[9] = bits 230-254. */

typedef uint32_t fe25519[10];

static void fe25519_0(fe25519 h) {
  int i; for (i = 0; i < 10; i++) h[i] = 0;
}
static void fe25519_1(fe25519 h) {
  fe25519_0(h); h[0] = 1;
}
static void fe25519_copy(fe25519 h, const fe25519 f) {
  int i; for (i = 0; i < 10; i++) h[i] = f[i];
}
static void fe25519_cswap(fe25519 f, fe25519 g, uint32_t b) {
  int i;
  uint32_t mask = (uint32_t)(-(int32_t)b);
  for (i = 0; i < 10; i++) {
    uint32_t t = mask & (f[i] ^ g[i]);
    f[i] ^= t; g[i] ^= t;
  }
}

/*
 * Deserialise a 32-byte little-endian encoding into ten limbs.
 * Limb sizes (bits): h0:26 h1:25 h2:26 h3:25 h4:26 h5:25 h6:26 h7:25 h8:26 h9:25.
 * Bit boundaries (field bit .. field bit):
 *   h0: 0-25, h1: 26-50, h2: 51-76, h3: 77-101, h4: 102-127,
 *   h5: 128-152, h6: 153-178, h7: 179-203, h8: 204-229, h9: 230-254.
 */
static void fe25519_from_bytes(fe25519 h, const uint8_t *s) {
  int64_t b0 =(int64_t)s[ 0],b1 =(int64_t)s[ 1],b2 =(int64_t)s[ 2],b3 =(int64_t)s[ 3];
  int64_t b4 =(int64_t)s[ 4],b5 =(int64_t)s[ 5],b6 =(int64_t)s[ 6];
  int64_t b7 =(int64_t)s[ 7],b8 =(int64_t)s[ 8],b9 =(int64_t)s[ 9];
  int64_t b10=(int64_t)s[10],b11=(int64_t)s[11],b12=(int64_t)s[12];
  int64_t b13=(int64_t)s[13],b14=(int64_t)s[14],b15=(int64_t)s[15];
  int64_t b16=(int64_t)s[16],b17=(int64_t)s[17],b18=(int64_t)s[18],b19=(int64_t)s[19];
  int64_t b20=(int64_t)s[20],b21=(int64_t)s[21],b22=(int64_t)s[22];
  int64_t b23=(int64_t)s[23],b24=(int64_t)s[24],b25=(int64_t)s[25];
  int64_t b26=(int64_t)s[26],b27=(int64_t)s[27],b28=(int64_t)s[28];
  int64_t b29=(int64_t)s[29],b30=(int64_t)s[30],b31=(int64_t)s[31];
  /* h0: field bits 0-25.  Byte 3 contributes bits 24-25 (mask 0x03). */
  h[0]=(uint32_t)(b0|(b1<<8)|(b2<<16)|((b3&0x03)<<24));
  /* h1: field bits 26-50.  Byte 3 bits 2-7, bytes 4-5, byte 6 bits 0-2. */
  h[1]=(uint32_t)((b3>>2)|(b4<<6)|(b5<<14)|((b6&0x07)<<22));
  /* h2: field bits 51-76.  Byte 6 bits 3-7, bytes 7-8, byte 9 bits 0-4. */
  h[2]=(uint32_t)((b6>>3)|(b7<<5)|(b8<<13)|((b9&0x1f)<<21));
  /* h3: field bits 77-101. Byte 9 bits 5-7, bytes 10-11, byte 12 bits 0-5. */
  h[3]=(uint32_t)((b9>>5)|(b10<<3)|(b11<<11)|((b12&0x3f)<<19));
  /* h4: field bits 102-127. Byte 12 bits 6-7, bytes 13-15 (full). */
  h[4]=(uint32_t)((b12>>6)|(b13<<2)|(b14<<10)|(b15<<18));
  /* h5: field bits 128-152. Bytes 16-18, byte 19 bit 0 only. */
  h[5]=(uint32_t)(b16|(b17<<8)|(b18<<16)|((b19&0x01)<<24));
  /* h6: field bits 153-178. Byte 19 bits 1-7, bytes 20-21, byte 22 bits 0-2. */
  h[6]=(uint32_t)((b19>>1)|(b20<<7)|(b21<<15)|((b22&0x07)<<23));
  /* h7: field bits 179-203. Byte 22 bits 3-7, bytes 23-24, byte 25 bits 0-3. */
  h[7]=(uint32_t)((b22>>3)|(b23<<5)|(b24<<13)|((b25&0x0f)<<21));
  /* h8: field bits 204-229. Byte 25 bits 4-7, bytes 26-27, byte 28 bits 0-5. */
  h[8]=(uint32_t)((b25>>4)|(b26<<4)|(b27<<12)|((b28&0x3f)<<20));
  /* h9: field bits 230-254. Byte 28 bits 6-7, bytes 29-30, byte 31 bits 0-6. */
  h[9]=(uint32_t)((b28>>6)|(b29<<2)|(b30<<10)|((b31&0x7f)<<18));
}

static void fe25519_to_bytes(uint8_t *s, fe25519 h) {
  int64_t h0=(int64_t)h[0], h1=(int64_t)h[1], h2=(int64_t)h[2], h3=(int64_t)h[3], h4=(int64_t)h[4];
  int64_t h5=(int64_t)h[5], h6=(int64_t)h[6], h7=(int64_t)h[7], h8=(int64_t)h[8], h9=(int64_t)h[9];
  int64_t q, carry;
  /* Reduce: compute q = floor((h + 19) / 2^255) */
  q = (19*h9 + (((int64_t)1)<<24)) >> 25;
  q = (h0+q) >> 26; q = (h1+q) >> 25; q = (h2+q) >> 26; q = (h3+q) >> 25;
  q = (h4+q) >> 26; q = (h5+q) >> 25; q = (h6+q) >> 26; q = (h7+q) >> 25;
  q = (h8+q) >> 26; q = (h9+q) >> 25;
  h0 += 19*q;
  carry=h0>>26; h1+=carry; h0&=0x3ffffff;
  carry=h1>>25; h2+=carry; h1&=0x1ffffff;
  carry=h2>>26; h3+=carry; h2&=0x3ffffff;
  carry=h3>>25; h4+=carry; h3&=0x1ffffff;
  carry=h4>>26; h5+=carry; h4&=0x3ffffff;
  carry=h5>>25; h6+=carry; h5&=0x1ffffff;
  carry=h6>>26; h7+=carry; h6&=0x3ffffff;
  carry=h7>>25; h8+=carry; h7&=0x1ffffff;
  carry=h8>>26; h9+=carry; h8&=0x3ffffff;
  h9&=0x1ffffff;
  /* Pack 10 limbs into 32 bytes. */
  /* h0:26 h1:25 h2:26 h3:25 h4:26 h5:25 h6:26 h7:25 h8:26 h9:25 */
  s[ 0]=(uint8_t)(h0);
  s[ 1]=(uint8_t)(h0>> 8);
  s[ 2]=(uint8_t)(h0>>16);
  s[ 3]=(uint8_t)((h0>>24)|(h1<<2));   /* h0 bits 24-25, h1 bits 0-5 */
  s[ 4]=(uint8_t)(h1>> 6);
  s[ 5]=(uint8_t)(h1>>14);
  s[ 6]=(uint8_t)((h1>>22)|(h2<<3));   /* h1 bits 22-24, h2 bits 0-4 */
  s[ 7]=(uint8_t)(h2>> 5);
  s[ 8]=(uint8_t)(h2>>13);
  s[ 9]=(uint8_t)((h2>>21)|(h3<<5));   /* h2 bits 21-25, h3 bits 0-2 */
  s[10]=(uint8_t)(h3>> 3);
  s[11]=(uint8_t)(h3>>11);
  s[12]=(uint8_t)((h3>>19)|(h4<<6));   /* h3 bits 19-24, h4 bits 0-1 */
  s[13]=(uint8_t)(h4>> 2);
  s[14]=(uint8_t)(h4>>10);
  s[15]=(uint8_t)(h4>>18);             /* h4 bits 18-25 (8 bits, full byte) */
  s[16]=(uint8_t)(h5);
  s[17]=(uint8_t)(h5>> 8);
  s[18]=(uint8_t)(h5>>16);
  s[19]=(uint8_t)((h5>>24)|(h6<<1));   /* h5 bit 24, h6 bits 0-6 */
  s[20]=(uint8_t)(h6>> 7);
  s[21]=(uint8_t)(h6>>15);
  s[22]=(uint8_t)((h6>>23)|(h7<<3));   /* h6 bits 23-25, h7 bits 0-4 */
  s[23]=(uint8_t)(h7>> 5);
  s[24]=(uint8_t)(h7>>13);
  s[25]=(uint8_t)((h7>>21)|(h8<<4));   /* h7 bits 21-24, h8 bits 0-3 */
  s[26]=(uint8_t)(h8>> 4);
  s[27]=(uint8_t)(h8>>12);
  s[28]=(uint8_t)((h8>>20)|(h9<<6));   /* h8 bits 20-25, h9 bits 0-1 */
  s[29]=(uint8_t)(h9>> 2);
  s[30]=(uint8_t)(h9>>10);
  s[31]=(uint8_t)(h9>>18);
}

static void fe25519_add(fe25519 h, const fe25519 f, const fe25519 g) {
  int i; for (i=0;i<10;i++) h[i]=f[i]+g[i];
}
static void fe25519_sub(fe25519 h, const fe25519 f, const fe25519 g) {
  /* Compute h = f - g + 2p to keep all limbs non-negative.
   * 2p = 2*(2^255-19). Per-limb values of 2p (all limbs fully set minus
   * the low bits of 19): */
  static const uint32_t TWO_P[10] = {
    0x7ffffda, 0x3fffffe, 0x7fffffe, 0x3fffffe, 0x7fffffe,
    0x3fffffe, 0x7fffffe, 0x3fffffe, 0x7fffffe, 0x3fffffe
  };
  /* Use int64_t to avoid uint32_t wrapping when f[i] < g[i]. */
  int i;
  for (i=0;i<10;i++) {
    int64_t diff = (int64_t)f[i] - (int64_t)g[i] + (int64_t)TWO_P[i];
    h[i] = (uint32_t)diff;
  }
}
/*
 * Field multiplication h = f * g mod 2^255-19.
 *
 * Limb weights: w[k] = {0,26,51,77,102,128,153,179,204,230}.
 * For any pair (i,j) with i+j=k, the product f[i]*g[j] has weight
 * w[i]+w[j].  When both i and j are odd, w[i]+w[j] = w[i+j]+1, so the
 * limb product must be scaled by 2.  This is absorbed by pre-doubling
 * odd-indexed f limbs: f1_2=2*f1, f3_2=2*f3, etc.  The same 2x factor
 * applies to wrap-around ODD*ODD terms (i+j = k+10).
 * This is the standard SUPERCOP ref10 formulation, valid for f!=g as well.
 */
static void fe25519_mul(fe25519 h, const fe25519 f, const fe25519 g) {
  int64_t f0=(int64_t)f[0],f1=(int64_t)f[1],f2=(int64_t)f[2],f3=(int64_t)f[3],f4=(int64_t)f[4];
  int64_t f5=(int64_t)f[5],f6=(int64_t)f[6],f7=(int64_t)f[7],f8=(int64_t)f[8],f9=(int64_t)f[9];
  int64_t g0=(int64_t)g[0],g1=(int64_t)g[1],g2=(int64_t)g[2],g3=(int64_t)g[3],g4=(int64_t)g[4];
  int64_t g5=(int64_t)g[5],g6=(int64_t)g[6],g7=(int64_t)g[7],g8=(int64_t)g[8],g9=(int64_t)g[9];
  /* Pre-doubled odd-indexed f limbs for ODD*ODD correction factor. */
  int64_t f1_2=2*f1, f3_2=2*f3, f5_2=2*f5, f7_2=2*f7, f9_2=2*f9;
  /* Precomputed 19x versions of g for wrap-around reduction. */
  int64_t g1_19=19*g1,g2_19=19*g2,g3_19=19*g3,g4_19=19*g4,g5_19=19*g5;
  int64_t g6_19=19*g6,g7_19=19*g7,g8_19=19*g8,g9_19=19*g9;
  int64_t r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,carry;
  /* For each output limb r[k]:
   *   non-wrap terms (i+j=k, i+j<10): use f[i]*g[j], with f1_2 for ODD i
   *   wrap terms (i+j=k+10):          use 19*f[i]*g[j], with f1_2 for ODD i
   * Pairs marked (o) have both indices odd => use f_odd_2 for the f factor. */
  r0=f0*g0     +f1_2*g9_19 +f2*g8_19  +f3_2*g7_19 +f4*g6_19  +f5_2*g5_19 +f6*g4_19  +f7_2*g3_19 +f8*g2_19  +f9_2*g1_19;
  r1=f0*g1     +f1*g0      +f2*g9_19  +f3*g8_19   +f4*g7_19  +f5*g6_19   +f6*g5_19  +f7*g4_19   +f8*g3_19  +f9*g2_19;
  r2=f0*g2     +f1_2*g1    +f2*g0     +f3_2*g9_19 +f4*g8_19  +f5_2*g7_19 +f6*g6_19  +f7_2*g5_19 +f8*g4_19  +f9_2*g3_19;
  r3=f0*g3     +f1*g2      +f2*g1     +f3*g0      +f4*g9_19  +f5*g8_19   +f6*g7_19  +f7*g6_19   +f8*g5_19  +f9*g4_19;
  r4=f0*g4     +f1_2*g3    +f2*g2     +f3_2*g1    +f4*g0     +f5_2*g9_19 +f6*g8_19  +f7_2*g7_19 +f8*g6_19  +f9_2*g5_19;
  r5=f0*g5     +f1*g4      +f2*g3     +f3*g2      +f4*g1     +f5*g0      +f6*g9_19  +f7*g8_19   +f8*g7_19  +f9*g6_19;
  r6=f0*g6     +f1_2*g5    +f2*g4     +f3_2*g3    +f4*g2     +f5_2*g1    +f6*g0     +f7_2*g9_19 +f8*g8_19  +f9_2*g7_19;
  r7=f0*g7     +f1*g6      +f2*g5     +f3*g4      +f4*g3     +f5*g2      +f6*g1     +f7*g0      +f8*g9_19  +f9*g8_19;
  r8=f0*g8     +f1_2*g7    +f2*g6     +f3_2*g5    +f4*g4     +f5_2*g3    +f6*g2     +f7_2*g1    +f8*g0     +f9_2*g9_19;
  r9=f0*g9     +f1*g8      +f2*g7     +f3*g6      +f4*g5     +f5*g4      +f6*g3     +f7*g2      +f8*g1     +f9*g0;
  carry=r0>>26; r1+=carry; r0&=0x3ffffff;
  carry=r1>>25; r2+=carry; r1&=0x1ffffff;
  carry=r2>>26; r3+=carry; r2&=0x3ffffff;
  carry=r3>>25; r4+=carry; r3&=0x1ffffff;
  carry=r4>>26; r5+=carry; r4&=0x3ffffff;
  carry=r5>>25; r6+=carry; r5&=0x1ffffff;
  carry=r6>>26; r7+=carry; r6&=0x3ffffff;
  carry=r7>>25; r8+=carry; r7&=0x1ffffff;
  carry=r8>>26; r9+=carry; r8&=0x3ffffff;
  carry=r9>>25; r0+=19*carry; r9&=0x1ffffff;
  carry=r0>>26; r1+=carry; r0&=0x3ffffff;
  h[0]=(uint32_t)r0;h[1]=(uint32_t)r1;h[2]=(uint32_t)r2;h[3]=(uint32_t)r3;h[4]=(uint32_t)r4;
  h[5]=(uint32_t)r5;h[6]=(uint32_t)r6;h[7]=(uint32_t)r7;h[8]=(uint32_t)r8;h[9]=(uint32_t)r9;
}
static void fe25519_sq(fe25519 h, const fe25519 f) { fe25519_mul(h, f, f); }
/* Multiply by a24 = (486662-2)/4 = 121665, the Montgomery ladder constant.
 * (Not ref10's fe_mul121666 -- that belongs to a differently arranged
 * ladder step; this AA + a24*E formulation uses 121665.) */
static void fe25519_mul_a24(fe25519 h, const fe25519 f) {
  int i; int64_t carry;
  int64_t t[10]; for(i=0;i<10;i++) t[i]=(int64_t)f[i]*121665;
  carry=t[9]>>25; t[0]+=19*carry; t[9]&=0x1ffffff;
  carry=t[0]>>26; t[1]+=carry; t[0]&=0x3ffffff;
  carry=t[1]>>25; t[2]+=carry; t[1]&=0x1ffffff;
  carry=t[2]>>26; t[3]+=carry; t[2]&=0x3ffffff;
  carry=t[3]>>25; t[4]+=carry; t[3]&=0x1ffffff;
  carry=t[4]>>26; t[5]+=carry; t[4]&=0x3ffffff;
  carry=t[5]>>25; t[6]+=carry; t[5]&=0x1ffffff;
  carry=t[6]>>26; t[7]+=carry; t[6]&=0x3ffffff;
  carry=t[7]>>25; t[8]+=carry; t[7]&=0x1ffffff;
  carry=t[8]>>26; t[9]+=carry; t[8]&=0x3ffffff;
  for(i=0;i<10;i++) h[i]=(uint32_t)t[i];
}
static void fe25519_invert(fe25519 out, const fe25519 z) {
  fe25519 t0,t1,t2,t3; int i;
  fe25519_sq(t0,z);
  fe25519_sq(t1,t0); fe25519_sq(t1,t1);
  fe25519_mul(t1,z,t1);
  fe25519_mul(t0,t0,t1);
  fe25519_sq(t2,t0); fe25519_mul(t1,t1,t2);
  fe25519_sq(t2,t1); for(i=1;i<5;i++) fe25519_sq(t2,t2);
  fe25519_mul(t1,t2,t1);
  fe25519_sq(t2,t1); for(i=1;i<10;i++) fe25519_sq(t2,t2);
  fe25519_mul(t2,t2,t1);
  fe25519_sq(t3,t2); for(i=1;i<20;i++) fe25519_sq(t3,t3);
  fe25519_mul(t2,t3,t2);
  fe25519_sq(t2,t2); for(i=1;i<10;i++) fe25519_sq(t2,t2);
  fe25519_mul(t1,t2,t1);
  fe25519_sq(t2,t1); for(i=1;i<50;i++) fe25519_sq(t2,t2);
  fe25519_mul(t2,t2,t1);
  fe25519_sq(t3,t2); for(i=1;i<100;i++) fe25519_sq(t3,t3);
  fe25519_mul(t2,t3,t2);
  fe25519_sq(t2,t2); for(i=1;i<50;i++) fe25519_sq(t2,t2);
  fe25519_mul(t1,t2,t1);
  fe25519_sq(t1,t1); for(i=1;i<5;i++) fe25519_sq(t1,t1);
  fe25519_mul(out,t1,t0);
}

static void x25519_ladder(const uint8_t *k, const uint8_t *u, uint8_t *out)
{
  fe25519 x1,x2,x3,z2,z3,A,AA,B,BB,E,C,D,DA,CB,tmp;
  uint8_t e[32]; int i; uint32_t swap=0, b;
  memcpy(e,k,32);
  e[0] &= 248; e[31] &= 127; e[31] |= 64;
  fe25519_from_bytes(x1,u);
  fe25519_1(x2); fe25519_0(z2);
  fe25519_copy(x3,x1); fe25519_1(z3);
  for (i=254; i>=0; i--) {
    b=(e[i/8]>>(i&7))&1;
    swap^=b; fe25519_cswap(x2,x3,swap); fe25519_cswap(z2,z3,swap); swap=b;
    fe25519_add(A,x2,z2); fe25519_sq(AA,A);
    fe25519_sub(B,x2,z2); fe25519_sq(BB,B);
    fe25519_sub(E,AA,BB);
    fe25519_add(C,x3,z3); fe25519_sub(D,x3,z3);
    fe25519_mul(DA,D,A); fe25519_mul(CB,C,B);
    fe25519_add(tmp,DA,CB); fe25519_sq(x3,tmp);
    fe25519_sub(tmp,DA,CB); fe25519_sq(z3,tmp); fe25519_mul(z3,z3,x1);
    fe25519_mul(x2,AA,BB);
    fe25519_mul_a24(tmp,E); fe25519_add(tmp,AA,tmp);
    fe25519_mul(z2,E,tmp);
  }
  fe25519_cswap(x2,x3,swap); fe25519_cswap(z2,z3,swap);
  fe25519_invert(z2,z2);
  fe25519_mul(x2,x2,z2);
  fe25519_to_bytes(out,x2);
}

int zz9k_soft_x25519(uint8_t out[32], const uint8_t scalar[32],
                     const uint8_t point[32])
{
  uint32_t zero=0; int i;
  x25519_ladder(scalar, point, out);
  for (i=0;i<32;i++) zero|=out[i];
  return zero!=0; /* 0 = all-zero output (small-order point), reject */
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
