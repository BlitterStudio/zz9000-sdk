/*
 * Compile-time ABI layout checks.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k/abi.h"
#include <stdint.h>

typedef char mailbox_entry_is_64_bytes[
  (sizeof(ZZ9KMailboxEntry) == ZZ9K_MAILBOX_ENTRY_SIZE) ? 1 : -1
];

typedef char buffer_payload_is_48_bytes[
  (sizeof(ZZ9KBufferPayload) == 48U) ? 1 : -1
];

typedef char alloc_shared_payload_is_48_bytes[
  (sizeof(ZZ9KAllocSharedPayload) == 48U) ? 1 : -1
];

typedef char shared_buffer_info_payload_is_48_bytes[
  (sizeof(ZZ9KSharedBufferInfoPayload) == 48U) ? 1 : -1
];

typedef char mem_fill_payload_is_48_bytes[
  (sizeof(ZZ9KMemFillPayload) == 48U) ? 1 : -1
];

typedef char mem_copy_payload_is_48_bytes[
  (sizeof(ZZ9KMemCopyPayload) == 48U) ? 1 : -1
];

typedef char diag_payload_is_48_bytes[
  (sizeof(ZZ9KDiagPayload) == 48U) ? 1 : -1
];

typedef char diag_timing_payload_is_48_bytes[
  (sizeof(ZZ9KDiagTimingPayload) == 48U) ? 1 : -1
];

typedef char diag_sched_payload_is_24_bytes[
  (sizeof(ZZ9KDiagSchedPayload) == 24U) ? 1 : -1
];

typedef char query_service_payload_is_48_bytes[
  (sizeof(ZZ9KQueryServicePayload) == 48U) ? 1 : -1
];

typedef char service_info_payload_is_48_bytes[
  (sizeof(ZZ9KServiceInfoPayload) == 48U) ? 1 : -1
];

typedef char surface_info_payload_is_48_bytes[
  (sizeof(ZZ9KSurfaceInfoPayload) == 48U) ? 1 : -1
];

typedef char alloc_surface_payload_is_48_bytes[
  (sizeof(ZZ9KAllocSurfacePayload) == 48U) ? 1 : -1
];

typedef char free_surface_payload_is_48_bytes[
  (sizeof(ZZ9KFreeSurfacePayload) == 48U) ? 1 : -1
];

typedef char scale_image_payload_is_48_bytes[
  (sizeof(ZZ9KScaleImagePayload) == 48U) ? 1 : -1
];

typedef char scale_image_clipped_payload_is_48_bytes[
  (sizeof(ZZ9KScaleImageClippedPayload) == 48U) ? 1 : -1
];

typedef char surface_fill_payload_is_48_bytes[
  (sizeof(ZZ9KSurfaceFillPayload) == 48U) ? 1 : -1
];

typedef char surface_copy_payload_is_48_bytes[
  (sizeof(ZZ9KSurfaceCopyPayload) == 48U) ? 1 : -1
];

typedef char image_decode_payload_is_48_bytes[
  (sizeof(ZZ9KImageDecodePayload) == 48U) ? 1 : -1
];

typedef char image_decode_result_payload_is_48_bytes[
  (sizeof(ZZ9KImageDecodeResultPayload) == 48U) ? 1 : -1
];

typedef char audio_decode_payload_is_48_bytes[
  (sizeof(ZZ9KAudioDecodePayload) == 48U) ? 1 : -1
];

typedef char audio_decode_result_payload_is_48_bytes[
  (sizeof(ZZ9KAudioDecodeResultPayload) == 48U) ? 1 : -1
];

typedef char audio_stream_begin_payload_is_48_bytes[
  (sizeof(ZZ9KAudioStreamBeginPayload) == 48U) ? 1 : -1
];

typedef char audio_stream_feed_payload_is_48_bytes[
  (sizeof(ZZ9KAudioStreamFeedPayload) == 48U) ? 1 : -1
];

typedef char audio_stream_read_payload_is_48_bytes[
  (sizeof(ZZ9KAudioStreamReadPayload) == 48U) ? 1 : -1
];

typedef char audio_stream_close_payload_is_48_bytes[
  (sizeof(ZZ9KAudioStreamClosePayload) == 48U) ? 1 : -1
];

typedef char audio_stream_play_payload_is_48_bytes[
  (sizeof(ZZ9KAudioStreamPlayPayload) == 48U) ? 1 : -1
];

typedef char audio_stream_stop_payload_is_48_bytes[
  (sizeof(ZZ9KAudioStreamStopPayload) == 48U) ? 1 : -1
];

typedef char audio_stream_result_payload_is_48_bytes[
  (sizeof(ZZ9KAudioStreamResultPayload) == 48U) ? 1 : -1
];

typedef char crypto_hash_payload_is_48_bytes[
  (sizeof(ZZ9KCryptoHashPayload) == 48U) ? 1 : -1
];

typedef char crypto_stream_payload_is_48_bytes[
  (sizeof(ZZ9KCryptoStreamPayload) == 48U) ? 1 : -1
];

typedef char crypto_aead_payload_is_48_bytes[
  (sizeof(ZZ9KCryptoAeadPayload) == 48U) ? 1 : -1
];

typedef char crypto_result_payload_is_48_bytes[
  (sizeof(ZZ9KCryptoResultPayload) == 48U) ? 1 : -1
];

typedef char decompress_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressPayload) == 48U) ? 1 : -1
];

typedef char decompress_test_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressTestPayload) == 48U) ? 1 : -1
];

typedef char decompress_result_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressResultPayload) == 48U) ? 1 : -1
];

typedef char decompress_stream_begin_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressStreamBeginPayload) == 48U) ? 1 : -1
];

typedef char decompress_stream_read_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressStreamReadPayload) == 48U) ? 1 : -1
];

typedef char decompress_stream_feed_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressStreamFeedPayload) == 48U) ? 1 : -1
];

typedef char decompress_stream_close_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressStreamClosePayload) == 48U) ? 1 : -1
];

typedef char decompress_stream_result_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressStreamResultPayload) == 48U) ? 1 : -1
];

typedef char decompress_batch_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressBatchPayload) == 48U) ? 1 : -1
];

typedef char decompress_batch_result_payload_is_48_bytes[
  (sizeof(ZZ9KDecompressBatchResultPayload) == 48U) ? 1 : -1
];

int main(void)
{
  uint8_t data[4];

  if (ZZ9K_REG_CONFIG != 0x0004U) return 3;
  if (ZZ9K_REG_SDK_DIAG_WRITE != 0x0110U) return 10;
  if (ZZ9K_REG_SDK_DIAG_DATA != 0x0114U) return 11;
  if (ZZ9K_REG_SDK_DIAG_Z3ADDR != 0x0118U) return 12;
  if (ZZ9K_INTERRUPT_SDK != 0x0008U) return 13;
  if (ZZ9K_CONFIG_ACK_MODE != 0x0008U) return 14;
  if (ZZ9K_CONFIG_ACK_SDK != 0x0080U) return 15;
  if (ZZ9K_SDK_IRQ_ACK_VALUE != 0x0001U) return 16;
  if (ZZ9K_SDK_IRQ_ENABLE_VALUE != 0x0002U) return 17;
  if (ZZ9K_SDK_IRQ_DISABLE_VALUE != 0x0004U) return 18;
  if (ZZ9K_OP_DECOMPRESS_STREAM_BEGIN != ZZ9K_SERVICE_CODEC + 0x02U) {
    return 19;
  }
  if (ZZ9K_OP_DECOMPRESS_STREAM_READ != ZZ9K_SERVICE_CODEC + 0x03U) {
    return 20;
  }
  if (ZZ9K_OP_DECOMPRESS_STREAM_CLOSE != ZZ9K_SERVICE_CODEC + 0x04U) {
    return 21;
  }
  if (ZZ9K_OP_DECOMPRESS_STREAM_FEED != ZZ9K_SERVICE_CODEC + 0x05U) {
    return 23;
  }
  if (ZZ9K_OP_DECOMPRESS_BATCH != ZZ9K_SERVICE_CODEC + 0x06U) return 60;
  if (ZZ9K_OP_DECOMPRESS_BATCH != 0x0606U) return 61;
  if (ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_BATCH != (1U << 30)) return 62;
  if (ZZ9K_BATCH_ARENA_MAGIC != 0x5A424154UL) return 63;
  if (ZZ9K_BATCH_ARENA_VERSION != 1U) return 64;
  if (ZZ9K_BATCH_MODE_TEST != 0U || ZZ9K_BATCH_MODE_EXTRACT != 1U) return 65;
  if (ZZ9K_BATCH_HEADER_SIZE != 48U) return 66;
  if (ZZ9K_BATCH_DESC_SIZE != 32U) return 67;
  if (ZZ9K_BATCH_RESULT_SIZE != 16U) return 68;
  if (ZZ9K_BATCH_MEMBER_LIMIT != 1024U) return 69;
  if (ZZ9K_BATCH_MEMBER_FLAG_HAVE_CRC != (1U << 0)) return 70;
  if (ZZ9K_BATCH_TEST_MAX_EXPECTED != 0x04000000UL) return 94;
  if (ZZ9K_OP_DIAG_TIMING != ZZ9K_SERVICE_DIAG + 0x01U) {
    return 24;
  }
  if (ZZ9K_OP_DIAG_SCHED != ZZ9K_SERVICE_DIAG + 0x02U) {
    return 25;
  }
  if (ZZ9K_OP_DIAG_SCHED != 0x0902U) {
    return 26;
  }
  if (ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_STREAM != (1U << 23)) {
    return 22;
  }
  if (ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED != (1U << 24)) {
    return 24;
  }
  if (ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_FEED != (1U << 25)) {
    return 25;
  }
  if (ZZ9K_SERVICE_FLAG_CODEC_ZLIB_FEED != (1U << 26)) return 26;
  if (ZZ9K_SERVICE_FLAG_CODEC_GZIP_FEED != (1U << 27)) return 27;
  if (ZZ9K_SERVICE_FLAG_IMAGE_RGB888_OUTPUT != (1U << 26)) return 47;
  if (ZZ9K_SURFACE_FORMAT_RGB888 != 8U) return 48;
  if (ZZ9K_DECOMPRESS_FLAG_FEED_INPUT != (1U << 1)) return 28;
  if (ZZ9K_DECOMPRESS_STREAM_FEED_EOF != (1U << 0)) return 29;
  if (ZZ9K_DECOMPRESS_RESULT_NEED_INPUT != (1U << 2)) return 30;
  if (ZZ9K_CAP_GFX_OPS != (1U << 17)) return 31;
  if (ZZ9K_CAP_STORAGE_OPS != (1U << 18)) return 32;
  if (ZZ9K_SERVICE_FLAG_AUDIO_MP3_DECODE != (1U << 16)) return 33;
  if (ZZ9K_SERVICE_FLAG_AUDIO_PCM_MIX != (1U << 17)) return 34;
  if (ZZ9K_SERVICE_FLAG_AUDIO_RESAMPLE != (1U << 18)) return 35;
  if (ZZ9K_SERVICE_FLAG_AUDIO_PCM16_STEREO != (1U << 19)) return 36;
  if (ZZ9K_AUDIO_SAMPLE_FORMAT_S16LE != 1U) return 37;
  if (ZZ9K_AUDIO_DECODE_FLAG_EXPECT_END != (1U << 0)) return 38;
  if (ZZ9K_AUDIO_DECODE_RESULT_END != (1U << 0)) return 39;
  if (ZZ9K_OP_AUDIO_STREAM_BEGIN != ZZ9K_SERVICE_AUDIO + 0x03U) return 40;
  if (ZZ9K_OP_AUDIO_STREAM_FEED != ZZ9K_SERVICE_AUDIO + 0x04U) return 41;
  if (ZZ9K_OP_AUDIO_STREAM_READ != ZZ9K_SERVICE_AUDIO + 0x05U) return 42;
  if (ZZ9K_OP_AUDIO_STREAM_CLOSE != ZZ9K_SERVICE_AUDIO + 0x06U) return 43;
  if (ZZ9K_OP_AUDIO_STREAM_PLAY != ZZ9K_SERVICE_AUDIO + 0x07U) return 95;
  if (ZZ9K_OP_AUDIO_STREAM_STOP != ZZ9K_SERVICE_AUDIO + 0x08U) return 96;
  if (ZZ9K_OP_AUDIO_STREAM_PLAY != 0x0507U) return 97;
  if (ZZ9K_OP_AUDIO_STREAM_STOP != 0x0508U) return 98;
  if (ZZ9K_CAP_AUDIO_PLAYBACK != (1U << 19)) return 99;
  if (ZZ9K_SERVICE_FLAG_AUDIO_MP3_STREAM != (1U << 20)) return 44;
  if (ZZ9K_AUDIO_STREAM_FEED_EOF != (1U << 0)) return 45;
  if (ZZ9K_AUDIO_STREAM_STATE_STREAMING == ZZ9K_AUDIO_STREAM_STATE_DONE) {
    return 46;
  }

  if (sizeof(struct ZZ9KCryptoKxPayload) != 48U) return 70;
  if ((int)ZZ9K_OP_CRYPTO_KX != 0x0803) return 71;
  if ((int)ZZ9K_CRYPTO_KX_X25519 != 1) return 72;
  if (ZZ9K_CRYPTO_X25519_KEY_BYTES != 32U) return 73;
  if (ZZ9K_CRYPTO_X25519_SHARED_BYTES != 32U) return 74;
  if (ZZ9K_SERVICE_FLAG_CRYPTO_X25519 != (1U << 16)) return 75;

  if ((int)ZZ9K_CRYPTO_KX_P256 != 2) return 76;
  if (ZZ9K_CRYPTO_P256_POINT_BYTES != 65U) return 77;
  if (ZZ9K_CRYPTO_P256_SHARED_BYTES != 32U) return 78;
  if (ZZ9K_SERVICE_FLAG_CRYPTO_P256 != (1U << 17)) return 79;

  if (sizeof(ZZ9KCryptoVerifyPayload) != 48U) return 80;
  if ((int)ZZ9K_OP_CRYPTO_VERIFY != 0x0804) return 81;
  if ((int)ZZ9K_CRYPTO_VERIFY_ECDSA_P256_SHA256 != 1) return 82;
  if ((int)ZZ9K_CRYPTO_VERIFY_RSA_PKCS1_2048_SHA256 != 2) return 83;
  if (ZZ9K_SERVICE_FLAG_CRYPTO_ECDSA_P256 != (1U << 18)) return 84;
  if (ZZ9K_SERVICE_FLAG_CRYPTO_RSA_2048 != (1U << 19)) return 85;

  if ((int)ZZ9K_CRYPTO_AEAD_AES128_GCM != 2) return 86;
  if ((int)ZZ9K_CRYPTO_AEAD_AES256_GCM != 3) return 87;
  if (ZZ9K_SERVICE_FLAG_CRYPTO_AES_GCM != (1U << 20)) return 88;
  if (ZZ9K_CRYPTO_AES128_KEY_BYTES != 16U) return 89;
  if (ZZ9K_CRYPTO_AES256_KEY_BYTES != 32U) return 90;
  if (ZZ9K_CRYPTO_AES_GCM_NONCE_BYTES != 12U) return 91;
  if (ZZ9K_CRYPTO_AES_GCM_TAG_BYTES != 16U) return 92;
  /* AEAD algorithm round-trips through the flags field. */
  if (ZZ9K_CRYPTO_AEAD_FLAG_GET_ALG(
          ZZ9K_CRYPTO_AEAD_FLAG_ALG(ZZ9K_CRYPTO_AEAD_AES256_GCM) |
          ZZ9K_CRYPTO_AEAD_FLAG_DECRYPT) != ZZ9K_CRYPTO_AEAD_AES256_GCM) {
    return 93;
  }

  zz9k_put_be16(data, 0x1234U);
  if (zz9k_get_be16(data) != 0x1234U) return 1;

  zz9k_put_be32(data, 0x12345678UL);
  if (zz9k_get_be32(data) != 0x12345678UL) return 2;

  return 0;
}
