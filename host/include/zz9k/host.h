/*
 * ZZ9000 SDK v2 Amiga-side host API.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZ9K_HOST_H
#define ZZ9K_HOST_H

#include <stdint.h>
#include "zz9k/abi.h"
#include "zz9k/crypto.h"
#include "zz9k/text.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ZZ9KContext ZZ9KContext;

typedef struct ZZ9KBoard {
  uint32_t board_addr;
  uint32_t board_size;
  uint16_t product;
  uint16_t zorro_version;
  uint32_t firmware_version;
} ZZ9KBoard;

typedef struct ZZ9KRequest {
  ZZ9KMailboxEntry entry;
} ZZ9KRequest;

typedef struct ZZ9KSharedBuffer {
  uint32_t handle;
  volatile void *data;
  uint32_t length;
} ZZ9KSharedBuffer;

typedef struct ZZ9KSurface {
  uint32_t handle;
  uint32_t arm_addr;
  volatile void *data;
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t format;
  uint32_t flags;
  uint32_t length;
} ZZ9KSurface;

typedef struct ZZ9KDiagInfo {
  uint32_t requests_completed;
  uint32_t requests_failed;
  uint32_t last_status;
  uint32_t pending_requests;
  uint32_t shared_buffers_used;
  uint32_t shared_heap_total;
  uint32_t shared_heap_free;
  uint32_t shared_heap_largest_free;
  uint32_t mailbox_arm_addr;
  uint32_t mailbox_ring_entries;
  uint32_t surfaces_used;
  uint32_t allocator_invalid_slots;
} ZZ9KDiagInfo;

typedef struct ZZ9KDiagTimingInfo {
  uint32_t version;
  uint32_t timer_hz;
  uint32_t requests_timed;
  uint32_t total_us;
  uint32_t surface_requests;
  uint32_t surface_us;
  uint32_t audio_requests;
  uint32_t audio_us;
  uint32_t last_opcode;
  uint32_t last_us;
  uint32_t max_opcode;
  uint32_t max_us;
} ZZ9KDiagTimingInfo;

typedef struct ZZ9KDiagSchedInfo {
  uint32_t version;
  uint32_t core1_online;
  uint32_t tasks_on_core1;
  uint32_t tasks_on_core0;
} ZZ9KDiagSchedInfo;

typedef struct ZZ9KServiceInfo {
  uint32_t service_id;
  uint32_t version;
  uint32_t capability_bits;
  uint32_t flags;
  uint32_t opcode_base;
  uint32_t opcode_count;
  uint32_t max_inline_payload;
  char name[21];
} ZZ9KServiceInfo;

#define ZZ9K_DEFAULT_TIMEOUT_TICKS 250UL
#define ZZ9K_CRYPTO_BATCH_MAX_IN_FLIGHT 16U

const char *zz9k_status_name(int status);
const char *zz9k_service_name(uint32_t service_id);
int zz9k_service_advertised_by_caps(uint32_t service_id,
                                    uint32_t capability_bits);

int zz9k_find_board(ZZ9KBoard *board);
int zz9k_open(ZZ9KContext **ctx);
int zz9k_attach_mailbox(ZZ9KContext **ctx, const ZZ9KBoard *board,
                        volatile ZZ9KMailboxDescriptor *mailbox,
                        volatile uint16_t *doorbell,
                        volatile uint16_t *irq_ack);
void zz9k_close(ZZ9KContext *ctx);
int zz9k_query_caps(ZZ9KContext *ctx, ZZ9KCaps *caps);
int zz9k_query_service(ZZ9KContext *ctx, uint32_t service_id,
                       ZZ9KServiceInfo *service);
int zz9k_ping(ZZ9KContext *ctx, const uint8_t *payload,
              uint32_t payload_len, uint8_t *reply_payload,
              uint32_t *reply_len);
int zz9k_alloc_shared(ZZ9KContext *ctx, uint32_t length, uint32_t alignment,
                      uint32_t flags, ZZ9KSharedBuffer *buffer);
int zz9k_free_shared(ZZ9KContext *ctx, uint32_t handle);
int zz9k_mem_fill(ZZ9KContext *ctx, uint32_t handle, uint32_t offset,
                  uint32_t length, uint8_t value);
int zz9k_mem_copy(ZZ9KContext *ctx, uint32_t dst_handle, uint32_t dst_offset,
                  uint32_t src_handle, uint32_t src_offset, uint32_t length);
int zz9k_alloc_surface(ZZ9KContext *ctx, uint32_t width, uint32_t height,
                       uint32_t format, uint32_t flags, ZZ9KSurface *surface);
int zz9k_alloc_surface_ex(ZZ9KContext *ctx, uint32_t width, uint32_t height,
                          uint32_t format, uint32_t flags, uint32_t pitch,
                          ZZ9KSurface *surface);
int zz9k_free_surface(ZZ9KContext *ctx, uint32_t handle);
int zz9k_map_framebuffer_surface(ZZ9KContext *ctx, ZZ9KSurface *surface);
int zz9k_scale_image(ZZ9KContext *ctx, const ZZ9KScaleImageDesc *desc);
int zz9k_scale_image_clipped(ZZ9KContext *ctx,
                             const ZZ9KScaleImageClippedDesc *desc);
int zz9k_fill_surface(ZZ9KContext *ctx, const ZZ9KSurfaceFillDesc *desc);
int zz9k_copy_surface(ZZ9KContext *ctx, const ZZ9KSurfaceCopyDesc *desc);
int zz9k_decode_image(ZZ9KContext *ctx, uint32_t opcode,
                      const ZZ9KImageDecodeDesc *desc,
                      ZZ9KImageDecodeResult *result);
int zz9k_decode_jpeg(ZZ9KContext *ctx, const ZZ9KImageDecodeDesc *desc,
                     ZZ9KImageDecodeResult *result);
int zz9k_decode_png(ZZ9KContext *ctx, const ZZ9KImageDecodeDesc *desc,
                    ZZ9KImageDecodeResult *result);
int zz9k_decode_gif(ZZ9KContext *ctx, const ZZ9KImageDecodeDesc *desc,
                    ZZ9KImageDecodeResult *result);
int zz9k_decode_mp3(ZZ9KContext *ctx, const ZZ9KAudioDecodeDesc *desc,
                    ZZ9KAudioDecodeResult *result);
int zz9k_audio_stream_begin(ZZ9KContext *ctx,
                            const ZZ9KAudioStreamBeginDesc *desc,
                            ZZ9KAudioStreamResult *result);
int zz9k_audio_stream_feed(ZZ9KContext *ctx,
                           const ZZ9KAudioStreamFeedDesc *desc,
                           ZZ9KAudioStreamResult *result);
int zz9k_audio_stream_read(ZZ9KContext *ctx, uint32_t session,
                           uint32_t pcm_read, uint32_t flags,
                           ZZ9KAudioStreamResult *result);
int zz9k_audio_stream_close(ZZ9KContext *ctx, uint32_t session,
                            uint32_t flags,
                            ZZ9KAudioStreamResult *result);
int zz9k_image_session_begin(ZZ9KContext *ctx,
                             const ZZ9KImageSessionBeginDesc *desc,
                             ZZ9KImageSessionResult *result);
int zz9k_image_session_feed(ZZ9KContext *ctx,
                            const ZZ9KImageSessionFeedDesc *desc,
                            ZZ9KImageSessionResult *result);
int zz9k_image_session_close(ZZ9KContext *ctx, uint32_t session,
                             uint32_t flags);
int zz9k_crypto_hash(ZZ9KContext *ctx, const ZZ9KCryptoHashDesc *desc,
                     ZZ9KCryptoResult *result);
int zz9k_crypto_hash_batch(ZZ9KContext *ctx,
                           const ZZ9KCryptoHashDesc *descs,
                           ZZ9KCryptoResult *results,
                           uint32_t count,
                           uint32_t max_in_flight,
                           uint32_t timeout_ticks);
int zz9k_crypto_stream(ZZ9KContext *ctx,
                       const ZZ9KCryptoStreamDesc *desc,
                       ZZ9KCryptoResult *result);
int zz9k_crypto_stream_batch(ZZ9KContext *ctx,
                             const ZZ9KCryptoStreamDesc *descs,
                             ZZ9KCryptoResult *results,
                             uint32_t count,
                             uint32_t max_in_flight,
                             uint32_t timeout_ticks);
int zz9k_crypto_aead(ZZ9KContext *ctx, const ZZ9KCryptoAeadDesc *desc,
                     ZZ9KCryptoResult *result);
int zz9k_crypto_kx(ZZ9KContext *ctx, const ZZ9KCryptoKxDesc *desc,
                   ZZ9KCryptoResult *result);
int zz9k_crypto_verify(ZZ9KContext *ctx, const ZZ9KCryptoVerifyDesc *desc,
                       int *valid);
int zz9k_crypto_aead_batch(ZZ9KContext *ctx,
                           const ZZ9KCryptoAeadDesc *descs,
                           ZZ9KCryptoResult *results,
                           uint32_t count,
                           uint32_t max_in_flight,
                           uint32_t timeout_ticks);
int zz9k_decompress(ZZ9KContext *ctx, const ZZ9KDecompressDesc *desc,
                    ZZ9KDecompressResult *result);
int zz9k_decompress_test(ZZ9KContext *ctx,
                         const ZZ9KDecompressTestDesc *desc,
                         ZZ9KDecompressResult *result);
int zz9k_decompress_stream_begin(
    ZZ9KContext *ctx,
    const ZZ9KDecompressStreamBeginDesc *desc,
    ZZ9KDecompressStreamResult *result);
int zz9k_decompress_stream_feed(
    ZZ9KContext *ctx,
    const ZZ9KDecompressStreamFeedDesc *desc,
    ZZ9KDecompressStreamResult *result);
int zz9k_decompress_stream_read(
    ZZ9KContext *ctx,
    const ZZ9KDecompressStreamReadDesc *desc,
    ZZ9KDecompressStreamResult *result);
int zz9k_decompress_stream_close(ZZ9KContext *ctx, uint32_t session,
                                 uint32_t flags);
int zz9k_read_diag(ZZ9KContext *ctx, ZZ9KDiagInfo *diag);
int zz9k_read_diag_timing(ZZ9KContext *ctx, ZZ9KDiagTimingInfo *timing);
int zz9k_read_diag_sched(ZZ9KContext *ctx, ZZ9KDiagSchedInfo *sched);
int zz9k_completion_irq_supported(ZZ9KContext *ctx);
int zz9k_completion_irq_enable(ZZ9KContext *ctx, int enable);
int zz9k_completion_irq_ack(ZZ9KContext *ctx);
int zz9k_interrupt_status(ZZ9KContext *ctx, uint16_t *status);
int zz9k_submit(ZZ9KContext *ctx, ZZ9KRequest *request,
                uint32_t *request_id);
int zz9k_submit_batch(ZZ9KContext *ctx, ZZ9KRequest *requests,
                      uint32_t count, uint32_t *request_ids,
                      uint32_t *submitted);
int zz9k_poll(ZZ9KContext *ctx, ZZ9KMailboxEntry *reply);
int zz9k_poll_batch(ZZ9KContext *ctx, ZZ9KMailboxEntry *replies,
                    uint32_t capacity, uint32_t *completed);
int zz9k_call(ZZ9KContext *ctx, ZZ9KRequest *request, ZZ9KMailboxEntry *reply,
              uint32_t timeout_ticks);

#ifdef __cplusplus
}
#endif

#endif /* ZZ9K_HOST_H */
