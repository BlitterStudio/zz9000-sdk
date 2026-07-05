/*
 * ZZ9000 SDK v2 shared ABI definitions.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZ9K_ABI_H
#define ZZ9K_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZZ9K_ABI_MAGIC              0x5a5a394bUL /* "ZZ9K" */
#define ZZ9K_ABI_VERSION_MAJOR      2U
#define ZZ9K_ABI_VERSION_MINOR      0U

#define ZZ9K_MAILBOX_ENTRY_SIZE     64U
#define ZZ9K_MAILBOX_DESCRIPTOR_SIZE 128U
#define ZZ9K_INVALID_HANDLE         0xffffffffUL
#define ZZ9K_SURFACE_HANDLE_FRAMEBUFFER 0x80000000UL

#define ZZ9K_MNT_MANUFACTURER       0x6d6eU
#define ZZ9K_PRODUCT_Z2             3U
#define ZZ9K_PRODUCT_Z3             4U

#define ZZ9K_ARM_MEMORY_START       0x00200000UL
#define ZZ9K_AMIGA_MEMORY_OFFSET    0x00010000UL
#define ZZ9K_AMIGA_MEMORY_LIMIT     0x10000000UL
#define ZZ9K_ARM_MEMORY_VISIBLE_END \
  (ZZ9K_ARM_MEMORY_START + (ZZ9K_AMIGA_MEMORY_LIMIT - ZZ9K_AMIGA_MEMORY_OFFSET))

/*
 * Legacy firmware-serviced board window. The current ZZ9000 firmware already
 * maps board offsets 0xa000..0xffff to this ARM buffer for SD/USB proxy I/O.
 * SDK v2 keeps its bootstrap mailbox inside the upper part of that window so
 * early discovery does not depend on generic DDR reads from the Amiga side.
 */
#define ZZ9K_MAPPED_IO_ARM_START    0x3FE40000UL
#define ZZ9K_MAPPED_IO_BOARD_OFFSET 0x0000A000UL
#define ZZ9K_MAPPED_IO_WINDOW_SIZE  0x00006000UL
#define ZZ9K_SDK_MAILBOX_BOARD_OFFSET 0x0000D000UL
#define ZZ9K_SDK_MAILBOX_ARM_ADDRESS \
  (ZZ9K_MAPPED_IO_ARM_START + \
   (ZZ9K_SDK_MAILBOX_BOARD_OFFSET - ZZ9K_MAPPED_IO_BOARD_OFFSET))

/*
 * SDK v2 bootstrap registers. Reads use the low board offsets below so older
 * firmware can expose the mailbox through the ARM-serviced request path.
 * Write doorbells must use the Z3 HDL register aperture when advertised.
 */
#define ZZ9K_Z3_REGISTER_WINDOW_OFFSET 0x00001000U
#define ZZ9K_REG_CONFIG             0x0004U
#define ZZ9K_REG_SDK_MAGIC          0x0100U
#define ZZ9K_REG_SDK_VERSION        0x0102U
#define ZZ9K_REG_SDK_MAILBOX_HI     0x0104U
#define ZZ9K_REG_SDK_MAILBOX_LO     0x0106U
#define ZZ9K_REG_SDK_DOORBELL       0x0108U
#define ZZ9K_REG_SDK_STATUS         0x010aU
#define ZZ9K_REG_SDK_IRQ_ACK        0x010cU
#define ZZ9K_REG_SDK_DIAG_WRITE     0x0110U
#define ZZ9K_REG_SDK_DIAG_DATA      0x0114U
#define ZZ9K_REG_SDK_DIAG_Z3ADDR    0x0118U
#define ZZ9K_REG_SDK_MAGIC_VALUE    0x5a39U

#define ZZ9K_INTERRUPT_ETH          0x0001U
#define ZZ9K_INTERRUPT_AUDIO        0x0002U
#define ZZ9K_INTERRUPT_VBLANK       0x0004U
#define ZZ9K_INTERRUPT_SDK          0x0008U
#define ZZ9K_CONFIG_ACK_MODE        0x0008U
#define ZZ9K_CONFIG_ACK_ETH         0x0010U
#define ZZ9K_CONFIG_ACK_AUDIO       0x0020U
#define ZZ9K_CONFIG_ACK_VBLANK      0x0040U
#define ZZ9K_CONFIG_ACK_SDK         0x0080U

#define ZZ9K_SDK_IRQ_ACK_VALUE      0x0001U
#define ZZ9K_SDK_IRQ_ENABLE_VALUE   0x0002U
#define ZZ9K_SDK_IRQ_DISABLE_VALUE  0x0004U

enum ZZ9KStatus {
  ZZ9K_STATUS_OK = 0,
  ZZ9K_STATUS_QUEUED = 1,
  ZZ9K_STATUS_BUSY = 2,
  ZZ9K_STATUS_UNSUPPORTED = 3,
  ZZ9K_STATUS_BAD_REQUEST = 4,
  ZZ9K_STATUS_BAD_HANDLE = 5,
  ZZ9K_STATUS_NO_MEMORY = 6,
  ZZ9K_STATUS_TIMEOUT = 7,
  ZZ9K_STATUS_CANCELLED = 8,
  ZZ9K_STATUS_IO_ERROR = 9,
  ZZ9K_STATUS_NOT_FOUND = 10,
  ZZ9K_STATUS_INTERNAL_ERROR = 0xffff
};

enum ZZ9KEntryFlags {
  ZZ9K_ENTRY_INLINE_PAYLOAD = 1U << 0,
  ZZ9K_ENTRY_BUFFER_PAYLOAD = 1U << 1,
  ZZ9K_ENTRY_ASYNC = 1U << 2,
  ZZ9K_ENTRY_NEEDS_IRQ = 1U << 3,
  ZZ9K_ENTRY_CANCEL_REQUEST = 1U << 4
};

enum ZZ9KService {
  ZZ9K_SERVICE_CORE = 0x0000,
  ZZ9K_SERVICE_MEMORY = 0x0100,
  ZZ9K_SERVICE_SURFACE = 0x0200,
  ZZ9K_SERVICE_GFX = 0x0300,
  ZZ9K_SERVICE_IMAGE = 0x0400,
  ZZ9K_SERVICE_AUDIO = 0x0500,
  ZZ9K_SERVICE_CODEC = 0x0600,
  ZZ9K_SERVICE_STORAGE = 0x0700,
  ZZ9K_SERVICE_CRYPTO = 0x0800,
  ZZ9K_SERVICE_DIAG = 0x0900,
  ZZ9K_SERVICE_MODULE = 0x0a00,
  ZZ9K_SERVICE_VENDOR = 0x8000
};

enum ZZ9KOpcode {
  ZZ9K_OP_NOP = ZZ9K_SERVICE_CORE + 0x00,
  ZZ9K_OP_QUERY_CAPS = ZZ9K_SERVICE_CORE + 0x01,
  ZZ9K_OP_PING = ZZ9K_SERVICE_CORE + 0x02,
  ZZ9K_OP_CANCEL = ZZ9K_SERVICE_CORE + 0x03,
  ZZ9K_OP_QUERY_SERVICE = ZZ9K_SERVICE_CORE + 0x04,

  ZZ9K_OP_ALLOC_SHARED = ZZ9K_SERVICE_MEMORY + 0x00,
  ZZ9K_OP_FREE_SHARED = ZZ9K_SERVICE_MEMORY + 0x01,
  ZZ9K_OP_MEM_FILL = ZZ9K_SERVICE_MEMORY + 0x02,
  ZZ9K_OP_MEM_COPY = ZZ9K_SERVICE_MEMORY + 0x03,

  ZZ9K_OP_ALLOC_SURFACE = ZZ9K_SERVICE_SURFACE + 0x00,
  ZZ9K_OP_FREE_SURFACE = ZZ9K_SERVICE_SURFACE + 0x01,
  ZZ9K_OP_MAP_FRAMEBUFFER_SURFACE = ZZ9K_SERVICE_SURFACE + 0x02,
  ZZ9K_OP_FILL_SURFACE = ZZ9K_SERVICE_SURFACE + 0x03,
  ZZ9K_OP_COPY_SURFACE = ZZ9K_SERVICE_SURFACE + 0x04,

  ZZ9K_OP_SCALE_IMAGE = ZZ9K_SERVICE_IMAGE + 0x00,
  ZZ9K_OP_DECODE_JPEG = ZZ9K_SERVICE_IMAGE + 0x01,
  ZZ9K_OP_DECODE_PNG = ZZ9K_SERVICE_IMAGE + 0x02,
  ZZ9K_OP_DECODE_GIF = ZZ9K_SERVICE_IMAGE + 0x03,
  ZZ9K_OP_IMAGE_SESSION_BEGIN = ZZ9K_SERVICE_IMAGE + 0x04,
  ZZ9K_OP_IMAGE_SESSION_FEED = ZZ9K_SERVICE_IMAGE + 0x05,
  ZZ9K_OP_IMAGE_SESSION_CLOSE = ZZ9K_SERVICE_IMAGE + 0x06,
  ZZ9K_OP_SCALE_IMAGE_CLIPPED = ZZ9K_SERVICE_IMAGE + 0x07,

  ZZ9K_OP_DECODE_MP3 = ZZ9K_SERVICE_AUDIO + 0x00,
  ZZ9K_OP_MIX_AUDIO = ZZ9K_SERVICE_AUDIO + 0x01,
  ZZ9K_OP_RESAMPLE_AUDIO = ZZ9K_SERVICE_AUDIO + 0x02,
  ZZ9K_OP_AUDIO_STREAM_BEGIN = ZZ9K_SERVICE_AUDIO + 0x03,
  ZZ9K_OP_AUDIO_STREAM_FEED = ZZ9K_SERVICE_AUDIO + 0x04,
  ZZ9K_OP_AUDIO_STREAM_READ = ZZ9K_SERVICE_AUDIO + 0x05,
  ZZ9K_OP_AUDIO_STREAM_CLOSE = ZZ9K_SERVICE_AUDIO + 0x06,

  ZZ9K_OP_DECOMPRESS = ZZ9K_SERVICE_CODEC + 0x00,
  ZZ9K_OP_DECOMPRESS_TEST = ZZ9K_SERVICE_CODEC + 0x01,
  ZZ9K_OP_DECOMPRESS_STREAM_BEGIN = ZZ9K_SERVICE_CODEC + 0x02,
  ZZ9K_OP_DECOMPRESS_STREAM_READ = ZZ9K_SERVICE_CODEC + 0x03,
  ZZ9K_OP_DECOMPRESS_STREAM_CLOSE = ZZ9K_SERVICE_CODEC + 0x04,
  ZZ9K_OP_DECOMPRESS_STREAM_FEED = ZZ9K_SERVICE_CODEC + 0x05,

  ZZ9K_OP_CRYPTO_HASH = ZZ9K_SERVICE_CRYPTO + 0x00,
  ZZ9K_OP_CRYPTO_STREAM = ZZ9K_SERVICE_CRYPTO + 0x01,
  ZZ9K_OP_CRYPTO_AEAD = ZZ9K_SERVICE_CRYPTO + 0x02,
  ZZ9K_OP_CRYPTO_KX        = ZZ9K_SERVICE_CRYPTO + 0x03,
  ZZ9K_OP_CRYPTO_VERIFY    = ZZ9K_SERVICE_CRYPTO + 0x04,

  ZZ9K_OP_DIAG_READ = ZZ9K_SERVICE_DIAG + 0x00,
  ZZ9K_OP_DIAG_TIMING = ZZ9K_SERVICE_DIAG + 0x01,
  ZZ9K_OP_DIAG_SCHED = ZZ9K_SERVICE_DIAG + 0x02
};

enum ZZ9KCapability {
  ZZ9K_CAP_MAILBOX = 1U << 0,
  ZZ9K_CAP_IRQ_COMPLETION = 1U << 1,
  ZZ9K_CAP_SHARED_ALLOC = 1U << 2,
  ZZ9K_CAP_SURFACES = 1U << 3,
  ZZ9K_CAP_FRAMEBUFFER_SURFACE = 1U << 4,
  ZZ9K_CAP_IMAGE_DECODE = 1U << 5,
  ZZ9K_CAP_IMAGE_SCALE = 1U << 6,
  ZZ9K_CAP_AUDIO_DECODE = 1U << 7,
  ZZ9K_CAP_CRYPTO = 1U << 8,
  ZZ9K_CAP_MODULES = 1U << 9,
  ZZ9K_CAP_MEMORY_OPS = 1U << 10,
  ZZ9K_CAP_DIAGNOSTICS = 1U << 11,
  ZZ9K_CAP_DOORBELL = 1U << 12,
  ZZ9K_CAP_POLLING_COMPLETION = 1U << 13,
  ZZ9K_CAP_SERVICE_DISCOVERY = 1U << 14,
  ZZ9K_CAP_SURFACE_OPS = 1U << 15,
  ZZ9K_CAP_COMPRESSION = 1U << 16,
  ZZ9K_CAP_GFX_OPS = 1U << 17,
  ZZ9K_CAP_STORAGE_OPS = 1U << 18
};

enum ZZ9KServiceFlags {
  ZZ9K_SERVICE_FLAG_FIRMWARE = 1U << 0,
  ZZ9K_SERVICE_FLAG_MODULE = 1U << 1,
  ZZ9K_SERVICE_FLAG_ASYNC = 1U << 2,
  ZZ9K_SERVICE_FLAG_ZERO_COPY = 1U << 3,
  ZZ9K_SERVICE_FLAG_IMAGE_JPEG_BASELINE = 1U << 16,
  ZZ9K_SERVICE_FLAG_IMAGE_JPEG_PROGRESSIVE = 1U << 17,
  ZZ9K_SERVICE_FLAG_IMAGE_JPEG_DIRECT_BGRA = 1U << 18,
  ZZ9K_SERVICE_FLAG_IMAGE_JPEG_SCALING = 1U << 19,
  ZZ9K_SERVICE_FLAG_IMAGE_STREAMING_INPUT = 1U << 20,
  ZZ9K_SERVICE_FLAG_IMAGE_TILE_OUTPUT = 1U << 21,
  ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT = 1U << 22,
  ZZ9K_SERVICE_FLAG_IMAGE_SCALE_BILINEAR = 1U << 23,
  ZZ9K_SERVICE_FLAG_IMAGE_SCALE_CLIPPED = 1U << 24,
  ZZ9K_SERVICE_FLAG_IMAGE_PNG_DIRECT_BGRA = 1U << 25,
  ZZ9K_SERVICE_FLAG_IMAGE_RGB888_OUTPUT = 1U << 26,

  ZZ9K_SERVICE_FLAG_AUDIO_MP3_DECODE = 1U << 16,
  ZZ9K_SERVICE_FLAG_AUDIO_PCM_MIX = 1U << 17,
  ZZ9K_SERVICE_FLAG_AUDIO_RESAMPLE = 1U << 18,
  ZZ9K_SERVICE_FLAG_AUDIO_PCM16_STEREO = 1U << 19,
  ZZ9K_SERVICE_FLAG_AUDIO_MP3_STREAM = 1U << 20,

  ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_RAW = 1U << 16,
  ZZ9K_SERVICE_FLAG_CODEC_ZLIB = 1U << 17,
  ZZ9K_SERVICE_FLAG_CODEC_GZIP = 1U << 18,
  ZZ9K_SERVICE_FLAG_CODEC_LZ4_BLOCK = 1U << 19,
  ZZ9K_SERVICE_FLAG_CODEC_LZMA_ALONE = 1U << 20,
  ZZ9K_SERVICE_FLAG_CODEC_CHECKSUM = 1U << 21,
  ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_TEST = 1U << 22,
  ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_STREAM = 1U << 23,
  ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED = 1U << 24,
  ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_FEED = 1U << 25,
  ZZ9K_SERVICE_FLAG_CODEC_ZLIB_FEED = 1U << 26,
  ZZ9K_SERVICE_FLAG_CODEC_GZIP_FEED = 1U << 27,
  ZZ9K_SERVICE_FLAG_CODEC_LZMA2 = 1U << 28,

  ZZ9K_SERVICE_FLAG_CRYPTO_X25519     = 1U << 16,
  ZZ9K_SERVICE_FLAG_CRYPTO_P256       = 1U << 17,
  ZZ9K_SERVICE_FLAG_CRYPTO_ECDSA_P256 = 1U << 18,
  ZZ9K_SERVICE_FLAG_CRYPTO_RSA_2048   = 1U << 19,
  ZZ9K_SERVICE_FLAG_CRYPTO_AES_GCM    = 1U << 20,
  /* P-256 keygen (scalar*G -> full 65-byte point) via the KX op's KEYGEN flag.
   * Distinct from CRYPTO_P256 (derive only): v2.2.0 advertises derive without
   * keygen, so the provider must gate ECDHE keygen offload on this bit. */
  ZZ9K_SERVICE_FLAG_CRYPTO_P256_KEYGEN = 1U << 21
};

enum ZZ9KAudioSampleFormat {
  ZZ9K_AUDIO_SAMPLE_FORMAT_NONE = 0,
  ZZ9K_AUDIO_SAMPLE_FORMAT_S16LE = 1,
  ZZ9K_AUDIO_SAMPLE_FORMAT_S16BE = 2
};

enum ZZ9KAudioDecodeFlags {
  ZZ9K_AUDIO_DECODE_FLAG_EXPECT_END = 1U << 0
};

enum ZZ9KAudioDecodeResultFlags {
  ZZ9K_AUDIO_DECODE_RESULT_END = 1U << 0
};

typedef struct ZZ9KBufferPayload {
  uint32_t handle;
  uint32_t offset;
  uint32_t length;
  uint32_t aux[9];
} ZZ9KBufferPayload;

/*
 * Inline service payloads are stored as big-endian byte arrays. This keeps
 * the mailbox wire format identical on m68k, ARM, and native test hosts.
 */
typedef struct ZZ9KAllocSharedPayload {
  uint8_t length[4];
  uint8_t alignment[4];
  uint8_t flags[4];
  uint8_t reserved[36];
} ZZ9KAllocSharedPayload;

typedef struct ZZ9KSharedBufferInfoPayload {
  uint8_t handle[4];
  uint8_t arm_addr[4];
  uint8_t length[4];
  uint8_t flags[4];
  uint8_t reserved[32];
} ZZ9KSharedBufferInfoPayload;

typedef struct ZZ9KFreeSharedPayload {
  uint8_t handle[4];
  uint8_t reserved[44];
} ZZ9KFreeSharedPayload;

typedef struct ZZ9KMemFillPayload {
  uint8_t handle[4];
  uint8_t offset[4];
  uint8_t length[4];
  uint8_t value;
  uint8_t reserved[35];
} ZZ9KMemFillPayload;

typedef struct ZZ9KMemCopyPayload {
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t length[4];
  uint8_t flags[4];
  uint8_t reserved[24];
} ZZ9KMemCopyPayload;

typedef struct ZZ9KDiagPayload {
  uint8_t requests_completed[4];
  uint8_t requests_failed[4];
  uint8_t last_status[4];
  uint8_t pending_requests[4];
  uint8_t shared_buffers_used[4];
  uint8_t shared_heap_total[4];
  uint8_t shared_heap_free[4];
  uint8_t shared_heap_largest_free[4];
  uint8_t mailbox_arm_addr[4];
  uint8_t mailbox_ring_entries[4];
  uint8_t surfaces_used[4];
  uint8_t allocator_invalid_slots[4];
} ZZ9KDiagPayload;

typedef struct ZZ9KDiagTimingPayload {
  uint8_t version[4];
  uint8_t timer_hz[4];
  uint8_t requests_timed[4];
  uint8_t total_us[4];
  uint8_t surface_requests[4];
  uint8_t surface_us[4];
  uint8_t audio_requests[4];
  uint8_t audio_us[4];
  uint8_t last_opcode[4];
  uint8_t last_us[4];
  uint8_t max_opcode[4];
  uint8_t max_us[4];
} ZZ9KDiagTimingPayload;

/* DIAG_SCHED (0x0902): dual-core scheduler observability. core1_online is 1 when
 * the core-1 worker is up (else single-core fallback); tasks_on_core{1,0} count
 * crypto tasks executed on each core (actual execution core, not dispatch). */
typedef struct ZZ9KDiagSchedPayload {
  uint8_t version[4];
  uint8_t core1_online[4];
  uint8_t tasks_on_core1[4];
  uint8_t tasks_on_core0[4];
  uint8_t decode_requests[4];  /* version 2+: decompress decode count */
  uint8_t decode_us[4];        /* version 2+: cumulative decode microseconds */
} ZZ9KDiagSchedPayload;

/* The version-1 base payload (core1_online + tasks_on_core{1,0}). Firmware that
 * predates the decode-timing counters sends exactly this; version 2+ appends the
 * decode_* fields. Decoders require the base and read the extension when present. */
#define ZZ9K_DIAG_SCHED_PAYLOAD_V1_BYTES 16U

typedef char ZZ9KDiagSchedPayload_must_be_24_bytes[
  (sizeof(ZZ9KDiagSchedPayload) == 24U) ? 1 : -1];

typedef struct ZZ9KQueryServicePayload {
  uint8_t service_id[4];
  uint8_t reserved[44];
} ZZ9KQueryServicePayload;

typedef struct ZZ9KServiceInfoPayload {
  uint8_t service_id[4];
  uint8_t version[4];
  uint8_t capability_bits[4];
  uint8_t flags[4];
  uint8_t opcode_base[4];
  uint8_t opcode_count[4];
  uint8_t max_inline_payload[4];
  uint8_t name[20];
} ZZ9KServiceInfoPayload;

typedef struct ZZ9KSurfaceInfoPayload {
  uint8_t handle[4];
  uint8_t arm_addr[4];
  uint8_t width[4];
  uint8_t height[4];
  uint8_t pitch[4];
  uint8_t format[4];
  uint8_t flags[4];
  uint8_t length[4];
  uint8_t reserved[16];
} ZZ9KSurfaceInfoPayload;

typedef struct ZZ9KAllocSurfacePayload {
  uint8_t width[4];
  uint8_t height[4];
  uint8_t format[4];
  uint8_t flags[4];
  uint8_t pitch[4];
  uint8_t reserved[28];
} ZZ9KAllocSurfacePayload;

typedef struct ZZ9KFreeSurfacePayload {
  uint8_t handle[4];
  uint8_t reserved[44];
} ZZ9KFreeSurfacePayload;

typedef struct ZZ9KScaleImagePayload {
  uint8_t src_surface[4];
  uint8_t dst_surface[4];
  uint8_t src_x[4];
  uint8_t src_y[4];
  uint8_t src_w[4];
  uint8_t src_h[4];
  uint8_t dst_x[4];
  uint8_t dst_y[4];
  uint8_t dst_w[4];
  uint8_t dst_h[4];
  uint8_t filter[4];
  uint8_t flags[4];
} ZZ9KScaleImagePayload;

typedef struct ZZ9KScaleImageClippedPayload {
  uint8_t src_surface[4];
  uint8_t dst_surface[4];
  uint8_t src_x[2];
  uint8_t src_y[2];
  uint8_t src_w[2];
  uint8_t src_h[2];
  uint8_t dst_x[2];
  uint8_t dst_y[2];
  uint8_t dst_w[2];
  uint8_t dst_h[2];
  uint8_t clip_x[2];
  uint8_t clip_y[2];
  uint8_t clip_w[2];
  uint8_t clip_h[2];
  uint8_t filter[4];
  uint8_t flags[4];
  uint8_t reserved[8];
} ZZ9KScaleImageClippedPayload;

typedef struct ZZ9KSurfaceFillPayload {
  uint8_t surface[4];
  uint8_t x[4];
  uint8_t y[4];
  uint8_t width[4];
  uint8_t height[4];
  uint8_t color[4];
  uint8_t flags[4];
  uint8_t reserved[20];
} ZZ9KSurfaceFillPayload;

typedef struct ZZ9KSurfaceCopyPayload {
  uint8_t src_surface[4];
  uint8_t dst_surface[4];
  uint8_t src_x[4];
  uint8_t src_y[4];
  uint8_t dst_x[4];
  uint8_t dst_y[4];
  uint8_t width[4];
  uint8_t height[4];
  uint8_t flags[4];
  uint8_t reserved[12];
} ZZ9KSurfaceCopyPayload;

typedef struct ZZ9KImageDecodePayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t dst_surface[4];
  uint8_t dst_x[4];
  uint8_t dst_y[4];
  uint8_t dst_width[4];
  uint8_t dst_height[4];
  uint8_t output_format[4];
  uint8_t flags[4];
  uint8_t reserved[8];
} ZZ9KImageDecodePayload;

typedef struct ZZ9KImageDecodeResultPayload {
  uint8_t width[4];
  uint8_t height[4];
  uint8_t output_format[4];
  uint8_t flags[4];
  uint8_t bytes_written[4];
  uint8_t reserved[28];
} ZZ9KImageDecodeResultPayload;

typedef struct ZZ9KImageSessionBeginPayload {
  uint8_t codec[4];
  uint8_t output_mode[4];
  uint8_t dst_surface[4];
  uint8_t dst_x[4];
  uint8_t dst_y[4];
  uint8_t dst_width[4];
  uint8_t dst_height[4];
  uint8_t output_format[4];
  uint8_t tile_handle[4];
  uint8_t tile_stride[4];
  uint8_t tile_rows[4];
  uint8_t flags[4];
} ZZ9KImageSessionBeginPayload;

typedef struct ZZ9KImageSessionFeedPayload {
  uint8_t session[4];
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t flags[4];
  uint8_t reserved[28];
} ZZ9KImageSessionFeedPayload;

typedef struct ZZ9KImageSessionResultPayload {
  uint8_t session[4];
  uint8_t state[4];
  uint8_t image_width[4];
  uint8_t image_height[4];
  uint8_t output_format[4];
  uint8_t tile_x[4];
  uint8_t tile_y[4];
  uint8_t tile_width[4];
  uint8_t tile_height[4];
  uint8_t bytes_consumed[4];
  uint8_t bytes_written[4];
  uint8_t flags[4];
} ZZ9KImageSessionResultPayload;

typedef struct ZZ9KImageSessionClosePayload {
  uint8_t session[4];
  uint8_t flags[4];
  uint8_t reserved[40];
} ZZ9KImageSessionClosePayload;

typedef struct ZZ9KAudioDecodePayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t dst_capacity[4];
  uint8_t output_hz[4];
  uint8_t output_channels[4];
  uint8_t output_format[4];
  uint8_t flags[4];
  uint8_t reserved[8];
} ZZ9KAudioDecodePayload;

typedef struct ZZ9KAudioDecodeResultPayload {
  uint8_t bytes_consumed[4];
  uint8_t bytes_written[4];
  uint8_t sample_rate[4];
  uint8_t channels[4];
  uint8_t sample_format[4];
  uint8_t frames_written[4];
  uint8_t flags[4];
  uint8_t reserved[20];
} ZZ9KAudioDecodeResultPayload;

typedef struct ZZ9KAudioStreamBeginPayload {
  uint8_t mp3_ring_handle[4];
  uint8_t mp3_ring_capacity[4];
  uint8_t pcm_ring_handle[4];
  uint8_t pcm_ring_capacity[4];
  uint8_t output_hz[4];
  uint8_t output_channels[4];
  uint8_t output_format[4];
  uint8_t low_water_bytes[4];
  uint8_t high_water_bytes[4];
  uint8_t flags[4];
  uint8_t reserved[8];
} ZZ9KAudioStreamBeginPayload;

typedef struct ZZ9KAudioStreamFeedPayload {
  uint8_t session[4];
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t flags[4];
  uint8_t reserved[28];
} ZZ9KAudioStreamFeedPayload;

typedef struct ZZ9KAudioStreamReadPayload {
  uint8_t session[4];
  uint8_t pcm_read[4];
  uint8_t flags[4];
  uint8_t reserved[36];
} ZZ9KAudioStreamReadPayload;

typedef struct ZZ9KAudioStreamClosePayload {
  uint8_t session[4];
  uint8_t flags[4];
  uint8_t reserved[40];
} ZZ9KAudioStreamClosePayload;

typedef struct ZZ9KAudioStreamResultPayload {
  uint8_t session[4];
  uint8_t state[4];
  uint8_t sample_rate[4];
  uint8_t channels[4];
  uint8_t sample_format[4];
  uint8_t mp3_read[4];
  uint8_t pcm_write[4];
  uint8_t pcm_read[4];
  uint8_t frames_decoded[4];
  uint8_t bytes_consumed[4];
  uint8_t bytes_produced[4];
  uint8_t flags[4];
} ZZ9KAudioStreamResultPayload;

typedef struct ZZ9KCryptoHashPayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t key_handle[4];
  uint8_t key_offset[4];
  uint8_t key_length[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[8];
} ZZ9KCryptoHashPayload;

typedef struct ZZ9KCryptoStreamPayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t key_handle[4];
  uint8_t key_offset[4];
  uint8_t nonce_handle[4];
  uint8_t nonce_offset[4];
  uint8_t counter[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
} ZZ9KCryptoStreamPayload;

typedef struct ZZ9KCryptoAeadPayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t aad_handle[4];
  uint8_t aad_offset[4];
  uint8_t aad_length[4];
  uint8_t key_handle[4];
  uint8_t key_offset[4];
  uint8_t nonce_handle[4];
  uint8_t flags[4];
} ZZ9KCryptoAeadPayload;

typedef struct ZZ9KCryptoResultPayload {
  uint8_t bytes_written[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[36];
} ZZ9KCryptoResultPayload;

struct ZZ9KCryptoKxPayload {
  uint8_t scalar_handle[4];
  uint8_t scalar_offset[4];
  uint8_t point_handle[4];
  uint8_t point_offset[4];
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[16];
};

typedef struct ZZ9KCryptoVerifyPayload {
  uint8_t algorithm[4];
  uint8_t hash_handle[4];
  uint8_t hash_offset[4];
  uint8_t hash_length[4];
  uint8_t sig_handle[4];
  uint8_t sig_offset[4];
  uint8_t sig_length[4];
  uint8_t key_handle[4];
  uint8_t key_offset[4];
  uint8_t key_length[4];
  uint8_t reserved[8];
} ZZ9KCryptoVerifyPayload;

typedef struct ZZ9KDecompressPayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t dst_capacity[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[16];
} ZZ9KDecompressPayload;

typedef struct ZZ9KDecompressTestPayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t output_limit[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[24];
} ZZ9KDecompressTestPayload;

typedef struct ZZ9KDecompressStreamBeginPayload {
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t output_limit[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[24];
} ZZ9KDecompressStreamBeginPayload;

typedef struct ZZ9KDecompressStreamReadPayload {
  uint8_t session[4];
  uint8_t dst_handle[4];
  uint8_t dst_offset[4];
  uint8_t dst_capacity[4];
  uint8_t flags[4];
  uint8_t reserved[28];
} ZZ9KDecompressStreamReadPayload;

typedef struct ZZ9KDecompressStreamFeedPayload {
  uint8_t session[4];
  uint8_t src_handle[4];
  uint8_t src_offset[4];
  uint8_t src_length[4];
  uint8_t flags[4];
  uint8_t reserved[28];
} ZZ9KDecompressStreamFeedPayload;

typedef struct ZZ9KDecompressStreamClosePayload {
  uint8_t session[4];
  uint8_t flags[4];
  uint8_t reserved[40];
} ZZ9KDecompressStreamClosePayload;

typedef struct ZZ9KDecompressResultPayload {
  uint8_t bytes_consumed[4];
  uint8_t bytes_written[4];
  uint8_t checksum[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[28];
} ZZ9KDecompressResultPayload;

typedef struct ZZ9KDecompressStreamResultPayload {
  uint8_t session[4];
  uint8_t bytes_consumed[4];
  uint8_t bytes_written[4];
  uint8_t checksum[4];
  uint8_t algorithm[4];
  uint8_t flags[4];
  uint8_t reserved[24];
} ZZ9KDecompressStreamResultPayload;

typedef char ZZ9KAllocSharedPayload_must_be_48_bytes[
  (sizeof(ZZ9KAllocSharedPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KSharedBufferInfoPayload_must_be_48_bytes[
  (sizeof(ZZ9KSharedBufferInfoPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KFreeSharedPayload_must_be_48_bytes[
  (sizeof(ZZ9KFreeSharedPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KMemFillPayload_must_be_48_bytes[
  (sizeof(ZZ9KMemFillPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KMemCopyPayload_must_be_48_bytes[
  (sizeof(ZZ9KMemCopyPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDiagPayload_must_be_48_bytes[
  (sizeof(ZZ9KDiagPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDiagTimingPayload_must_be_48_bytes[
  (sizeof(ZZ9KDiagTimingPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KQueryServicePayload_must_be_48_bytes[
  (sizeof(ZZ9KQueryServicePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KServiceInfoPayload_must_be_48_bytes[
  (sizeof(ZZ9KServiceInfoPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KSurfaceInfoPayload_must_be_48_bytes[
  (sizeof(ZZ9KSurfaceInfoPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KAllocSurfacePayload_must_be_48_bytes[
  (sizeof(ZZ9KAllocSurfacePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KFreeSurfacePayload_must_be_48_bytes[
  (sizeof(ZZ9KFreeSurfacePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KScaleImagePayload_must_be_48_bytes[
  (sizeof(ZZ9KScaleImagePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KScaleImageClippedPayload_must_be_48_bytes[
  (sizeof(ZZ9KScaleImageClippedPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KSurfaceFillPayload_must_be_48_bytes[
  (sizeof(ZZ9KSurfaceFillPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KSurfaceCopyPayload_must_be_48_bytes[
  (sizeof(ZZ9KSurfaceCopyPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KImageDecodePayload_must_be_48_bytes[
  (sizeof(ZZ9KImageDecodePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KImageDecodeResultPayload_must_be_48_bytes[
  (sizeof(ZZ9KImageDecodeResultPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KImageSessionBeginPayload_must_be_48_bytes[
  (sizeof(ZZ9KImageSessionBeginPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KImageSessionFeedPayload_must_be_48_bytes[
  (sizeof(ZZ9KImageSessionFeedPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KImageSessionResultPayload_must_be_48_bytes[
  (sizeof(ZZ9KImageSessionResultPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KImageSessionClosePayload_must_be_48_bytes[
  (sizeof(ZZ9KImageSessionClosePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KAudioDecodePayload_must_be_48_bytes[
  (sizeof(ZZ9KAudioDecodePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KAudioDecodeResultPayload_must_be_48_bytes[
  (sizeof(ZZ9KAudioDecodeResultPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KCryptoHashPayload_must_be_48_bytes[
  (sizeof(ZZ9KCryptoHashPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KCryptoStreamPayload_must_be_48_bytes[
  (sizeof(ZZ9KCryptoStreamPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KCryptoAeadPayload_must_be_48_bytes[
  (sizeof(ZZ9KCryptoAeadPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KCryptoResultPayload_must_be_48_bytes[
  (sizeof(ZZ9KCryptoResultPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KCryptoVerifyPayload_must_be_48_bytes[
  (sizeof(ZZ9KCryptoVerifyPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressPayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressTestPayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressTestPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressResultPayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressResultPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressStreamBeginPayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressStreamBeginPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressStreamReadPayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressStreamReadPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressStreamFeedPayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressStreamFeedPayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressStreamClosePayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressStreamClosePayload) == 48U) ? 1 : -1
];
typedef char ZZ9KDecompressStreamResultPayload_must_be_48_bytes[
  (sizeof(ZZ9KDecompressStreamResultPayload) == 48U) ? 1 : -1
];

typedef union ZZ9KEntryPayload {
  uint8_t inline_data[48];
  ZZ9KBufferPayload buffer;
} ZZ9KEntryPayload;

static inline uint16_t zz9k_get_be16(const volatile void *p)
{
  const volatile uint8_t *b = (const volatile uint8_t *)p;
  return (uint16_t)(((uint16_t)b[0] << 8) | b[1]);
}

static inline uint32_t zz9k_get_be32(const volatile void *p)
{
  const volatile uint8_t *b = (const volatile uint8_t *)p;
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
         ((uint32_t)b[2] << 8) | b[3];
}

static inline void zz9k_put_be16(volatile void *p, uint16_t value)
{
  volatile uint8_t *b = (volatile uint8_t *)p;
  b[0] = (uint8_t)((value >> 8) & 0xffU);
  b[1] = (uint8_t)(value & 0xffU);
}

static inline void zz9k_put_be32(volatile void *p, uint32_t value)
{
  volatile uint8_t *b = (volatile uint8_t *)p;
  b[0] = (uint8_t)((value >> 24) & 0xffU);
  b[1] = (uint8_t)((value >> 16) & 0xffU);
  b[2] = (uint8_t)((value >> 8) & 0xffU);
  b[3] = (uint8_t)(value & 0xffU);
}

typedef struct ZZ9KMailboxEntry {
  uint32_t request_id;
  uint16_t opcode;
  uint16_t status;
  uint16_t flags;
  uint16_t payload_len;
  uint32_t user_cookie;
  ZZ9KEntryPayload payload;
} ZZ9KMailboxEntry;

typedef char ZZ9KMailboxEntry_must_be_64_bytes[
  (sizeof(ZZ9KMailboxEntry) == ZZ9K_MAILBOX_ENTRY_SIZE) ? 1 : -1
];

/*
 * Wire entries are always big-endian so the m68k host, ARM firmware, and
 * native tooling can share exactly one representation.
 */
typedef struct ZZ9KMailboxWireEntry {
  uint8_t request_id[4];
  uint8_t opcode[2];
  uint8_t status[2];
  uint8_t flags[2];
  uint8_t payload_len[2];
  uint8_t user_cookie[4];
  uint8_t payload[48];
} ZZ9KMailboxWireEntry;

typedef char ZZ9KMailboxWireEntry_must_be_64_bytes[
  (sizeof(ZZ9KMailboxWireEntry) == ZZ9K_MAILBOX_ENTRY_SIZE) ? 1 : -1
];

typedef struct ZZ9KMailboxDescriptor {
  uint8_t magic[4];
  uint8_t abi_major[2];
  uint8_t abi_minor[2];
  uint8_t descriptor_size[4];
  uint8_t request_ring_offset[4];
  uint8_t request_ring_entries[4];
  uint8_t request_head[4];
  uint8_t request_tail[4];
  uint8_t completion_ring_offset[4];
  uint8_t completion_ring_entries[4];
  uint8_t completion_head[4];
  uint8_t completion_tail[4];
  uint8_t capability_bits[4];
  uint8_t reserved[80];
} ZZ9KMailboxDescriptor;

typedef char ZZ9KMailboxDescriptor_must_be_128_bytes[
  (sizeof(ZZ9KMailboxDescriptor) == ZZ9K_MAILBOX_DESCRIPTOR_SIZE) ? 1 : -1
];

typedef struct ZZ9KCaps {
  uint32_t magic;
  uint16_t abi_major;
  uint16_t abi_minor;
  uint32_t capability_bits;
  uint32_t max_inline_payload;
  uint32_t max_shared_buffers;
  uint32_t max_surfaces;
  uint32_t firmware_version;
  uint32_t request_ring_entries;
  uint32_t completion_ring_entries;
  uint32_t reserved[6];
} ZZ9KCaps;

typedef struct ZZ9KQueryCapsPayload {
  uint32_t magic;
  uint16_t abi_major;
  uint16_t abi_minor;
  uint32_t capability_bits;
  uint32_t max_inline_payload;
  uint32_t max_shared_buffers;
  uint32_t max_surfaces;
  uint32_t firmware_version;
  uint32_t request_ring_entries;
  uint32_t completion_ring_entries;
  uint8_t reserved[12];
} ZZ9KQueryCapsPayload;

typedef char ZZ9KQueryCapsPayload_must_fit_inline[
  (sizeof(ZZ9KQueryCapsPayload) <= 48U) ? 1 : -1
];

typedef struct ZZ9KSurfaceDesc {
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t format;
  uint32_t flags;
  uint32_t handle;
  uint32_t offset;
  uint32_t reserved[5];
} ZZ9KSurfaceDesc;

typedef struct ZZ9KScaleImageDesc {
  uint32_t src_surface;
  uint32_t dst_surface;
  uint32_t src_x;
  uint32_t src_y;
  uint32_t src_w;
  uint32_t src_h;
  uint32_t dst_x;
  uint32_t dst_y;
  uint32_t dst_w;
  uint32_t dst_h;
  uint32_t filter;
  uint32_t flags;
} ZZ9KScaleImageDesc;

typedef struct ZZ9KScaleImageClippedDesc {
  uint32_t src_surface;
  uint32_t dst_surface;
  uint32_t src_x;
  uint32_t src_y;
  uint32_t src_w;
  uint32_t src_h;
  uint32_t dst_x;
  uint32_t dst_y;
  uint32_t dst_w;
  uint32_t dst_h;
  uint32_t clip_x;
  uint32_t clip_y;
  uint32_t clip_w;
  uint32_t clip_h;
  uint32_t filter;
  uint32_t flags;
} ZZ9KScaleImageClippedDesc;

typedef struct ZZ9KSurfaceFillDesc {
  uint32_t surface;
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t color;
  uint32_t flags;
} ZZ9KSurfaceFillDesc;

typedef struct ZZ9KSurfaceCopyDesc {
  uint32_t src_surface;
  uint32_t dst_surface;
  uint32_t src_x;
  uint32_t src_y;
  uint32_t dst_x;
  uint32_t dst_y;
  uint32_t width;
  uint32_t height;
  uint32_t flags;
} ZZ9KSurfaceCopyDesc;

typedef struct ZZ9KImageDecodeDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t dst_surface;
  uint32_t dst_x;
  uint32_t dst_y;
  uint32_t dst_width;
  uint32_t dst_height;
  uint32_t output_format;
  uint32_t flags;
} ZZ9KImageDecodeDesc;

typedef struct ZZ9KImageDecodeResult {
  uint32_t width;
  uint32_t height;
  uint32_t output_format;
  uint32_t flags;
  uint32_t bytes_written;
} ZZ9KImageDecodeResult;

typedef struct ZZ9KImageSessionBeginDesc {
  uint32_t codec;
  uint32_t output_mode;
  uint32_t dst_surface;
  uint32_t dst_x;
  uint32_t dst_y;
  uint32_t dst_width;
  uint32_t dst_height;
  uint32_t output_format;
  uint32_t tile_handle;
  uint32_t tile_stride;
  uint32_t tile_rows;
  uint32_t flags;
} ZZ9KImageSessionBeginDesc;

typedef struct ZZ9KImageSessionFeedDesc {
  uint32_t session;
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t flags;
} ZZ9KImageSessionFeedDesc;

typedef struct ZZ9KImageSessionResult {
  uint32_t session;
  uint32_t state;
  uint32_t image_width;
  uint32_t image_height;
  uint32_t output_format;
  uint32_t tile_x;
  uint32_t tile_y;
  uint32_t tile_width;
  uint32_t tile_height;
  uint32_t bytes_consumed;
  uint32_t bytes_written;
  uint32_t flags;
} ZZ9KImageSessionResult;

typedef struct ZZ9KAudioDecodeDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t dst_handle;
  uint32_t dst_offset;
  uint32_t dst_capacity;
  uint32_t output_hz;
  uint32_t output_channels;
  uint32_t output_format;
  uint32_t flags;
} ZZ9KAudioDecodeDesc;

typedef struct ZZ9KAudioDecodeResult {
  uint32_t bytes_consumed;
  uint32_t bytes_written;
  uint32_t sample_rate;
  uint32_t channels;
  uint32_t sample_format;
  uint32_t frames_written;
  uint32_t flags;
} ZZ9KAudioDecodeResult;

typedef struct ZZ9KAudioStreamBeginDesc {
  uint32_t mp3_ring_handle;
  uint32_t mp3_ring_capacity;
  uint32_t pcm_ring_handle;
  uint32_t pcm_ring_capacity;
  uint32_t output_hz;
  uint32_t output_channels;
  uint32_t output_format;
  uint32_t low_water_bytes;
  uint32_t high_water_bytes;
  uint32_t flags;
} ZZ9KAudioStreamBeginDesc;

typedef struct ZZ9KAudioStreamFeedDesc {
  uint32_t session;
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t flags;
} ZZ9KAudioStreamFeedDesc;

typedef struct ZZ9KAudioStreamResult {
  uint32_t session;
  uint32_t state;
  uint32_t sample_rate;
  uint32_t channels;
  uint32_t sample_format;
  uint32_t mp3_read;
  uint32_t pcm_write;
  uint32_t pcm_read;
  uint32_t frames_decoded;
  uint32_t bytes_consumed;
  uint32_t bytes_produced;
  uint32_t flags;
} ZZ9KAudioStreamResult;

typedef struct ZZ9KCryptoHashDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t dst_handle;
  uint32_t dst_offset;
  uint32_t key_handle;
  uint32_t key_offset;
  uint32_t key_length;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KCryptoHashDesc;

typedef struct ZZ9KCryptoStreamDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t dst_handle;
  uint32_t dst_offset;
  uint32_t key_handle;
  uint32_t key_offset;
  uint32_t nonce_handle;
  uint32_t nonce_offset;
  uint32_t counter;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KCryptoStreamDesc;

typedef struct ZZ9KCryptoAeadDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t dst_handle;
  uint32_t dst_offset;
  uint32_t aad_handle;
  uint32_t aad_offset;
  uint32_t aad_length;
  uint32_t key_handle;
  uint32_t key_offset;
  uint32_t nonce_handle;
  uint32_t flags;
} ZZ9KCryptoAeadDesc;

typedef struct ZZ9KCryptoResult {
  uint32_t bytes_written;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KCryptoResult;

typedef struct ZZ9KDecompressDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t dst_handle;
  uint32_t dst_offset;
  uint32_t dst_capacity;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KDecompressDesc;

typedef struct ZZ9KDecompressTestDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t output_limit;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KDecompressTestDesc;

typedef struct ZZ9KDecompressStreamBeginDesc {
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t output_limit;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KDecompressStreamBeginDesc;

typedef struct ZZ9KDecompressStreamReadDesc {
  uint32_t session;
  uint32_t dst_handle;
  uint32_t dst_offset;
  uint32_t dst_capacity;
  uint32_t flags;
} ZZ9KDecompressStreamReadDesc;

typedef struct ZZ9KDecompressStreamFeedDesc {
  uint32_t session;
  uint32_t src_handle;
  uint32_t src_offset;
  uint32_t src_length;
  uint32_t flags;
} ZZ9KDecompressStreamFeedDesc;

typedef struct ZZ9KDecompressResult {
  uint32_t bytes_consumed;
  uint32_t bytes_written;
  uint32_t checksum;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KDecompressResult;

typedef struct ZZ9KDecompressStreamResult {
  uint32_t session;
  uint32_t bytes_consumed;
  uint32_t bytes_written;
  uint32_t checksum;
  uint32_t algorithm;
  uint32_t flags;
} ZZ9KDecompressStreamResult;

enum ZZ9KSurfaceFormat {
  ZZ9K_SURFACE_FORMAT_UNKNOWN = 0,
  ZZ9K_SURFACE_FORMAT_RGB565 = 1,
  ZZ9K_SURFACE_FORMAT_ARGB8888 = 2,
  ZZ9K_SURFACE_FORMAT_RGBA8888 = 3,
  ZZ9K_SURFACE_FORMAT_INDEX8 = 4,
  ZZ9K_SURFACE_FORMAT_PLANAR = 5,
  ZZ9K_SURFACE_FORMAT_RGB555 = 6,
  ZZ9K_SURFACE_FORMAT_BGRA8888 = 7,
  ZZ9K_SURFACE_FORMAT_RGB888 = 8
};

enum ZZ9KSurfaceFlags {
  ZZ9K_SURFACE_FLAG_CPU_VISIBLE = 1U << 0,
  ZZ9K_SURFACE_FLAG_FRAMEBUFFER = 1U << 1,
  ZZ9K_SURFACE_FLAG_DISPLAYED = 1U << 2,
  ZZ9K_SURFACE_FLAG_SHARED_BUFFER = 1U << 3,
  ZZ9K_SURFACE_FLAG_ARM_LOCAL = 1U << 4
};

enum ZZ9KScaleFilter {
  ZZ9K_SCALE_NEAREST = 0,
  ZZ9K_SCALE_BILINEAR = 1,
  ZZ9K_SCALE_BICUBIC = 2,
  ZZ9K_SCALE_LANCZOS3 = 3
};

enum ZZ9KImageDecodeFlags {
  ZZ9K_IMAGE_DECODE_FLAG_FIT = 1U << 0,
  ZZ9K_IMAGE_DECODE_FLAG_PRESERVE_ASPECT = 1U << 1,
  ZZ9K_IMAGE_DECODE_FLAG_DITHER = 1U << 2
};

enum ZZ9KImageDecodeResultFlags {
  ZZ9K_IMAGE_DECODE_RESULT_ALPHA = 1U << 0,
  ZZ9K_IMAGE_DECODE_RESULT_ANIMATED = 1U << 1,
  ZZ9K_IMAGE_DECODE_RESULT_PARTIAL = 1U << 2
};

enum ZZ9KImageCodec {
  ZZ9K_IMAGE_CODEC_JPEG = 1U,
  ZZ9K_IMAGE_CODEC_PNG = 2U,
  ZZ9K_IMAGE_CODEC_GIF = 3U
};

enum ZZ9KImageOutputMode {
  ZZ9K_IMAGE_OUTPUT_SURFACE = 1U,
  ZZ9K_IMAGE_OUTPUT_FRAMEBUFFER = 2U,
  ZZ9K_IMAGE_OUTPUT_TILE_BUFFER = 3U
};

enum ZZ9KImageSessionFeedFlags {
  ZZ9K_IMAGE_SESSION_FEED_EOF = 1U << 0
};

enum ZZ9KImageSessionState {
  ZZ9K_IMAGE_SESSION_STATE_NEED_INPUT = 1U,
  ZZ9K_IMAGE_SESSION_STATE_HEADER_READY = 2U,
  ZZ9K_IMAGE_SESSION_STATE_TILE_READY = 3U,
  ZZ9K_IMAGE_SESSION_STATE_COMPLETE = 4U,
  ZZ9K_IMAGE_SESSION_STATE_ERROR = 5U
};

enum ZZ9KImageSessionResultFlags {
  ZZ9K_IMAGE_SESSION_RESULT_HEADER_READY = 1U << 0,
  ZZ9K_IMAGE_SESSION_RESULT_PARTIAL = 1U << 1,
  ZZ9K_IMAGE_SESSION_RESULT_SCALED = 1U << 2
};

enum ZZ9KAudioStreamFeedFlags {
  ZZ9K_AUDIO_STREAM_FEED_EOF = 1U << 0
};

enum ZZ9KAudioStreamState {
  ZZ9K_AUDIO_STREAM_STATE_NEED_INPUT = 1U,
  ZZ9K_AUDIO_STREAM_STATE_STREAMING = 2U,
  ZZ9K_AUDIO_STREAM_STATE_DONE = 3U,
  ZZ9K_AUDIO_STREAM_STATE_ERROR = 4U
};

enum ZZ9KAudioStreamResultFlags {
  ZZ9K_AUDIO_STREAM_RESULT_NEED_INPUT = 1U << 0,
  ZZ9K_AUDIO_STREAM_RESULT_PCM_READY = 1U << 1,
  ZZ9K_AUDIO_STREAM_RESULT_DONE = 1U << 2,
  ZZ9K_AUDIO_STREAM_RESULT_BACKPRESSURE = 1U << 3
};

enum ZZ9KCryptoHashAlgorithm {
  ZZ9K_CRYPTO_HASH_NONE = 0,
  ZZ9K_CRYPTO_HASH_SHA1 = 1,
  ZZ9K_CRYPTO_HASH_SHA256 = 2,
  ZZ9K_CRYPTO_HASH_SHA384 = 3,
  ZZ9K_CRYPTO_HASH_SHA512 = 4,
  ZZ9K_CRYPTO_HASH_BLAKE2S = 5,
  ZZ9K_CRYPTO_HASH_POLY1305 = 6
};

enum ZZ9KCryptoHashFlags {
  ZZ9K_CRYPTO_HASH_FLAG_HMAC = 1U << 0
};

enum ZZ9KCryptoStreamAlgorithm {
  ZZ9K_CRYPTO_STREAM_NONE = 0,
  ZZ9K_CRYPTO_STREAM_CHACHA20 = 1
};

enum ZZ9KCryptoAeadAlgorithm {
  ZZ9K_CRYPTO_AEAD_NONE = 0,
  ZZ9K_CRYPTO_AEAD_CHACHA20_POLY1305 = 1,
  ZZ9K_CRYPTO_AEAD_AES128_GCM = 2,
  ZZ9K_CRYPTO_AEAD_AES256_GCM = 3
};

typedef enum ZZ9KCryptoKxAlgorithm {
  ZZ9K_CRYPTO_KX_NONE    = 0,
  ZZ9K_CRYPTO_KX_X25519  = 1U,
  ZZ9K_CRYPTO_KX_P256    = 2U
} ZZ9KCryptoKxAlgorithm;

/* KX descriptor flags. KEYGEN turns a P-256 KX request into a base-point
 * multiply: `scalar` is the private key, `point` is unused, and `dst` receives
 * the full uncompressed public point (ZZ9K_CRYPTO_P256_POINT_BYTES). Firmware
 * that predates the keygen primitive rejects a non-zero flags word with
 * UNSUPPORTED, so callers must gate on ZZ9K_SERVICE_FLAG_CRYPTO_P256_KEYGEN. */
#define ZZ9K_CRYPTO_KX_FLAG_KEYGEN 1U

typedef enum ZZ9KCryptoVerifyAlgorithm {
  ZZ9K_CRYPTO_VERIFY_NONE                     = 0,
  ZZ9K_CRYPTO_VERIFY_ECDSA_P256_SHA256        = 1U,
  ZZ9K_CRYPTO_VERIFY_RSA_PKCS1_2048_SHA256    = 2U
} ZZ9KCryptoVerifyAlgorithm;

#define ZZ9K_CRYPTO_X25519_KEY_BYTES    32U
#define ZZ9K_CRYPTO_X25519_SHARED_BYTES 32U

/* P-256 public point is the uncompressed SEC1 form: 0x04 || X(32) || Y(32). */
#define ZZ9K_CRYPTO_P256_POINT_BYTES   65U
#define ZZ9K_CRYPTO_P256_PRIVATE_BYTES 32U
#define ZZ9K_CRYPTO_P256_SHARED_BYTES  32U

/* AES-GCM (reuses the AEAD op): 96-bit nonce, 128-bit tag, key 16 or 32. */
#define ZZ9K_CRYPTO_AES128_KEY_BYTES    16U
#define ZZ9K_CRYPTO_AES256_KEY_BYTES    32U
#define ZZ9K_CRYPTO_AES_GCM_NONCE_BYTES 12U
#define ZZ9K_CRYPTO_AES_GCM_TAG_BYTES   16U

/* The AEAD payload has no algorithm field, so the AEAD algorithm is carried in
 * the flags field at bits 8-15. A zero algorithm nibble means the legacy
 * default, ChaCha20-Poly1305, so existing callers stay byte-compatible. */
enum ZZ9KCryptoAeadFlags {
  ZZ9K_CRYPTO_AEAD_FLAG_DECRYPT = 1U << 0,
  ZZ9K_CRYPTO_AEAD_ALG_SHIFT = 8,
  ZZ9K_CRYPTO_AEAD_ALG_MASK = 0xFFU << 8
};

/* Encode/decode the AEAD algorithm in the flags field. */
#define ZZ9K_CRYPTO_AEAD_FLAG_ALG(alg) \
  (((uint32_t)(alg) << ZZ9K_CRYPTO_AEAD_ALG_SHIFT) & ZZ9K_CRYPTO_AEAD_ALG_MASK)
#define ZZ9K_CRYPTO_AEAD_FLAG_GET_ALG(flags) \
  (((flags) & ZZ9K_CRYPTO_AEAD_ALG_MASK) >> ZZ9K_CRYPTO_AEAD_ALG_SHIFT)

enum ZZ9KCompressionAlgorithm {
  ZZ9K_COMPRESSION_NONE = 0,
  ZZ9K_COMPRESSION_DEFLATE_RAW = 1,
  ZZ9K_COMPRESSION_ZLIB = 2,
  ZZ9K_COMPRESSION_GZIP = 3,
  ZZ9K_COMPRESSION_LZ4_BLOCK = 4,
  ZZ9K_COMPRESSION_LZMA_ALONE = 5,
  ZZ9K_COMPRESSION_LZMA2 = 6
};

enum ZZ9KDecompressFlags {
  ZZ9K_DECOMPRESS_FLAG_EXPECT_END = 1U << 0,
  ZZ9K_DECOMPRESS_FLAG_FEED_INPUT = 1U << 1
};

enum ZZ9KDecompressStreamFeedFlags {
  ZZ9K_DECOMPRESS_STREAM_FEED_EOF = 1U << 0
};

enum ZZ9KDecompressResultFlags {
  ZZ9K_DECOMPRESS_RESULT_STREAM_END = 1U << 0,
  ZZ9K_DECOMPRESS_RESULT_CHECKSUM_VALID = 1U << 1,
  ZZ9K_DECOMPRESS_RESULT_NEED_INPUT = 1U << 2
};

#ifdef __cplusplus
}
#endif

#endif /* ZZ9K_ABI_H */
