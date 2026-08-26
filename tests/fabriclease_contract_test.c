/*
 * Client-contract host test for the audio fabric lease proof client
 * (plan U4, R13/AE5): the typed builder/validation layer, the
 * begin/submit/release/state dispatchers against a mock mailbox with
 * a scripted firmware responder, and both documented decline paths
 * (Zorro 2 board shape, firmware without ZZ9K_CAP_AUDIO_FABRIC).
 *
 * The tool is compiled in directly (single TU, the audio_meter_test
 * discipline) with its main() excluded, so every helper is reachable
 * without a board.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define ZZ9K_FABRICLEASE_NO_MAIN 1
#include "../tools/zz9k-fabriclease.c"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "zz9k/reply.h"
#include "zz9k/request.h"
#include <stdlib.h>
#include <string.h>

/* Test seam exported (undeclared) by host/src/zz9k_host.c. */
void zz9k_set_idle_hook_for_test(void (*hook)(void));

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static int g_failures;

#define CHECK(ok, name)                                                 \
  do {                                                                  \
    if (!(ok)) {                                                        \
      g_failures++;                                                     \
      printf("FAILED: %s (%s:%d)\n", (name), __FILE__, __LINE__);       \
    }                                                                   \
  } while (0)

/* A Zorro 3-shaped board window at a fixed low address so the staging
 * buffer's board mapping (board_addr + 0x10000) is real,
 * dereferenceable memory: the session writes its tone there. */
#define MOCK_BOARD_ADDR 0x20000000UL
#define MOCK_BOARD_SIZE 0x01000000UL
#define MOCK_RING_ENTRIES 8U
#define MOCK_LEASE_HANDLE 0x21U /* generation 2, slot 1 */
#define MOCK_LEASE_RING_CAPACITY 61440U

static void *mock_board_window(void)
{
#if defined(_WIN32)
  return VirtualAlloc((LPVOID)MOCK_BOARD_ADDR, (SIZE_T)MOCK_BOARD_SIZE,
                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
  void *p = mmap((void *)MOCK_BOARD_ADDR, (size_t)MOCK_BOARD_SIZE,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
  return p == MAP_FAILED ? NULL : p;
#endif
}

static void mock_board_window_free(void *p)
{
  if (!p) {
    return;
  }
#if defined(_WIN32)
  VirtualFree(p, 0, MEM_RELEASE);
#else
  munmap(p, (size_t)MOCK_BOARD_SIZE);
#endif
}

struct TestMailbox {
  ZZ9KMailboxDescriptor descriptor;
  ZZ9KMailboxWireEntry request_ring[MOCK_RING_ENTRIES];
  ZZ9KMailboxWireEntry completion_ring[MOCK_RING_ENTRIES];
};

static struct {
  struct TestMailbox mailbox;
  ZZ9KBoard board;
  uint32_t served_tail;
  int alloc_calls, free_calls, begin_calls, submit_calls, release_calls;
  int state_calls, busy_submits;
  uint32_t begin_slot, begin_identity, begin_gain;
  uint32_t submit_lease, submit_handle, submit_length_max;
  int submit_length_bad;
  uint32_t written, read;
  int released;
  int busy_once;      /* first submit completes BUSY, exactly once */
  int corrupt_begin;  /* begin reply carries an out-of-range gain */
} g_mock;

static void mock_reset(uint32_t caps, uint16_t zorro_version)
{
  struct TestMailbox *m;

  memset(&g_mock, 0, sizeof(g_mock));
  m = &g_mock.mailbox;
  zz9k_put_be32(m->descriptor.magic, ZZ9K_ABI_MAGIC);
  zz9k_put_be16(m->descriptor.abi_major, ZZ9K_ABI_VERSION_MAJOR);
  zz9k_put_be16(m->descriptor.abi_minor, ZZ9K_ABI_VERSION_MINOR);
  zz9k_put_be32(m->descriptor.descriptor_size,
                (uint32_t)sizeof(m->descriptor));
  zz9k_put_be32(m->descriptor.request_ring_offset,
                (uint32_t)offsetof(struct TestMailbox, request_ring));
  zz9k_put_be32(m->descriptor.request_ring_entries, MOCK_RING_ENTRIES);
  zz9k_put_be32(m->descriptor.completion_ring_offset,
                (uint32_t)offsetof(struct TestMailbox, completion_ring));
  zz9k_put_be32(m->descriptor.completion_ring_entries, MOCK_RING_ENTRIES);
  zz9k_put_be32(m->descriptor.capability_bits, ZZ9K_CAP_MAILBOX | caps);
  g_mock.board.zorro_version = zorro_version;
  g_mock.board.board_addr = MOCK_BOARD_ADDR;
  g_mock.board.board_size = MOCK_BOARD_SIZE;
  g_mock.board.product =
      zorro_version == 2U ? ZZ9K_PRODUCT_Z2 : ZZ9K_PRODUCT_Z3;
}

/* Stage the completion for the request at request_ring[head]; the
 * returned entry's payload is the caller's to fill. */
static ZZ9KMailboxWireEntry *mock_complete(const ZZ9KMailboxWireEntry *req,
                                           uint16_t status,
                                           uint16_t payload_len)
{
  struct TestMailbox *m = &g_mock.mailbox;
  ZZ9KMailboxWireEntry *reply;
  uint32_t tail = zz9k_get_be32(m->descriptor.completion_tail);

  reply = &m->completion_ring[tail % MOCK_RING_ENTRIES];
  memset((void *)reply->payload, 0, sizeof(reply->payload));
  zz9k_put_be32(reply->request_id, zz9k_get_be32(req->request_id));
  zz9k_put_be16(reply->opcode, zz9k_get_be16(req->opcode));
  zz9k_put_be16(reply->status, status);
  zz9k_put_be16(reply->payload_len, payload_len);
  zz9k_put_be32(reply->user_cookie, zz9k_get_be32(req->user_cookie));
  zz9k_put_be32(m->descriptor.completion_tail,
                (tail + 1U) % MOCK_RING_ENTRIES);
  zz9k_put_be32(m->descriptor.request_head,
                (zz9k_get_be32(m->descriptor.request_head) + 1U) %
                    MOCK_RING_ENTRIES);
  return reply;
}

static void mock_word(ZZ9KMailboxWireEntry *reply, uint32_t index,
                      uint32_t value)
{
  zz9k_put_be32(&reply->payload[index * 4U], value);
}

static uint32_t mock_req_word(const ZZ9KMailboxWireEntry *req,
                              uint32_t index)
{
  return zz9k_get_be32(&req->payload[index * 4U]);
}

/* The reactive responder: installed as the test idle hook, it serves
 * the just-enqueued request during the first poll backoff of its
 * zz9k_call, exactly like the firmware dispatcher would. */
static void mock_respond(void)
{
  struct TestMailbox *m = &g_mock.mailbox;
  const ZZ9KMailboxWireEntry *req;
  uint32_t head = zz9k_get_be32(m->descriptor.request_head);
  uint32_t tail = zz9k_get_be32(m->descriptor.request_tail);
  uint16_t opcode;
  ZZ9KMailboxWireEntry *reply;

  if (head == tail || g_mock.served_tail == tail) {
    return;
  }
  g_mock.served_tail = tail;
  req = &m->request_ring[head];
  opcode = zz9k_get_be16(req->opcode);

  switch (opcode) {
  case ZZ9K_OP_QUERY_CAPS:
    reply = mock_complete(req, ZZ9K_STATUS_OK, 40U);
    mock_word(reply, 0, ZZ9K_ABI_MAGIC);
    mock_word(reply, 1, ((uint32_t)ZZ9K_ABI_VERSION_MAJOR << 16) |
                            ZZ9K_ABI_VERSION_MINOR);
    mock_word(reply, 2,
              zz9k_get_be32(m->descriptor.capability_bits));
    mock_word(reply, 3, 48U);
    mock_word(reply, 4, 32U);
    mock_word(reply, 7, MOCK_RING_ENTRIES);
    mock_word(reply, 8, MOCK_RING_ENTRIES);
    break;
  case ZZ9K_OP_ALLOC_SHARED:
    g_mock.alloc_calls++;
    reply = mock_complete(req, ZZ9K_STATUS_OK, 16U);
    mock_word(reply, 0, 9U);                    /* handle */
    mock_word(reply, 1, ZZ9K_ARM_MEMORY_START); /* arm address */
    mock_word(reply, 2, mock_req_word(req, 0)); /* length echo */
    mock_word(reply, 3, 0U);
    break;
  case ZZ9K_OP_AUDIO_LEASE_BEGIN:
    g_mock.begin_calls++;
    g_mock.begin_slot = mock_req_word(req, 0);
    g_mock.begin_identity = mock_req_word(req, 1);
    g_mock.begin_gain = mock_req_word(req, 2);
    reply = mock_complete(req, ZZ9K_STATUS_OK,
                          sizeof(ZZ9KAudioLeaseBeginResultPayload));
    mock_word(reply, 0, (2U << 4) | g_mock.begin_slot);
    mock_word(reply, 1, g_mock.begin_slot);
    mock_word(reply, 2, 2U); /* generation: epoch of the handle */
    mock_word(reply, 3, g_mock.corrupt_begin
                            ? 0U
                            : ZZ9K_AUDIO_LEASE_RESULT_GAIN_BOUNDED);
    mock_word(reply, 4, g_mock.corrupt_begin ? 256U : 128U);
    break;
  case ZZ9K_OP_AUDIO_LEASE_SUBMIT: {
    uint32_t length = mock_req_word(req, 3);
    uint32_t space;
    uint32_t consumed;

    g_mock.submit_calls++;
    g_mock.submit_lease = mock_req_word(req, 0);
    g_mock.submit_handle = mock_req_word(req, 1);
    if (length > g_mock.submit_length_max) {
      g_mock.submit_length_max = length;
    }
    if ((length & 3U) != 0U || length == 0U) {
      g_mock.submit_length_bad = 1;
    }
    space = MOCK_LEASE_RING_CAPACITY -
            (g_mock.written - g_mock.read);
    if (g_mock.busy_once && g_mock.busy_submits == 0) {
      g_mock.busy_submits++;
      (void)mock_complete(req, ZZ9K_STATUS_BUSY, 0U);
      break;
    }
    if (space == 0U) {
      /* Real playback advances while the Amiga waits one DOS tick
       * after BUSY. Model one 20-ms period for the next retry. */
      uint32_t owed = g_mock.written - g_mock.read;
      uint32_t drained = owed < 3072U ? owed : 3072U;

      g_mock.read += drained;
      (void)mock_complete(req, ZZ9K_STATUS_BUSY, 0U);
      break;
    }
    consumed = length < space ? length : space;
    g_mock.written += consumed;
    reply = mock_complete(req, ZZ9K_STATUS_OK,
                          sizeof(ZZ9KAudioLeaseSubmitResultPayload));
    mock_word(reply, 0, g_mock.submit_lease);
    mock_word(reply, 1, consumed);
    mock_word(reply, 2, 0U);
    break;
  }
  case ZZ9K_OP_AUDIO_LEASE_RELEASE:
    g_mock.release_calls++;
    g_mock.released = 1;
    (void)mock_complete(req, ZZ9K_STATUS_OK, 0U);
    break;
  case ZZ9K_OP_AUDIO_FABRIC_STATE_GET: {
    uint32_t state;

    g_mock.state_calls++;
    /* Playback drains 3072 bytes per state poll. */
    g_mock.read += 3072U;
    if (g_mock.read > g_mock.written) {
      g_mock.read = g_mock.written;
    }
    state = g_mock.released
                ? ZZ9K_AUDIO_FABRIC_SLOT_FREE
                : (g_mock.written != 0U ? ZZ9K_AUDIO_FABRIC_SLOT_ACTIVE
                                        : ZZ9K_AUDIO_FABRIC_SLOT_LEASED);
    reply = mock_complete(req, ZZ9K_STATUS_OK,
                          sizeof(ZZ9KAudioFabricStateResultPayload));
    mock_word(reply, 0, mock_req_word(req, 0)); /* slot echo */
    mock_word(reply, 1, 2U);                    /* generation */
    mock_word(reply, 2, 3U);                    /* slot count */
    mock_word(reply, 3, state);
    mock_word(reply, 4, g_mock.released
                            ? ZZ9K_AUDIO_METER_IDENTITY_UNKNOWN
                            : ZZ9K_AUDIO_METER_IDENTITY_SDK_STREAM);
    mock_word(reply, 5, g_mock.released ? ZZ9K_INVALID_HANDLE
                                        : MOCK_LEASE_HANDLE);
    mock_word(reply, 6, g_mock.written);
    mock_word(reply, 7, g_mock.read);
    mock_word(reply, 8, 0U);  /* underruns */
    mock_word(reply, 9, 0U);  /* flags */
    mock_word(reply, 10, 24000U); /* peak: 16.16 of amplitude 12000 */
    mock_word(reply, 11, 0U);     /* clips */
    break;
  }
  case ZZ9K_OP_FREE_SHARED:
    g_mock.free_calls++;
    (void)mock_complete(req, ZZ9K_STATUS_OK, 0U);
    break;
  default:
    (void)mock_complete(req, ZZ9K_STATUS_UNSUPPORTED, 0U);
    break;
  }
}

/* ---- pure builder/validation layer ---- */

static int test_builders(void)
{
  ZZ9KAudioLeaseBeginDesc begin;
  ZZ9KAudioLeaseSubmitDesc submit;
  ZZ9KAudioFabricStateDesc state;

  if (zz9k_audio_build_lease_begin_desc(&begin, 1U,
                                        ZZ9K_AUDIO_METER_IDENTITY_SDK_STREAM,
                                        200U, 0U) != 1 ||
      begin.slot != 1U ||
      begin.identity != ZZ9K_AUDIO_METER_IDENTITY_SDK_STREAM ||
      begin.gain != 200U || begin.flags != 0U) {
    return 1;
  }
  if (zz9k_audio_build_lease_begin_desc(&begin, 0U, 0U, 128U, 0U) ||
      zz9k_audio_build_lease_begin_desc(&begin, 3U, 0U, 128U, 0U) ||
      zz9k_audio_build_lease_begin_desc(&begin, 1U, 4U, 128U, 0U) ||
      zz9k_audio_build_lease_begin_desc(&begin, 1U, 0U, 256U, 0U) ||
      zz9k_audio_build_lease_begin_desc(&begin, 1U, 0U, 128U, 1U)) {
    return 2;
  }
  if (zz9k_audio_build_lease_submit_desc(&submit, 0x21U, 9U, 0U, 4096U,
                                         0U) != 1 ||
      submit.lease != 0x21U || submit.src_handle != 9U ||
      submit.src_length != 4096U) {
    return 3;
  }
  if (zz9k_audio_build_lease_submit_desc(&submit, 0U, 9U, 0U, 4096U, 0U) ||
      zz9k_audio_build_lease_submit_desc(&submit, 0xffffffffU, 9U, 0U,
                                         4096U, 0U) ||
      zz9k_audio_build_lease_submit_desc(&submit, 0x21U,
                                         ZZ9K_INVALID_HANDLE, 0U, 4096U,
                                         0U) ||
      zz9k_audio_build_lease_submit_desc(&submit, 0x21U, 9U, 0U, 0U, 0U) ||
      zz9k_audio_build_lease_submit_desc(&submit, 0x21U, 9U, 0U, 5U, 0U) ||
      zz9k_audio_build_lease_submit_desc(&submit, 0x21U, 9U, 0U, 4096U,
                                         1U)) {
    return 4;
  }
  if (zz9k_audio_build_fabric_state_desc(&state, 1U,
                                         ZZ9K_AUDIO_FABRIC_STATE_HOLD_RESET) !=
          1 ||
      state.slot != 1U || state.flags != ZZ9K_AUDIO_FABRIC_STATE_HOLD_RESET) {
    return 5;
  }
  if (zz9k_audio_build_fabric_state_desc(&state, 3U, 0U) ||
      zz9k_audio_build_fabric_state_desc(&state, 1U, 2U)) {
    return 6;
  }
  return 0;
}

/* ---- request packing ---- */

static int test_request_helpers(void)
{
  ZZ9KAudioLeaseBeginDesc begin;
  ZZ9KRequest request;

  if (!zz9k_audio_build_lease_begin_desc(&begin, 1U,
                                         ZZ9K_AUDIO_METER_IDENTITY_MEDIA,
                                         255U, 0U) ||
      zz9k_request_audio_lease_begin(&request, &begin) != ZZ9K_STATUS_OK ||
      request.entry.opcode != ZZ9K_OP_AUDIO_LEASE_BEGIN ||
      request.entry.payload_len != 48U ||
      zz9k_get_be32(&request.entry.payload.inline_data[0]) != 1U ||
      zz9k_get_be32(&request.entry.payload.inline_data[4]) !=
          ZZ9K_AUDIO_METER_IDENTITY_MEDIA ||
      zz9k_get_be32(&request.entry.payload.inline_data[8]) != 255U ||
      zz9k_get_be32(&request.entry.payload.inline_data[12]) != 0U) {
    return 1;
  }
  if (zz9k_request_audio_lease_begin(&request, 0) !=
      ZZ9K_STATUS_BAD_REQUEST) {
    return 2;
  }
  if (zz9k_request_audio_lease_release(&request, 0x21U, 0U) !=
          ZZ9K_STATUS_OK ||
      request.entry.opcode != ZZ9K_OP_AUDIO_LEASE_RELEASE ||
      request.entry.payload_len != 48U ||
      zz9k_get_be32(&request.entry.payload.inline_data[0]) != 0x21U) {
    return 3;
  }
  if (zz9k_request_audio_lease_release(&request, 0xffffffffU, 0U) ==
      ZZ9K_STATUS_OK) {
    return 4;
  }
  return 0;
}

/* ---- reply decoding ---- */

static void put_begin_reply(ZZ9KMailboxEntry *reply, uint32_t lease,
                            uint32_t slot, uint32_t generation,
                            uint32_t flags, uint32_t gain)
{
  memset(reply, 0, sizeof(*reply));
  reply->opcode = ZZ9K_OP_AUDIO_LEASE_BEGIN;
  reply->status = ZZ9K_STATUS_OK;
  reply->payload_len = sizeof(ZZ9KAudioLeaseBeginResultPayload);
  zz9k_put_be32(&reply->payload.inline_data[0], lease);
  zz9k_put_be32(&reply->payload.inline_data[4], slot);
  zz9k_put_be32(&reply->payload.inline_data[8], generation);
  zz9k_put_be32(&reply->payload.inline_data[12], flags);
  zz9k_put_be32(&reply->payload.inline_data[16], gain);
}

static int test_reply_helpers(void)
{
  ZZ9KAudioLeaseBeginResult begin;
  ZZ9KAudioFabricStateResult state;
  ZZ9KMailboxEntry reply;

  put_begin_reply(&reply, 0x21U, 1U, 2U,
                  ZZ9K_AUDIO_LEASE_RESULT_GAIN_BOUNDED, 128U);
  if (zz9k_reply_audio_lease_begin_result(&reply, &begin) !=
          ZZ9K_STATUS_OK ||
      begin.lease != 0x21U || begin.slot != 1U ||
      begin.generation != 2U || begin.gain_applied != 128U ||
      (begin.flags & ZZ9K_AUDIO_LEASE_RESULT_GAIN_BOUNDED) == 0U) {
    return 1;
  }
  /* Nonsense handles, slot/handle disagreement and out-of-range
   * applied gains fail closed. */
  put_begin_reply(&reply, 0x22U, 1U, 2U, 0U, 128U);
  if (zz9k_reply_audio_lease_begin_result(&reply, &begin) ==
      ZZ9K_STATUS_OK) {
    return 2;
  }
  put_begin_reply(&reply, 0x21U, 1U, 2U, 0U, 256U);
  if (zz9k_reply_audio_lease_begin_result(&reply, &begin) ==
      ZZ9K_STATUS_OK) {
    return 3;
  }

  memset(&reply, 0, sizeof(reply));
  reply.opcode = ZZ9K_OP_AUDIO_FABRIC_STATE_GET;
  reply.status = ZZ9K_STATUS_OK;
  reply.payload_len = sizeof(ZZ9KAudioFabricStateResultPayload);
  zz9k_put_be32(&reply.payload.inline_data[0], 1U);   /* slot */
  zz9k_put_be32(&reply.payload.inline_data[8], 3U);   /* slot count */
  zz9k_put_be32(&reply.payload.inline_data[12],
                ZZ9K_AUDIO_FABRIC_SLOT_ACTIVE);
  zz9k_put_be32(&reply.payload.inline_data[20], 0x21U);
  zz9k_put_be32(&reply.payload.inline_data[32], 3U);  /* underruns */
  if (zz9k_reply_audio_fabric_state_result(&reply, &state) !=
          ZZ9K_STATUS_OK ||
      state.slot != 1U || state.slot_count != 3U ||
      state.state != ZZ9K_AUDIO_FABRIC_SLOT_ACTIVE ||
      state.lease != 0x21U || state.underrun_count != 3U) {
    return 4;
  }
  /* A live handle on a FREE slot is impossible. */
  zz9k_put_be32(&reply.payload.inline_data[12],
                ZZ9K_AUDIO_FABRIC_SLOT_FREE);
  if (zz9k_reply_audio_fabric_state_result(&reply, &state) ==
      ZZ9K_STATUS_OK) {
    return 5;
  }
  /* The pump slot keeps the invalid handle even while LEASED. */
  zz9k_put_be32(&reply.payload.inline_data[0], 0U);
  zz9k_put_be32(&reply.payload.inline_data[12],
                ZZ9K_AUDIO_FABRIC_SLOT_LEASED);
  zz9k_put_be32(&reply.payload.inline_data[20], ZZ9K_INVALID_HANDLE);
  if (zz9k_reply_audio_fabric_state_result(&reply, &state) !=
      ZZ9K_STATUS_OK) {
    return 6;
  }
  return 0;
}

/* ---- the two documented decline paths ---- */

static int test_gate(void)
{
  ZZ9KBoard z2;
  ZZ9KBoard z3;
  ZZ9KBoard unknown;

  memset(&z2, 0, sizeof(z2));
  z2.zorro_version = 2U;
  memset(&z3, 0, sizeof(z3));
  z3.zorro_version = 3U;
  memset(&unknown, 0, sizeof(unknown));

  /* Zorro 2 declines even when the firmware advertises the fabric. */
  if (zz9k_fabriclease_gate(&z2, ZZ9K_CAP_AUDIO_FABRIC) !=
      ZZ9K_FABRICLEASE_DECLINE_ZORRO2) {
    return 1;
  }
  /* Firmware without the capability declines cleanly (R13/AE5). */
  if (zz9k_fabriclease_gate(&z3, 0U) !=
      ZZ9K_FABRICLEASE_DECLINE_NO_FABRIC) {
    return 2;
  }
  if (zz9k_fabriclease_gate(&unknown, 0U) !=
      ZZ9K_FABRICLEASE_DECLINE_NO_FABRIC) {
    return 3;
  }
  if (zz9k_fabriclease_gate(&z3, ZZ9K_CAP_AUDIO_FABRIC) !=
      ZZ9K_FABRICLEASE_OK) {
    return 4;
  }
  return 0;
}

static int cancel_after_lease_begin(void *user)
{
  (void)user;
  return g_mock.begin_calls != 0;
}

static int test_staging_horizon(void)
{
  /* The real Amiga must perform tone generation plus two mailbox calls
   * before the next submit. One 20-ms period leaves no practical margin;
   * require at least four complete periods before a lease goes live. */
  return ZZ9K_FABRICLEASE_STAGING_BYTES >= (4U * 3840U) ? 0 : 1;
}

/* ---- the full begin/submit/poll/release cycle against the mock ---- */

static int run_session(uint32_t caps, uint16_t zorro, uint32_t seconds,
                       uint32_t gain, uint32_t slot, int corrupt_begin,
                       int *session_status)
{
  ZZ9KContext *ctx = 0;
  ZZ9KFabricLeaseOptions options;
  int status;

  mock_reset(caps, zorro);
  /* Scripted fabric behavior survives the reset: one BUSY retry on
   * the first submit, and optionally a malformed begin reply. */
  g_mock.busy_once = 1;
  g_mock.corrupt_begin = corrupt_begin;
  zz9k_set_idle_hook_for_test(mock_respond);
  status = zz9k_attach_mailbox(&ctx, &g_mock.board,
                               &g_mock.mailbox.descriptor, 0, 0);
  if (status != ZZ9K_STATUS_OK) {
    zz9k_set_idle_hook_for_test(0);
    return -1;
  }
  memset(&options, 0, sizeof(options));
  options.seconds = seconds;
  options.gain = gain;
  options.slot = slot;
  *session_status = zz9k_fabriclease_session(ctx, &options);
  zz9k_close(ctx);
  zz9k_set_idle_hook_for_test(0);
  return 0;
}

static int test_session_walkthrough(void)
{
  volatile int16_t *window;
  void *mapping = mock_board_window();
  int status = 0;
  int rc;

  if (!mapping) {
    return 1;
  }
  rc = run_session(ZZ9K_CAP_AUDIO_FABRIC, 3U, 0U, 200U, 1U, 0, &status);
  if (rc != 0) {
    return 2;
  }
  if (status != ZZ9K_STATUS_OK) {
    return 3;
  }

  /* Exactly one lifecycle: staging alloc/free, one BEGIN, one
   * RELEASE, and the feed loop retried once through BUSY. */
  if (g_mock.alloc_calls != 1 || g_mock.free_calls != 1 ||
      g_mock.begin_calls != 1 || g_mock.release_calls != 1 ||
      g_mock.busy_submits != 1) {
    return 4;
  }
  /* BEGIN carried the client's request verbatim. */
  if (g_mock.begin_slot != 1U ||
      g_mock.begin_identity != ZZ9K_AUDIO_METER_IDENTITY_SDK_STREAM ||
      g_mock.begin_gain != 200U) {
    return 5;
  }
  /* Submits referenced the staging buffer by handle, whole frames
   * only, bounded by the staging chunk. */
  if (g_mock.submit_handle != 9U || g_mock.submit_lease != MOCK_LEASE_HANDLE ||
      g_mock.submit_length_bad || g_mock.submit_length_max == 0U ||
      g_mock.submit_length_max > ZZ9K_FABRICLEASE_STAGING_BYTES ||
      (g_mock.submit_length_max & 3U) != 0U) {
    return 6;
  }
  /* State was polled across keep-ahead, drain and the final report. */
  if (g_mock.state_calls < 3) {
    return 7;
  }

  /* The tone really landed in the HOST_WINDOW staging mapping: the
   * first frame is the burst pattern's block 0 (-12000 both
   * channels), the last of the 1024-frame chunk is block 21 (+12000,
   * odd), little-endian. */
  window = (volatile int16_t *)(void *)((uintptr_t)MOCK_BOARD_ADDR +
                                        0x10000UL);
  if (window[0] != -12000 || window[1] != -12000 ||
      window[2044] != 12000 || window[2045] != 12000) {
    return 8;
  }
  mock_board_window_free(mapping);
  return 0;
}

static int test_session_rejects_malformed_begin(void)
{
  int status = 0;
  int rc;

  rc = run_session(ZZ9K_CAP_AUDIO_FABRIC, 3U, 0U, 200U, 1U, 1, &status);
  if (rc != 0) {
    return 1;
  }
  /* The decoder fails closed on the out-of-range applied gain, the
   * session never releases a lease it never got, and the staging
   * allocation is still freed. */
  if (status == ZZ9K_STATUS_OK) {
    return 2;
  }
  if (g_mock.release_calls != 0 || g_mock.free_calls != 1) {
    return 3;
  }
  return 0;
}

static int test_session_slot2(void)
{
  void *mapping = mock_board_window();
  int status = 0;
  int rc;

  if (!mapping)
    return 1;
  rc = run_session(ZZ9K_CAP_AUDIO_FABRIC, 3U, 0U, 128U, 2U, 0,
                   &status);
  if (rc != 0 || status != ZZ9K_STATUS_OK) {
    mock_board_window_free(mapping);
    return 2;
  }
  if (g_mock.begin_slot != 2U || g_mock.submit_lease != 0x22U ||
      g_mock.release_calls != 1) {
    mock_board_window_free(mapping);
    return 3;
  }
  mock_board_window_free(mapping);
  return 0;
}

static int test_session_multichunk_keepahead(void)
{
  void *mapping = mock_board_window();
  int status = 0;
  int rc;

  if (!mapping)
    return 1;
  rc = run_session(ZZ9K_CAP_AUDIO_FABRIC, 3U, 1U, 128U, 1U, 0,
                   &status);
  if (rc != 0 || status != ZZ9K_STATUS_OK) {
    mock_board_window_free(mapping);
    return 2;
  }
  if (g_mock.submit_calls < 12 || g_mock.busy_submits != 1 ||
      g_mock.written != ZZ9K_FABRICLEASE_BYTES_PER_SECOND ||
      g_mock.read != g_mock.written || g_mock.release_calls != 1) {
    mock_board_window_free(mapping);
    return 3;
  }
  mock_board_window_free(mapping);
  return 0;
}

static int test_ctrl_c_releases_lease(void)
{
  void *mapping = mock_board_window();
  int status = 0;
  int rc;

  if (!mapping)
    return 1;
  zz9k_fabriclease_set_cancel_hook_for_test(cancel_after_lease_begin, 0);
  rc = run_session(ZZ9K_CAP_AUDIO_FABRIC, 3U, 60U, 128U, 1U, 0,
                   &status);
  zz9k_fabriclease_set_cancel_hook_for_test(0, 0);
  if (rc != 0 || status != ZZ9K_STATUS_CANCELLED) {
    mock_board_window_free(mapping);
    return 2;
  }
  if (g_mock.release_calls != 1 || g_mock.free_calls != 1) {
    mock_board_window_free(mapping);
    return 3;
  }
  mock_board_window_free(mapping);
  return 0;
}

int main(void)
{
  int r;

  printf("fabriclease_contract_test: builder/dispatcher/decline checks\n");
  r = test_staging_horizon();
  CHECK(r == 0, "staging covers at least four TX periods");

  r = test_builders();
  CHECK(r == 0, "builders");
  r = test_request_helpers();
  CHECK(r == 0, "request helpers");
  r = test_reply_helpers();
  CHECK(r == 0, "reply helpers");
  r = test_gate();
  CHECK(r == 0, "gate declines");
  r = test_session_walkthrough();
  CHECK(r == 0, "session walkthrough");
  r = test_session_rejects_malformed_begin();
  CHECK(r == 0, "session fails closed on malformed begin");
  r = test_session_slot2();
  CHECK(r == 0, "slot 2 session for B4");
  r = test_session_multichunk_keepahead();
  CHECK(r == 0, "one-second multi-chunk keep-ahead and drain");

  r = test_ctrl_c_releases_lease();
  CHECK(r == 0, "Ctrl-C cancellation releases lease and staging");
  if (g_failures != 0) {
    printf("fabriclease_contract_test: %d failure(s)\n", g_failures);
    return 1;
  }
  printf("fabriclease_contract_test: all checks passed\n");
  return 0;
}
