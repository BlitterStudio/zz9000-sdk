/*
 * ZZ9000AX audio fabric lease proof client (plan U4, R13/AE5).
 *
 * Exercises the fabric lease plane end to end: the capability gate
 * (mixed-version fallback -- old firmware declines cleanly), the
 * Zorro 3 first gating, HOST_WINDOW staging allocation, LEASE_BEGIN
 * on a caller-selected slot (1 by default; slot 2 for the B4
 * instrument build), a generated 48 kHz stereo S16 tone fed in
 * submit-sized chunks with partial-accept retry, FABRIC_STATE_GET
 * polling, LEASE_RELEASE and a clean exit. A proof client, not a player.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k/abi.h"
#include "zz9k/audio.h"
#include "zz9k/caps.h"
#include "zz9k/host.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__amigaos__) || defined(__amiga__) || defined(__AMIGA__) || \
    defined(__VBCC__)
#define ZZ9K_FABRICLEASE_AMIGA 1
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>
#endif

/* Set by the contract test, which compiles this file directly and
 * drives the session against a mock mailbox. */
#ifndef ZZ9K_FABRICLEASE_NO_MAIN
#define ZZ9K_FABRICLEASE_NO_MAIN 0
#endif

typedef int (*ZZ9KFabricLeaseCancelHook)(void *user);

static ZZ9KFabricLeaseCancelHook g_cancel_hook;
static void *g_cancel_user;

#if ZZ9K_FABRICLEASE_AMIGA
static volatile sig_atomic_t g_ctrl_c_requested;

static void fabriclease_sigint(int sig)
{
  (void)sig;
  g_ctrl_c_requested = 1;
}
#endif

void zz9k_fabriclease_set_cancel_hook_for_test(
    ZZ9KFabricLeaseCancelHook hook, void *user)
{
  g_cancel_hook = hook;
  g_cancel_user = user;
}

static int fabriclease_cancel_requested(void)
{
  if (g_cancel_hook && g_cancel_hook(g_cancel_user)) {
    return 1;
  }
#if ZZ9K_FABRICLEASE_AMIGA
  if (g_ctrl_c_requested ||
      (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0U) {
    g_ctrl_c_requested = 1;
    return 1;
  }
#endif
  return 0;
}

/* HOST_WINDOW staging: four complete 20-ms periods plus margin. A
 * 4096-byte (21.3-ms) chunk left only 1.3 ms for tone generation and
 * two mailbox round trips on the real Amiga, causing repeated lease
 * underruns. 16 KiB preloads 85.3 ms before first activation and cuts
 * the sustained submit cadence to about 11.7 calls/s. */
#define ZZ9K_FABRICLEASE_STAGING_BYTES 16384U
#define ZZ9K_FABRICLEASE_RATE_HZ 48000U
#define ZZ9K_FABRICLEASE_FRAME_BYTES 4U
#define ZZ9K_FABRICLEASE_BYTES_PER_SECOND \
  (ZZ9K_FABRICLEASE_RATE_HZ * ZZ9K_FABRICLEASE_FRAME_BYTES)
#define ZZ9K_FABRICLEASE_DEFAULT_SECONDS 5U
/* Keep-ahead stays below the 61,440-byte card-side lease ring. */
#define ZZ9K_FABRICLEASE_HIGH_WATER_BYTES \
  (3U * ZZ9K_FABRICLEASE_STAGING_BYTES)
/* 48-sample sign blocks at 48 kHz ~= 500 Hz square bursts; integer
 * only (no libm on every toolchain) and phase-continuous across
 * chunks. */
#define ZZ9K_FABRICLEASE_BURST_SAMPLES 48U
#define ZZ9K_FABRICLEASE_AMPLITUDE 12000

typedef enum ZZ9KFabricLeaseStatus {
  ZZ9K_FABRICLEASE_OK = 0,
  ZZ9K_FABRICLEASE_DECLINE_NO_FABRIC, /* firmware predates the plane */
  ZZ9K_FABRICLEASE_DECLINE_ZORRO2     /* Zorro 3 first gating */
} ZZ9KFabricLeaseStatus;

typedef struct ZZ9KFabricLeaseOptions {
  uint32_t seconds; /* bounded run length; 0 runs one chunk */
  uint32_t gain;    /* requested 0..255 producer scale */
  uint32_t slot;    /* fabric lease slot 1 or 2 */
} ZZ9KFabricLeaseOptions;
const char *zz9k_fabriclease_status_name(int status)
{
  switch (status) {
  case ZZ9K_FABRICLEASE_OK:
    return "ok";
  case ZZ9K_FABRICLEASE_DECLINE_NO_FABRIC:
    return "firmware lacks the audio fabric (capability not advertised)";
  case ZZ9K_FABRICLEASE_DECLINE_ZORRO2:
    return "audio fabric leases are Zorro 3 first (this board is Zorro 2)";
  default:
    return "error";
  }
}

/*
 * The mixed-version and bus-shape gate, pure so the contract test can
 * drive every path: Zorro 2 declines before anything is opened (the
 * fabric lease rings live in the Z3-mapped DDR window), a firmware
 * without ZZ9K_CAP_AUDIO_FABRIC declines cleanly -- the R13/AE5
 * fallback proof, not an error.
 */
int zz9k_fabriclease_gate(const ZZ9KBoard *board, uint32_t capability_bits)
{
  if (board != 0 && board->zorro_version == 2U) {
    return ZZ9K_FABRICLEASE_DECLINE_ZORRO2;
  }
  if ((capability_bits & ZZ9K_CAP_AUDIO_FABRIC) == 0U) {
    return ZZ9K_FABRICLEASE_DECLINE_NO_FABRIC;
  }
  return ZZ9K_FABRICLEASE_OK;
}

static void fabriclease_wait(void)
{
#if ZZ9K_FABRICLEASE_AMIGA
  Delay(1L); /* one 20 ms DOS tick: enough for a period of playback */
#else
  volatile uint32_t spin;

  for (spin = 0; spin < 1000000UL; spin++) {
  }
#endif
}

/* One chunk of the burst tone, continuing the absolute sample index
 * so the stream is phase-continuous across submits. */
static void fabriclease_fill_chunk(volatile void *staging, uint64_t first,
                                   uint32_t bytes)
{
  volatile uint8_t *dst = (volatile uint8_t *)staging;
  uint32_t frames = bytes / ZZ9K_FABRICLEASE_FRAME_BYTES;
  uint32_t f;

  for (f = 0; f < frames; f++) {
    uint64_t left = first + (uint64_t)f;
    int16_t v = ((left / ZZ9K_FABRICLEASE_BURST_SAMPLES) & 1U)
                    ? (int16_t)ZZ9K_FABRICLEASE_AMPLITUDE
                    : (int16_t)-ZZ9K_FABRICLEASE_AMPLITUDE;
    uint16_t u = (uint16_t)v;

    dst[f * 4U + 0U] = (uint8_t)(u & 0xffU);
    dst[f * 4U + 1U] = (uint8_t)(u >> 8);
    dst[f * 4U + 2U] = (uint8_t)(u & 0xffU);
    dst[f * 4U + 3U] = (uint8_t)(u >> 8);
  }
}

static int fabriclease_print_state(ZZ9KContext *ctx, uint32_t slot)
{
  ZZ9KAudioFabricStateDesc desc;
  ZZ9KAudioFabricStateResult state;

  if (!zz9k_audio_build_fabric_state_desc(&desc, slot, 0U)) {
    return ZZ9K_STATUS_BAD_REQUEST;
  }
  if (zz9k_audio_fabric_state_get(ctx, &desc, &state) != ZZ9K_STATUS_OK) {
    return ZZ9K_STATUS_INTERNAL_ERROR;
  }
  printf("fabric: slot %lu state=%lu gen=%lu w=%lu r=%lu "
         "underruns=%lu peak=0x%06lx clip=%lu\n",
         (unsigned long)state.slot, (unsigned long)state.state,
         (unsigned long)state.generation,
         (unsigned long)state.cursor_write,
         (unsigned long)state.cursor_read,
         (unsigned long)state.underrun_count,
         (unsigned long)state.peak, (unsigned long)state.clip);
  return ZZ9K_STATUS_OK;
}

/*
 * The lease lifecycle against one open context: staging allocation,
 * BEGIN, the feed loop (chunked submits, partial-accept retry, BUSY
 * backpressure, state polling under the keep-ahead water mark),
 * bounded drain, RELEASE, cleanup. Returns a ZZ9K_STATUS_* word.
 */
int zz9k_fabriclease_session(ZZ9KContext *ctx,
                             const ZZ9KFabricLeaseOptions *options)
{
  ZZ9KAudioLeaseBeginDesc begin;
  ZZ9KAudioLeaseBeginResult granted;
  ZZ9KAudioLeaseSubmitDesc submit;
  ZZ9KAudioLeaseSubmitResult accepted;
  ZZ9KAudioFabricStateDesc state_desc;
  ZZ9KAudioFabricStateResult state;
  ZZ9KSharedBuffer staging;
  uint64_t target_bytes;
  uint64_t submitted = 0U;
  uint64_t confirmed = 0U;
  uint32_t chunk_bytes;
  uint32_t state_polls = 0U;
  int status;

  if (ctx == 0 || options == 0 || options->gain > 255U ||
      options->slot < 1U || options->slot > 2U) {
    return ZZ9K_STATUS_BAD_REQUEST;
  }
  if (fabriclease_cancel_requested()) {
    return ZZ9K_STATUS_CANCELLED;
  }
  target_bytes = options->seconds == 0U
                     ? (uint64_t)ZZ9K_FABRICLEASE_STAGING_BYTES
                     : (uint64_t)options->seconds *
                           (uint64_t)ZZ9K_FABRICLEASE_BYTES_PER_SECOND;

  /* HOST_WINDOW staging: the Z3 shared heap (the host-window flag is
   * a Z2 mapping concern the library itself drops on Z3). */
  memset(&staging, 0, sizeof(staging));
  status = zz9k_alloc_shared(ctx, ZZ9K_FABRICLEASE_STAGING_BYTES, 4U,
                             ZZ9K_ALLOC_HOST_WINDOW, &staging);
  if (status != ZZ9K_STATUS_OK) {
    printf("fabriclease: staging allocation failed: %s (%d)\n",
           zz9k_status_name(status), status);
    return status;
  }
  chunk_bytes = staging.length < ZZ9K_FABRICLEASE_STAGING_BYTES
                    ? staging.length
                    : ZZ9K_FABRICLEASE_STAGING_BYTES;
  chunk_bytes &= ~(uint32_t)(ZZ9K_FABRICLEASE_FRAME_BYTES - 1U);

  if (!zz9k_audio_build_lease_begin_desc(
          &begin, options->slot, ZZ9K_AUDIO_METER_IDENTITY_SDK_STREAM,
          options->gain, 0U)) {
    zz9k_free_shared(ctx, staging.handle);
    return ZZ9K_STATUS_BAD_REQUEST;
  }
  status = zz9k_audio_lease_begin(ctx, &begin, &granted);
  if (status != ZZ9K_STATUS_OK) {
    printf("fabriclease: lease begin failed: %s (%d)\n",
           zz9k_status_name(status), status);
    zz9k_free_shared(ctx, staging.handle);
    return status;
  }
  printf("fabriclease: slot %lu leased, handle 0x%08lx, gain %lu/255",
         (unsigned long)granted.slot, (unsigned long)granted.lease,
         (unsigned long)granted.gain_applied);
  if ((granted.flags & ZZ9K_AUDIO_LEASE_RESULT_GAIN_BOUNDED) != 0U) {
    printf(" (bounded from %lu by the ceiling composition)",
           (unsigned long)options->gain);
  }
  printf("\n");

  while (submitted < target_bytes) {
    uint32_t offset = 0U;
    uint32_t length = chunk_bytes;
    uint32_t wait_polls = 0U;

    if (fabriclease_cancel_requested()) {
      status = ZZ9K_STATUS_CANCELLED;
      goto out;
    }
    if (length > (uint32_t)(target_bytes - submitted)) {
      length = (uint32_t)(target_bytes - submitted);
      length &= ~(uint32_t)(ZZ9K_FABRICLEASE_FRAME_BYTES - 1U);
      if (length == 0U) {
        break;
      }
    }

    /* Keep-ahead: let playback drain while the card owes more than
     * the high-water limit. */
    for (;;) {
      uint32_t owed;

      if (fabriclease_cancel_requested()) {
        status = ZZ9K_STATUS_CANCELLED;
        goto out;
      }
      if (!zz9k_audio_build_fabric_state_desc(
              &state_desc, options->slot, 0U)) {
        status = ZZ9K_STATUS_BAD_REQUEST;
        goto out;
      }
      status = zz9k_audio_fabric_state_get(ctx, &state_desc, &state);
      if (status != ZZ9K_STATUS_OK) {
        goto out;
      }
      confirmed = state.cursor_read;
      if ((state_polls & 15U) == 0U) {
        printf("fabric: slot %lu state=%lu gen=%lu w=%lu r=%lu "
               "underruns=%lu peak=0x%06lx clip=%lu\n",
               (unsigned long)state.slot, (unsigned long)state.state,
               (unsigned long)state.generation,
               (unsigned long)state.cursor_write,
               (unsigned long)state.cursor_read,
               (unsigned long)state.underrun_count,
               (unsigned long)state.peak, (unsigned long)state.clip);
      }
      state_polls++;
      owed = (uint32_t)(state.cursor_write - state.cursor_read);
      if (owed < ZZ9K_FABRICLEASE_HIGH_WATER_BYTES) {
        break;
      }
      if (++wait_polls > 10000U) {
        status = ZZ9K_STATUS_TIMEOUT;
        goto out;
      }
      fabriclease_wait();
      if (fabriclease_cancel_requested()) {
        status = ZZ9K_STATUS_CANCELLED;
        goto out;
      }
    }

    fabriclease_fill_chunk(staging.data, submitted / 4U, length);
    while (offset < length) {
      if (fabriclease_cancel_requested()) {
        status = ZZ9K_STATUS_CANCELLED;
        goto out;
      }
      if (!zz9k_audio_build_lease_submit_desc(&submit, granted.lease,
                                              staging.handle, offset,
                                              length - offset, 0U)) {
        status = ZZ9K_STATUS_BAD_REQUEST;
        goto out;
      }
      status = zz9k_audio_lease_submit(ctx, &submit, &accepted);
      if (status == ZZ9K_STATUS_BUSY) {
        /* Ring full: the staged chunk stays valid, retry after a
         * period of playback (one mailbox op per retry). */
        fabriclease_wait();
        if (fabriclease_cancel_requested()) {
          status = ZZ9K_STATUS_CANCELLED;
          goto out;
        }
        continue;
      }
      if (status != ZZ9K_STATUS_OK) {
        goto out;
      }
      offset += accepted.bytes_consumed;
      submitted += accepted.bytes_consumed;
    }
  }

  /* Bounded drain: wait for the compositor to consume what it owes
   * (a proof client leaves no staged bytes behind). */
  {
    uint32_t drain_polls = 0U;

    for (;;) {
      if (fabriclease_cancel_requested()) {
        status = ZZ9K_STATUS_CANCELLED;
        goto out;
      }
      if (!zz9k_audio_build_fabric_state_desc(
              &state_desc, options->slot, 0U)) {
        status = ZZ9K_STATUS_BAD_REQUEST;
        goto out;
      }
      status = zz9k_audio_fabric_state_get(ctx, &state_desc, &state);
      if (status != ZZ9K_STATUS_OK) {
        goto out;
      }
      confirmed = state.cursor_read;
      if (confirmed >= submitted) {
        break;
      }
      if (++drain_polls > 10000U) {
        status = ZZ9K_STATUS_TIMEOUT;
        goto out;
      }
      fabriclease_wait();
      if (fabriclease_cancel_requested()) {
        status = ZZ9K_STATUS_CANCELLED;
        goto out;
      }
    }
  }
  status = fabriclease_print_state(ctx, options->slot);

out:
  if (zz9k_audio_lease_release(ctx, granted.lease, 0U) !=
      ZZ9K_STATUS_OK) {
    printf("fabriclease: lease release failed\n");
    if (status == ZZ9K_STATUS_OK || status == ZZ9K_STATUS_CANCELLED) {
      status = ZZ9K_STATUS_INTERNAL_ERROR;
    }
  }
  if (fabriclease_print_state(ctx, options->slot) == ZZ9K_STATUS_OK &&
      status == ZZ9K_STATUS_OK) {
    printf("fabriclease: released after %lu bytes (%lu confirmed)\n",
           (unsigned long)submitted, (unsigned long)confirmed);
  }
  zz9k_free_shared(ctx, staging.handle);
  return status;
}

#if !ZZ9K_FABRICLEASE_NO_MAIN
static void print_usage(void)
{
  printf("usage: zz9k-fabriclease [--seconds N] [--gain 0..255] "
         "[--slot 1|2] [--force]\n");
}

int main(int argc, char **argv)
{
  ZZ9KFabricLeaseOptions options;
  ZZ9KContext *ctx = 0;
  ZZ9KBoard board;
  ZZ9KCaps caps;
  int status;
  int i;
  int force = 0;
#if ZZ9K_FABRICLEASE_AMIGA
  (void)signal(SIGINT, fabriclease_sigint);
#endif

  memset(&options, 0, sizeof(options));
  options.seconds = ZZ9K_FABRICLEASE_DEFAULT_SECONDS;
  options.gain = 128U;
  options.slot = 1U;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
      options.seconds = (uint32_t)strtoul(argv[++i], 0, 10);
    } else if (strcmp(argv[i], "--gain") == 0 && i + 1 < argc) {
      options.gain = (uint32_t)strtoul(argv[++i], 0, 10);
    } else if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc) {
      options.slot = (uint32_t)strtoul(argv[++i], 0, 10);
    } else if (strcmp(argv[i], "--force") == 0) {
      force = 1;
    } else {
      print_usage();
      return 1;
    }
  }
  if (options.gain > 255U || options.slot < 1U || options.slot > 2U) {
    print_usage();
    return 1;
  }

  if (zz9k_find_board(&board) != ZZ9K_STATUS_OK) {
    memset(&board, 0, sizeof(board));
  }
  status = zz9k_fabriclease_gate(&board, 0U);
  if (status == ZZ9K_FABRICLEASE_DECLINE_ZORRO2) {
    printf("zz9k-fabriclease: %s\n", zz9k_fabriclease_status_name(status));
    return 0;
  }

  status = zz9k_open(&ctx);
  if (status != ZZ9K_STATUS_OK) {
    printf("zz9k-fabriclease: open failed: %s (%d)\n",
           zz9k_status_name(status), status);
    return 1;
  }
  memset(&caps, 0, sizeof(caps));
  status = zz9k_query_caps(ctx, &caps);
  if (status == ZZ9K_STATUS_OK) {
    status = zz9k_fabriclease_gate(&board, caps.capability_bits);
  }
  if (status == ZZ9K_FABRICLEASE_DECLINE_NO_FABRIC) {
    if (!force) {
      /* Mixed-version fallback (R13/AE5): a clean, documented decline,
       * not an error. */
      printf("zz9k-fabriclease: %s\n",
             zz9k_fabriclease_status_name(status));
      zz9k_close(ctx);
      return 0;
    }
    /* Qualification-only path: instrument firmware implements the
     * opcodes but deliberately withholds the capability until this
     * hardware session passes. Unsupported firmware still fails the
     * first lease opcode cleanly. */
    printf("zz9k-fabriclease: forcing unadvertised fabric opcodes "
           "(hardware qualification only)\n");
    status = ZZ9K_FABRICLEASE_OK;
  }
  if (status != ZZ9K_FABRICLEASE_OK) {
    printf("zz9k-fabriclease: %s (%d)\n", zz9k_status_name(status), status);
    zz9k_close(ctx);
    return 1;
  }

  status = zz9k_fabriclease_session(ctx, &options);
  zz9k_close(ctx);
  if (status == ZZ9K_STATUS_CANCELLED) {
    printf("zz9k-fabriclease: cancelled; lease and staging released\n");
    return 0;
  }
  if (status != ZZ9K_STATUS_OK) {
    printf("zz9k-fabriclease: session failed: %s (%d)\n",
           zz9k_status_name(status), status);
    return 1;
  }
  return 0;
}
#endif /* !ZZ9K_FABRICLEASE_NO_MAIN */
