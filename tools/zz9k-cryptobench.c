/*
 * ChaCha20-Poly1305 offload break-even benchmark and X25519 timing section.
 *
 * Sweeps TLS-sized records and compares the portable software reference
 * (the m68k baseline) against the ZZ9000 offload AEAD, reporting per-size
 * throughput and the break-even record size at which offload overtakes
 * software. This answers the central Phase 0 question: is symmetric offload
 * worth the mailbox round-trip at realistic record sizes?
 *
 * The X25519 section times software vs. offload for a single scalar multiply,
 * reporting milliseconds per operation and the speedup ratio.
 *
 * The software sweeps run on any target. The offload paths need the crypto
 * service and are skipped when the capability is absent (e.g. on the host).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k/caps.h"
#include "zz9k/crypto.h"
#include "zz9k/host.h"
#include "zz9k/shared.h"
#include "zz9k-crypto-soft.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__amigaos__) || defined(__amiga__) || defined(__AMIGA__) || \
    defined(__VBCC__)
#define ZZ9K_CRYPTOBENCH_AMIGA 1
#include <devices/timer.h>
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/timer.h>
#else
#include <time.h>
#endif

#define ZZ9K_CRYPTOBENCH_SWEEP_COUNT 9U
#define ZZ9K_CRYPTOBENCH_MAX_RECORD_BYTES 16384U
#define ZZ9K_CRYPTOBENCH_DEFAULT_ITERATIONS 16U
#define ZZ9K_CRYPTOBENCH_MAX_ITERATIONS 4096U
#define ZZ9K_CRYPTOBENCH_TAG_BYTES 16U
#define ZZ9K_CRYPTOBENCH_KEY_BYTES 32U
#define ZZ9K_CRYPTOBENCH_NONCE_BYTES 12U

typedef uint64_t ZZ9KCryptoBenchTick;

/* Record size for sweep index 0..ZZ9K_CRYPTOBENCH_SWEEP_COUNT-1. */
static uint32_t zz9k_cryptobench_sweep_size(uint32_t index)
{
  if (index >= ZZ9K_CRYPTOBENCH_SWEEP_COUNT) {
    return 0U;
  }
  return 64U << index; /* 64, 128, 256 ... 16384 */
}

static uint32_t zz9k_cryptobench_parse_iterations(int argc, char **argv)
{
  unsigned long value;

  if (argc < 2) {
    return ZZ9K_CRYPTOBENCH_DEFAULT_ITERATIONS;
  }
  value = strtoul(argv[1], 0, 0);
  if (value < 1UL) {
    return 1U;
  }
  if (value > ZZ9K_CRYPTOBENCH_MAX_ITERATIONS) {
    return ZZ9K_CRYPTOBENCH_MAX_ITERATIONS;
  }
  return (uint32_t)value;
}

static uint32_t zz9k_cryptobench_kib_per_second(uint32_t bytes,
                                                ZZ9KCryptoBenchTick ticks,
                                                uint32_t ticks_per_second)
{
  uint64_t rate;

  if (ticks == 0 || ticks_per_second == 0) {
    return 0;
  }
  rate = ((uint64_t)bytes * ticks_per_second) / ((uint64_t)ticks * 1024ULL);
  if (rate > UINT32_MAX) {
    return UINT32_MAX;
  }
  return (uint32_t)rate;
}

/*
 * Smallest record size at which offload throughput meets or beats software,
 * or 0 if offload never wins across the sampled sizes. Arrays are parallel
 * and `count` entries long, ordered by ascending size.
 */
static uint32_t zz9k_cryptobench_breakeven_size(const uint32_t *sizes,
                                                const uint32_t *software_kib,
                                                const uint32_t *offload_kib,
                                                uint32_t count)
{
  uint32_t i;

  if (!sizes || !software_kib || !offload_kib) {
    return 0U;
  }
  for (i = 0; i < count; i++) {
    if (offload_kib[i] != 0U && offload_kib[i] >= software_kib[i]) {
      return sizes[i];
    }
  }
  return 0U;
}

#if ZZ9K_CRYPTOBENCH_AMIGA
struct Device *TimerBase;
#endif

typedef struct ZZ9KCryptoBenchTimer {
  uint32_t ticks_per_second;
  int high_resolution;
#if ZZ9K_CRYPTOBENCH_AMIGA
  struct MsgPort *timer_port;
  struct timerequest *timer_request;
#endif
} ZZ9KCryptoBenchTimer;

static void zz9k_cryptobench_timer_close(ZZ9KCryptoBenchTimer *timer)
{
#if ZZ9K_CRYPTOBENCH_AMIGA
  if (!timer) {
    return;
  }
  if (timer->timer_request) {
    if (timer->high_resolution) {
      CloseDevice((struct IORequest *)timer->timer_request);
    }
    DeleteIORequest(timer->timer_request);
  }
  if (timer->timer_port) {
    DeleteMsgPort(timer->timer_port);
  }
  if (TimerBase && timer->high_resolution) {
    TimerBase = 0;
  }
#else
  (void)timer;
#endif
}

static void zz9k_cryptobench_timer_open(ZZ9KCryptoBenchTimer *timer)
{
  memset(timer, 0, sizeof(*timer));
#if ZZ9K_CRYPTOBENCH_AMIGA
  timer->timer_port = CreateMsgPort();
  if (timer->timer_port) {
    timer->timer_request = (struct timerequest *)CreateIORequest(
        timer->timer_port, sizeof(*timer->timer_request));
  }
  if (timer->timer_request &&
      OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                 (struct IORequest *)timer->timer_request, 0) == 0) {
    struct EClockVal value;

    TimerBase = (struct Device *)timer->timer_request->tr_node.io_Device;
    timer->ticks_per_second = ReadEClock(&value);
    if (timer->ticks_per_second != 0) {
      timer->high_resolution = 1;
      return;
    }
    CloseDevice((struct IORequest *)timer->timer_request);
  }
  zz9k_cryptobench_timer_close(timer);
  memset(timer, 0, sizeof(*timer));
  timer->ticks_per_second = 50U;
#else
  timer->ticks_per_second = (uint32_t)CLOCKS_PER_SEC;
  timer->high_resolution = 1;
#endif
}

static ZZ9KCryptoBenchTick zz9k_cryptobench_timer_now(
    const ZZ9KCryptoBenchTimer *timer)
{
#if ZZ9K_CRYPTOBENCH_AMIGA
  if (timer && timer->high_resolution) {
    struct EClockVal value;

    ReadEClock(&value);
    return ((ZZ9KCryptoBenchTick)value.ev_hi << 32) | value.ev_lo;
  }
  return 0;
#else
  (void)timer;
  return (ZZ9KCryptoBenchTick)clock();
#endif
}

/*
 * Returns elapsed time in milliseconds * 100 (fixed-point hundredths) for
 * `count` X25519 operations, or 0 on timer error. Used for both software and
 * offload paths so the caller can compute a consistent speedup ratio.
 */
static uint32_t zz9k_cryptobench_ms_x100_per_op(ZZ9KCryptoBenchTick elapsed,
                                                 uint32_t count,
                                                 uint32_t ticks_per_second)
{
  uint64_t numerator;

  if (elapsed == 0 || count == 0 || ticks_per_second == 0) {
    return 0U;
  }
  /* ms * 100 = (elapsed * 100000) / (count * ticks_per_second) */
  numerator = (uint64_t)elapsed * 100000ULL;
  return (uint32_t)(numerator / ((uint64_t)count * ticks_per_second));
}

#ifndef ZZ9K_CRYPTOBENCH_NO_MAIN

static void zz9k_cryptobench_fill_pattern(uint8_t *data, uint32_t length)
{
  uint32_t i;

  for (i = 0; i < length; i++) {
    data[i] = (uint8_t)((i * 37U + 11U) & 0xffU);
  }
}

/*
 * RFC 7748 Section 5.2 test vector 1, shared by the software and offload
 * timing paths so both always measure identical inputs and either result
 * can be verified against the known shared secret.
 */
static const uint8_t
zz9k_cryptobench_x25519_scalar[ZZ9K_CRYPTO_X25519_KEY_BYTES] = {
  0xa5, 0x46, 0xe3, 0x6b, 0xf0, 0x52, 0x7c, 0x9d,
  0x3b, 0x16, 0x15, 0x4b, 0x82, 0x46, 0x5e, 0xdd,
  0x62, 0x14, 0x4c, 0x0a, 0xc1, 0xfc, 0x5a, 0x18,
  0x50, 0x6a, 0x22, 0x44, 0xba, 0x44, 0x9a, 0xc4
};
static const uint8_t
zz9k_cryptobench_x25519_point[ZZ9K_CRYPTO_X25519_KEY_BYTES] = {
  0xe6, 0xdb, 0x68, 0x67, 0x58, 0x30, 0x30, 0xdb,
  0x35, 0x94, 0xc1, 0xa4, 0x24, 0xb1, 0x5f, 0x7c,
  0x72, 0x66, 0x24, 0xec, 0x26, 0xb3, 0x35, 0x3b,
  0x10, 0xa9, 0x03, 0xa6, 0xd0, 0xab, 0x1c, 0x4c
};
static const uint8_t
zz9k_cryptobench_x25519_shared[ZZ9K_CRYPTO_X25519_SHARED_BYTES] = {
  0xc3, 0xda, 0x55, 0x37, 0x9d, 0xe9, 0xc6, 0x90,
  0x8e, 0x94, 0xea, 0x4d, 0xf2, 0x8d, 0x08, 0x4f,
  0x32, 0xec, 0xcf, 0x03, 0x49, 0x1c, 0x71, 0xf7,
  0x54, 0xb4, 0x07, 0x55, 0x77, 0xa2, 0x85, 0x52
};

/*
 * Times software X25519 over `count` operations using the RFC 7748 section 5.2
 * test vector.  Returns ms*100 per op (hundredths of a millisecond) so the
 * caller can print "X.XX ms/op" without floating-point, or 0 if the computed
 * shared secret does not match the known answer.
 */
static uint32_t zz9k_cryptobench_x25519_soft_ms_x100(
    const ZZ9KCryptoBenchTimer *timer, uint32_t count)
{
  uint8_t out[ZZ9K_CRYPTO_X25519_SHARED_BYTES];
  ZZ9KCryptoBenchTick start;
  ZZ9KCryptoBenchTick elapsed;
  uint32_t i;

  start = zz9k_cryptobench_timer_now(timer);
  for (i = 0; i < count; i++) {
    zz9k_soft_x25519(out, zz9k_cryptobench_x25519_scalar,
                     zz9k_cryptobench_x25519_point);
  }
  elapsed = zz9k_cryptobench_timer_now(timer) - start;
  if (memcmp(out, zz9k_cryptobench_x25519_shared, sizeof(out)) != 0) {
    printf("x25519 software: RFC 7748 verification FAILED\n");
    return 0U;
  }
  return zz9k_cryptobench_ms_x100_per_op(elapsed, count,
                                          timer->ticks_per_second);
}

/*
 * Times ZZ9000 offload X25519 over `count` operations and verifies the
 * computed shared secret against the RFC 7748 known answer.
 * Returns ms*100 per op, or 0 on failure or verification mismatch.
 */
static uint32_t zz9k_cryptobench_x25519_offload_ms_x100(
    ZZ9KContext *ctx, const ZZ9KCryptoBenchTimer *timer, uint32_t count)
{
  ZZ9KSharedBuffer scalar_buf;
  ZZ9KSharedBuffer point_buf;
  ZZ9KSharedBuffer out_buf;
  ZZ9KCryptoKxDesc desc;
  ZZ9KCryptoResult result;
  ZZ9KCryptoBenchTick start;
  ZZ9KCryptoBenchTick elapsed;
  uint32_t ms_x100 = 0U;
  uint32_t i;
  int status;

  memset(&scalar_buf, 0, sizeof(scalar_buf));
  memset(&point_buf,  0, sizeof(point_buf));
  memset(&out_buf,    0, sizeof(out_buf));

  status = zz9k_alloc_shared(ctx, ZZ9K_CRYPTO_X25519_KEY_BYTES, 16U, 0,
                             &scalar_buf);
  if (status != ZZ9K_STATUS_OK) {
    printf("x25519 offload scalar alloc: %s (%d)\n",
           zz9k_status_name(status), status);
    goto out;
  }
  status = zz9k_alloc_shared(ctx, ZZ9K_CRYPTO_X25519_KEY_BYTES, 16U, 0,
                             &point_buf);
  if (status != ZZ9K_STATUS_OK) {
    printf("x25519 offload point alloc: %s (%d)\n",
           zz9k_status_name(status), status);
    goto out;
  }
  status = zz9k_alloc_shared(ctx, ZZ9K_CRYPTO_X25519_SHARED_BYTES, 16U, 0,
                             &out_buf);
  if (status != ZZ9K_STATUS_OK) {
    printf("x25519 offload output alloc: %s (%d)\n",
           zz9k_status_name(status), status);
    goto out;
  }

  memcpy((void *)scalar_buf.data, zz9k_cryptobench_x25519_scalar,
         ZZ9K_CRYPTO_X25519_KEY_BYTES);
  memcpy((void *)point_buf.data, zz9k_cryptobench_x25519_point,
         ZZ9K_CRYPTO_X25519_KEY_BYTES);

  if (!zz9k_crypto_build_x25519_desc(&desc,
                                     scalar_buf.handle, 0U,
                                     point_buf.handle,  0U,
                                     out_buf.handle,    0U)) {
    printf("x25519 offload descriptor build failed\n");
    goto out;
  }

  start = zz9k_cryptobench_timer_now(timer);
  for (i = 0; i < count; i++) {
    memset(&result, 0, sizeof(result));
    status = zz9k_crypto_kx(ctx, &desc, &result);
    if (status != ZZ9K_STATUS_OK) {
      printf("x25519 offload: %s (%d)\n", zz9k_status_name(status), status);
      goto out;
    }
  }
  elapsed = zz9k_cryptobench_timer_now(timer) - start;

  if (memcmp((const void *)out_buf.data, zz9k_cryptobench_x25519_shared,
             ZZ9K_CRYPTO_X25519_SHARED_BYTES) != 0) {
    printf("x25519 offload: RFC 7748 verification FAILED "
           "(firmware computed a wrong shared secret)\n");
    goto out;
  }

  ms_x100 = zz9k_cryptobench_ms_x100_per_op(elapsed, count,
                                              timer->ticks_per_second);

out:
  if (out_buf.handle != 0 && out_buf.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, out_buf.handle);
  }
  if (point_buf.handle != 0 && point_buf.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, point_buf.handle);
  }
  if (scalar_buf.handle != 0 && scalar_buf.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, scalar_buf.handle);
  }
  return ms_x100;
}

/* Times the software AEAD over `iterations` records of `size` bytes. */
static uint32_t zz9k_cryptobench_software_rate(
    const ZZ9KCryptoBenchTimer *timer, uint32_t size, uint32_t iterations,
    uint8_t *plaintext, uint8_t *ciphertext)
{
  static const uint8_t key[ZZ9K_CRYPTOBENCH_KEY_BYTES] = {
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
  };
  static const uint8_t nonce[ZZ9K_CRYPTOBENCH_NONCE_BYTES] = {
    0x07, 0x00, 0x00, 0x00, 0x40, 0x41,
    0x42, 0x43, 0x44, 0x45, 0x46, 0x47
  };
  uint8_t tag[ZZ9K_CRYPTOBENCH_TAG_BYTES];
  ZZ9KCryptoBenchTick start;
  ZZ9KCryptoBenchTick elapsed;
  uint32_t i;

  start = zz9k_cryptobench_timer_now(timer);
  for (i = 0; i < iterations; i++) {
    zz9k_soft_chacha20_poly1305_encrypt(ciphertext, tag, plaintext, size,
                                        0, 0U, key, nonce);
  }
  elapsed = zz9k_cryptobench_timer_now(timer) - start;
  return zz9k_cryptobench_kib_per_second(size * iterations, elapsed,
                                         timer->ticks_per_second);
}

/* Times the ZZ9000 offload AEAD over `iterations` records of `size` bytes. */
static uint32_t zz9k_cryptobench_offload_rate(
    ZZ9KContext *ctx, const ZZ9KCryptoBenchTimer *timer, uint32_t size,
    uint32_t iterations, const ZZ9KSharedBuffer *input,
    const ZZ9KSharedBuffer *output, const ZZ9KSharedBuffer *key,
    const ZZ9KSharedBuffer *nonce)
{
  ZZ9KCryptoAeadDesc desc;
  ZZ9KCryptoResult result;
  ZZ9KCryptoBenchTick start;
  ZZ9KCryptoBenchTick elapsed;
  uint32_t i;
  int status;

  if (!zz9k_crypto_build_chacha20_poly1305_desc(
          &desc, input->handle, 0U, size, output->handle, 0U, 0U, 0U, 0U,
          key->handle, 0U, nonce->handle, 0U)) {
    printf("offload %5lu: descriptor build failed\n", (unsigned long)size);
    return 0;
  }

  start = zz9k_cryptobench_timer_now(timer);
  for (i = 0; i < iterations; i++) {
    memset(&result, 0, sizeof(result));
    status = zz9k_crypto_aead(ctx, &desc, &result);
    if (status != ZZ9K_STATUS_OK) {
      printf("offload %5lu: %s (%d)\n", (unsigned long)size,
             zz9k_status_name(status), status);
      return 0;
    }
  }
  elapsed = zz9k_cryptobench_timer_now(timer) - start;
  return zz9k_cryptobench_kib_per_second(size * iterations, elapsed,
                                         timer->ticks_per_second);
}

static int zz9k_cryptobench_run_offload_sweep(
    ZZ9KContext *ctx, const ZZ9KCryptoBenchTimer *timer, uint32_t iterations,
    const uint32_t *sizes, const uint32_t *software_kib,
    uint32_t *offload_kib)
{
  ZZ9KSharedBuffer input;
  ZZ9KSharedBuffer output;
  ZZ9KSharedBuffer key;
  ZZ9KSharedBuffer nonce;
  uint32_t i;
  int status;
  int rc = 1;

  memset(&input, 0, sizeof(input));
  memset(&output, 0, sizeof(output));
  memset(&key, 0, sizeof(key));
  memset(&nonce, 0, sizeof(nonce));

  status = zz9k_alloc_shared(ctx, ZZ9K_CRYPTOBENCH_MAX_RECORD_BYTES, 16U, 0,
                             &input);
  if (status != ZZ9K_STATUS_OK) {
    printf("offload input alloc: %s (%d)\n", zz9k_status_name(status),
           status);
    goto out;
  }
  status = zz9k_alloc_shared(
      ctx, ZZ9K_CRYPTOBENCH_MAX_RECORD_BYTES + ZZ9K_CRYPTOBENCH_TAG_BYTES,
      16U, 0, &output);
  if (status != ZZ9K_STATUS_OK) {
    printf("offload output alloc: %s (%d)\n", zz9k_status_name(status),
           status);
    goto out;
  }
  status = zz9k_alloc_shared(ctx, ZZ9K_CRYPTOBENCH_KEY_BYTES, 16U, 0, &key);
  if (status != ZZ9K_STATUS_OK) {
    printf("offload key alloc: %s (%d)\n", zz9k_status_name(status), status);
    goto out;
  }
  status = zz9k_alloc_shared(ctx, ZZ9K_CRYPTOBENCH_NONCE_BYTES, 16U, 0,
                             &nonce);
  if (status != ZZ9K_STATUS_OK) {
    printf("offload nonce alloc: %s (%d)\n", zz9k_status_name(status),
           status);
    goto out;
  }

  zz9k_cryptobench_fill_pattern((uint8_t *)input.data,
                                ZZ9K_CRYPTOBENCH_MAX_RECORD_BYTES);
  zz9k_cryptobench_fill_pattern((uint8_t *)key.data,
                                ZZ9K_CRYPTOBENCH_KEY_BYTES);
  zz9k_cryptobench_fill_pattern((uint8_t *)nonce.data,
                                ZZ9K_CRYPTOBENCH_NONCE_BYTES);

  for (i = 0; i < ZZ9K_CRYPTOBENCH_SWEEP_COUNT; i++) {
    offload_kib[i] = zz9k_cryptobench_offload_rate(
        ctx, timer, sizes[i], iterations, &input, &output, &key, &nonce);
    printf("%6lu  %10lu  %10lu\n", (unsigned long)sizes[i],
           (unsigned long)software_kib[i], (unsigned long)offload_kib[i]);
  }
  rc = 0;

out:
  if (nonce.handle != 0 && nonce.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, nonce.handle);
  }
  if (key.handle != 0 && key.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, key.handle);
  }
  if (output.handle != 0 && output.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, output.handle);
  }
  if (input.handle != 0 && input.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, input.handle);
  }
  return rc;
}

int main(int argc, char **argv)
{
  static uint8_t plaintext[ZZ9K_CRYPTOBENCH_MAX_RECORD_BYTES];
  static uint8_t ciphertext[ZZ9K_CRYPTOBENCH_MAX_RECORD_BYTES +
                            ZZ9K_CRYPTOBENCH_TAG_BYTES];
  uint32_t sizes[ZZ9K_CRYPTOBENCH_SWEEP_COUNT];
  uint32_t software_kib[ZZ9K_CRYPTOBENCH_SWEEP_COUNT];
  uint32_t offload_kib[ZZ9K_CRYPTOBENCH_SWEEP_COUNT];
  ZZ9KCryptoBenchTimer timer;
  ZZ9KContext *ctx = 0;
  ZZ9KCaps caps;
  uint32_t iterations;
  uint32_t i;
  uint32_t breakeven;
  int status;

  iterations = zz9k_cryptobench_parse_iterations(argc, argv);
  zz9k_cryptobench_fill_pattern(plaintext, sizeof(plaintext));
  zz9k_cryptobench_timer_open(&timer);

  for (i = 0; i < ZZ9K_CRYPTOBENCH_SWEEP_COUNT; i++) {
    sizes[i] = zz9k_cryptobench_sweep_size(i);
    offload_kib[i] = 0U;
  }

  printf("ChaCha20-Poly1305 break-even sweep (%lu iterations/size)\n",
         (unsigned long)iterations);
  printf("%6s  %10s  %10s\n", "bytes", "soft KiB/s", "off KiB/s");

  for (i = 0; i < ZZ9K_CRYPTOBENCH_SWEEP_COUNT; i++) {
    software_kib[i] = zz9k_cryptobench_software_rate(
        &timer, sizes[i], iterations, plaintext, ciphertext);
  }

  status = zz9k_open(&ctx);
  if (status == ZZ9K_STATUS_OK) {
    status = zz9k_query_caps(ctx, &caps);
  }
  if (status == ZZ9K_STATUS_OK &&
      (caps.capability_bits & ZZ9K_CAP_CRYPTO) != 0U) {
    if (zz9k_cryptobench_run_offload_sweep(ctx, &timer, iterations, sizes,
                                           software_kib, offload_kib) != 0) {
      zz9k_close(ctx);
      zz9k_cryptobench_timer_close(&timer);
      return 1;
    }
  } else {
    printf("crypto offload unavailable; software sweep only:\n");
    for (i = 0; i < ZZ9K_CRYPTOBENCH_SWEEP_COUNT; i++) {
      printf("%6lu  %10lu  %10s\n", (unsigned long)sizes[i],
             (unsigned long)software_kib[i], "-");
    }
  }

  breakeven = zz9k_cryptobench_breakeven_size(sizes, software_kib,
                                              offload_kib,
                                              ZZ9K_CRYPTOBENCH_SWEEP_COUNT);
  if (breakeven != 0U) {
    printf("break-even: offload wins from %lu bytes\n",
           (unsigned long)breakeven);
  } else {
    printf("break-even: offload did not beat software in this sweep\n");
  }

  /* X25519 key-exchange benchmark */
  {
    uint32_t soft_ms_x100;
    uint32_t off_ms_x100 = 0U;
    ZZ9KServiceInfo crypto_service;

    printf("\nX25519 key exchange (%lu iterations):\n",
           (unsigned long)iterations);

    soft_ms_x100 = zz9k_cryptobench_x25519_soft_ms_x100(&timer, iterations);
    printf("  Software (m68k):  %lu.%02lu ms/op\n",
           (unsigned long)(soft_ms_x100 / 100U),
           (unsigned long)(soft_ms_x100 % 100U));

    if (ctx != 0 && status == ZZ9K_STATUS_OK &&
        (caps.capability_bits & ZZ9K_CAP_CRYPTO) != 0U) {
      int qs = zz9k_query_service(ctx, ZZ9K_SERVICE_CRYPTO, &crypto_service);
      if (qs == ZZ9K_STATUS_OK &&
          (crypto_service.flags & ZZ9K_SERVICE_FLAG_CRYPTO_X25519) != 0U) {
        off_ms_x100 = zz9k_cryptobench_x25519_offload_ms_x100(
            ctx, &timer, iterations);
        if (off_ms_x100 > 0U && soft_ms_x100 > 0U) {
          uint32_t speedup_x100 = (uint32_t)(
              ((uint64_t)soft_ms_x100 * 100ULL) / off_ms_x100);
          printf("  Offload (ZZ9000): %lu.%02lu ms/op  [%lu.%02lux speedup]\n",
                 (unsigned long)(off_ms_x100 / 100U),
                 (unsigned long)(off_ms_x100 % 100U),
                 (unsigned long)(speedup_x100 / 100U),
                 (unsigned long)(speedup_x100 % 100U));
        } else if (off_ms_x100 > 0U) {
          printf("  Offload (ZZ9000): %lu.%02lu ms/op\n",
                 (unsigned long)(off_ms_x100 / 100U),
                 (unsigned long)(off_ms_x100 % 100U));
        } else {
          printf("  Offload (ZZ9000): measurement failed\n");
        }
      } else {
        printf("  Offload (ZZ9000): X25519 not advertised by crypto service\n");
      }
    }
  }

  if (ctx) {
    zz9k_close(ctx);
  }
  zz9k_cryptobench_timer_close(&timer);
  return 0;
}

#endif /* ZZ9K_CRYPTOBENCH_NO_MAIN */
