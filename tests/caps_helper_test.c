/*
 * Tests for public capability bit helpers.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k/caps.h"
#include <string.h>

static int expect_name(const char *actual, const char *expected)
{
  if (!actual || strcmp(actual, expected) != 0) {
    return 0;
  }
  return 1;
}

static int test_capability_helpers(void)
{
  uint32_t available;
  uint32_t required;

  available = ZZ9K_CAP_SHARED_ALLOC | ZZ9K_CAP_SURFACES |
              ZZ9K_CAP_IMAGE_DECODE;
  required = ZZ9K_CAP_SHARED_ALLOC | ZZ9K_CAP_IMAGE_DECODE;

  if (!zz9k_has_capabilities(available, required)) return 1;
  if (zz9k_missing_capabilities(available, required) != 0U) return 2;
  if (zz9k_has_capability(available, ZZ9K_CAP_CRYPTO)) return 3;
  if (zz9k_has_capabilities(available, required | ZZ9K_CAP_CRYPTO)) return 4;
  if (zz9k_missing_capabilities(available, required | ZZ9K_CAP_CRYPTO) !=
      ZZ9K_CAP_CRYPTO) return 5;

  return 0;
}

static int test_service_flag_helpers(void)
{
  uint32_t available;
  uint32_t required;

  available = ZZ9K_SERVICE_FLAG_IMAGE_STREAMING_INPUT |
              ZZ9K_SERVICE_FLAG_IMAGE_JPEG_DIRECT_BGRA |
              ZZ9K_SERVICE_FLAG_IMAGE_TILE_OUTPUT;
  required = ZZ9K_SERVICE_FLAG_IMAGE_STREAMING_INPUT |
             ZZ9K_SERVICE_FLAG_IMAGE_JPEG_DIRECT_BGRA;

  if (!zz9k_has_service_flags(available, required)) return 1;
  if (zz9k_missing_service_flags(available, required) != 0U) return 2;
  if (zz9k_has_service_flag(
        available, ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT)) return 3;
  if (zz9k_has_service_flags(
        available, required | ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT)) {
    return 4;
  }
  if (zz9k_missing_service_flags(
        available, required | ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT) !=
      ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT) {
    return 5;
  }

  return 0;
}

static int test_service_capability_advertising_helpers(void)
{
  uint32_t image_mask;

  image_mask = ZZ9K_CAP_IMAGE_DECODE | ZZ9K_CAP_IMAGE_SCALE;
  if (zz9k_service_capability_mask(ZZ9K_SERVICE_IMAGE) != image_mask) {
    return 1;
  }
  if (!zz9k_service_advertised_by_capabilities(ZZ9K_SERVICE_IMAGE,
                                               ZZ9K_CAP_IMAGE_SCALE)) {
    return 2;
  }
  if (!zz9k_service_advertised_by_capabilities(ZZ9K_SERVICE_MEMORY,
                                               ZZ9K_CAP_SHARED_ALLOC)) {
    return 3;
  }
  if (zz9k_service_advertised_by_capabilities(ZZ9K_SERVICE_CRYPTO,
                                              ZZ9K_CAP_IMAGE_SCALE)) {
    return 4;
  }
  if (zz9k_service_capability_mask(ZZ9K_SERVICE_VENDOR) != 0U) {
    return 5;
  }
  if (zz9k_service_capability_mask(ZZ9K_SERVICE_CODEC) !=
      ZZ9K_CAP_COMPRESSION) {
    return 6;
  }
  if (!zz9k_service_advertised_by_capabilities(ZZ9K_SERVICE_CODEC,
                                               ZZ9K_CAP_COMPRESSION)) {
    return 7;
  }
  if (zz9k_service_capability_mask(ZZ9K_SERVICE_GFX) !=
      ZZ9K_CAP_GFX_OPS) {
    return 8;
  }
  if (zz9k_service_capability_mask(ZZ9K_SERVICE_STORAGE) !=
      ZZ9K_CAP_STORAGE_OPS) {
    return 9;
  }
  if (!zz9k_service_advertised_by_capabilities(ZZ9K_SERVICE_GFX,
                                               ZZ9K_CAP_GFX_OPS)) {
    return 10;
  }
  if (!zz9k_service_advertised_by_capabilities(ZZ9K_SERVICE_STORAGE,
                                               ZZ9K_CAP_STORAGE_OPS)) {
    return 11;
  }

  return 0;
}

static int test_capability_names(void)
{
  if (!expect_name(zz9k_capability_name(ZZ9K_CAP_MAILBOX), "mailbox")) {
    return 1;
  }
  if (!expect_name(zz9k_capability_name(ZZ9K_CAP_FRAMEBUFFER_SURFACE),
                   "framebuffer-surface")) {
    return 2;
  }
  if (!expect_name(zz9k_capability_name(ZZ9K_CAP_SERVICE_DISCOVERY),
                   "service-discovery")) {
    return 3;
  }
  if (!expect_name(zz9k_capability_name(ZZ9K_CAP_COMPRESSION),
                   "compression")) {
    return 4;
  }
  if (!expect_name(zz9k_capability_name(ZZ9K_CAP_GFX_OPS), "gfx-ops")) {
    return 5;
  }
  if (!expect_name(zz9k_capability_name(ZZ9K_CAP_STORAGE_OPS),
                   "storage-ops")) {
    return 6;
  }
  if (zz9k_capability_name(0x80000000UL) != 0) {
    return 7;
  }

  return 0;
}

static int test_capability_iteration(void)
{
  if (zz9k_known_capability_count() != 19U) {
    return 1;
  }
  if (zz9k_known_capability_bit(0) != ZZ9K_CAP_MAILBOX) {
    return 2;
  }
  if (zz9k_known_capability_bit(15) != ZZ9K_CAP_SURFACE_OPS) {
    return 3;
  }
  if (zz9k_known_capability_bit(16) != ZZ9K_CAP_COMPRESSION) {
    return 4;
  }
  if (zz9k_known_capability_bit(17) != ZZ9K_CAP_GFX_OPS) {
    return 5;
  }
  if (zz9k_known_capability_bit(18) != ZZ9K_CAP_STORAGE_OPS) {
    return 6;
  }
  if (zz9k_known_capability_bit(19) != 0U) {
    return 7;
  }

  return 0;
}

static int test_service_flag_names(void)
{
  if (!expect_name(zz9k_service_flag_name(ZZ9K_SERVICE_CRYPTO,
                                          ZZ9K_SERVICE_FLAG_ASYNC),
                   "async")) {
    return 1;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_IMAGE,
                       ZZ9K_SERVICE_FLAG_IMAGE_JPEG_DIRECT_BGRA),
                   "jpeg-direct-bgra")) {
    return 2;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_IMAGE,
                       ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT),
                   "framebuffer-output")) {
    return 3;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_IMAGE,
                       ZZ9K_SERVICE_FLAG_IMAGE_RGB888_OUTPUT),
                   "rgb888-output")) {
    return 21;
  }
  if (zz9k_service_flag_name(ZZ9K_SERVICE_CRYPTO,
                             ZZ9K_SERVICE_FLAG_IMAGE_JPEG_DIRECT_BGRA) != 0) {
    return 4;
  }
  if (zz9k_service_flag_name(ZZ9K_SERVICE_IMAGE, 0x80000000UL) != 0) {
    return 5;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_GZIP),
                   "gzip")) {
    return 6;
  }
  if (zz9k_service_flag_name(ZZ9K_SERVICE_CRYPTO,
                             ZZ9K_SERVICE_FLAG_CODEC_GZIP) != 0) {
    return 7;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_TEST),
                   "decompress-test")) {
    return 8;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_STREAM),
                   "decompress-stream")) {
    return 9;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED),
                   "decompress-feed")) {
    return 10;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_FEED),
                   "deflate-feed")) {
    return 11;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_ZLIB_FEED),
                   "zlib-feed")) {
    return 12;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_GZIP_FEED),
                   "gzip-feed")) {
    return 13;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_CODEC,
                       ZZ9K_SERVICE_FLAG_CODEC_LZMA2),
                   "lzma2")) {
    return 14;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_AUDIO,
                       ZZ9K_SERVICE_FLAG_AUDIO_MP3_DECODE),
                   "mp3-decode")) {
    return 15;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_AUDIO,
                       ZZ9K_SERVICE_FLAG_AUDIO_PCM_MIX),
                   "pcm-mix")) {
    return 16;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_AUDIO,
                       ZZ9K_SERVICE_FLAG_AUDIO_RESAMPLE),
                   "resample")) {
    return 17;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_AUDIO,
                       ZZ9K_SERVICE_FLAG_AUDIO_PCM16_STEREO),
                   "pcm16-stereo")) {
    return 18;
  }
  if (!expect_name(zz9k_service_flag_name(
                       ZZ9K_SERVICE_AUDIO,
                       ZZ9K_SERVICE_FLAG_AUDIO_MP3_STREAM),
                   "mp3-stream")) {
    return 20;
  }
  if (zz9k_service_flag_name(ZZ9K_SERVICE_AUDIO, 0x80000000UL) != 0) {
    return 19;
  }

  return 0;
}

static int test_service_flag_iteration(void)
{
  if (zz9k_known_service_flag_count(ZZ9K_SERVICE_CRYPTO) != 5U) {
    return 1;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CRYPTO, 0) !=
      ZZ9K_SERVICE_FLAG_FIRMWARE) {
    return 2;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CRYPTO, 3) !=
      ZZ9K_SERVICE_FLAG_ZERO_COPY) {
    return 3;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CRYPTO, 4) !=
      ZZ9K_SERVICE_FLAG_CRYPTO_X25519) {
    return 4;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CRYPTO, 5) != 0U) {
    return 40;
  }
  if (zz9k_service_flag_name(ZZ9K_SERVICE_CRYPTO,
                             ZZ9K_SERVICE_FLAG_CRYPTO_X25519) == 0 ||
      strcmp(zz9k_service_flag_name(ZZ9K_SERVICE_CRYPTO,
                                    ZZ9K_SERVICE_FLAG_CRYPTO_X25519),
             "x25519") != 0) {
    return 41;
  }
  if (zz9k_known_service_flag_count(ZZ9K_SERVICE_IMAGE) != 15U) {
    return 5;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_IMAGE, 4) !=
      ZZ9K_SERVICE_FLAG_IMAGE_JPEG_BASELINE) {
    return 6;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_IMAGE, 13) !=
      ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT) {
    return 7;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_IMAGE, 14) !=
      ZZ9K_SERVICE_FLAG_IMAGE_RGB888_OUTPUT) {
    return 8;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_IMAGE, 15) != 0U) {
    return 21;
  }
  if (zz9k_known_service_flag_count(ZZ9K_SERVICE_CODEC) != 17U) {
    return 9;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 4) !=
      ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_RAW) {
    return 10;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 9) !=
      ZZ9K_SERVICE_FLAG_CODEC_CHECKSUM) {
    return 11;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 10) !=
      ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_TEST) {
    return 12;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 11) !=
      ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_STREAM) {
    return 13;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 12) !=
      ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED) {
    return 14;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 13) !=
      ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_FEED) {
    return 15;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 14) !=
      ZZ9K_SERVICE_FLAG_CODEC_ZLIB_FEED) {
    return 16;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 15) !=
      ZZ9K_SERVICE_FLAG_CODEC_GZIP_FEED) {
    return 17;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 16) !=
      ZZ9K_SERVICE_FLAG_CODEC_LZMA2) {
    return 18;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_CODEC, 17) != 0U) {
    return 19;
  }
  if (zz9k_known_service_flag_count(ZZ9K_SERVICE_AUDIO) != 9U) {
    return 20;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_AUDIO, 4) !=
      ZZ9K_SERVICE_FLAG_AUDIO_MP3_DECODE) {
    return 21;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_AUDIO, 5) !=
      ZZ9K_SERVICE_FLAG_AUDIO_PCM_MIX) {
    return 22;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_AUDIO, 6) !=
      ZZ9K_SERVICE_FLAG_AUDIO_RESAMPLE) {
    return 23;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_AUDIO, 7) !=
      ZZ9K_SERVICE_FLAG_AUDIO_PCM16_STEREO) {
    return 24;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_AUDIO, 8) !=
      ZZ9K_SERVICE_FLAG_AUDIO_MP3_STREAM) {
    return 25;
  }
  if (zz9k_known_service_flag(ZZ9K_SERVICE_AUDIO, 9) != 0U) {
    return 26;
  }

  return 0;
}

int main(void)
{
  int result;

  result = test_capability_helpers();
  if (result != 0) return 10 + result;
  result = test_service_flag_helpers();
  if (result != 0) return 20 + result;
  result = test_service_capability_advertising_helpers();
  if (result != 0) return 30 + result;
  result = test_capability_names();
  if (result != 0) return 40 + result;
  result = test_capability_iteration();
  if (result != 0) return 50 + result;
  result = test_service_flag_names();
  if (result != 0) return 60 + result;
  result = test_service_flag_iteration();
  if (result != 0) return 70 + result;
  return 0;
}
