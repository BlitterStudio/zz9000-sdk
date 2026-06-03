/*
 * Logic checks for zz9k-mp3.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define ZZ9K_MP3_NO_MAIN 1
#include "../tools/zz9k-mp3.c"

#include <stdint.h>
#include <stdio.h>

static int test_stream_pcm_ack_batch_threshold(void)
{
  if (mp3_stream_pcm_ack_batch_bytes(256UL * 1024UL) !=
      192UL * 1024UL) {
    return 1;
  }
  if (mp3_stream_pcm_ack_batch_bytes(32UL * 1024UL) != 16UL * 1024UL) {
    return 2;
  }
  if (mp3_stream_pcm_ack_due(0U, 256UL * 1024UL, 1)) {
    return 3;
  }
  if (mp3_stream_pcm_ack_due(64UL * 1024UL, 256UL * 1024UL, 0)) {
    return 4;
  }
  if (!mp3_stream_pcm_ack_due(192UL * 1024UL, 256UL * 1024UL, 0)) {
    return 5;
  }
  if (!mp3_stream_pcm_ack_due(64UL * 1024UL, 256UL * 1024UL, 1)) {
    return 6;
  }
  if (!mp3_stream_pcm_ack_due(16UL * 1024UL, 32UL * 1024UL, 0)) {
    return 7;
  }
  return 0;
}

static int test_stream_decode_quantum(void)
{
  if (mp3_stream_decode_quantum_bytes(0U, 256UL * 1024UL) != 0U) {
    return 1;
  }
  if (mp3_stream_decode_quantum_bytes(32UL * 1024UL,
                                      256UL * 1024UL) !=
      32UL * 1024UL) {
    return 2;
  }
  if (mp3_stream_decode_quantum_bytes(0U, 32UL * 1024UL) != 0U) {
    return 3;
  }
  if (mp3_stream_decode_quantum_bytes(256UL * 1024UL,
                                      256UL * 1024UL) != 0U) {
    return 4;
  }
  return 0;
}

static int test_stream_feed_chunk(void)
{
  if (mp3_stream_feed_chunk_bytes(0U, 0U) != STREAM_CHUNK_BYTES) {
    return 1;
  }
  if (mp3_stream_feed_chunk_bytes(0U, 64UL * 1024UL) !=
      16UL * 1024UL) {
    return 2;
  }
  if (mp3_stream_feed_chunk_bytes(0U, 32UL * 1024UL) !=
      8UL * 1024UL) {
    return 3;
  }
  if (mp3_stream_feed_chunk_bytes(12UL * 1024UL,
                                  32UL * 1024UL) !=
      12UL * 1024UL) {
    return 4;
  }
  if (mp3_stream_feed_chunk_bytes(STREAM_CHUNK_BYTES + 2U,
                                  32UL * 1024UL) != 0U) {
    return 5;
  }
  return 0;
}

static int test_stream_ring_advance_wraps(void)
{
  if (mp3_stream_ring_advance(0U, 64U, 256U) != 64U) {
    return 1;
  }
  if (mp3_stream_ring_advance(200U, 100U, 256U) != 44U) {
    return 2;
  }
  if (mp3_stream_ring_advance(7U, 0U, 256U) != 7U) {
    return 3;
  }
  return 0;
}

static int test_stream_input_room_checks(void)
{
  if (mp3_stream_input_buffered(96U, 32U) != 64U) {
    return 1;
  }
  if (mp3_stream_input_buffered(32U, 96U) != 0U) {
    return 2;
  }
  if (mp3_stream_input_room_low(96U, 32U, 128U, 64U)) {
    return 3;
  }
  if (!mp3_stream_input_room_low(97U, 32U, 128U, 64U)) {
    return 4;
  }
  if (mp3_stream_input_room_low(97U, 32U, 0U, 64U)) {
    return 5;
  }
  return 0;
}

int main(void)
{
  int result;

  result = test_stream_pcm_ack_batch_threshold();
  if (result != 0) {
    printf("test_stream_pcm_ack_batch_threshold failed: %d\n", result);
    return result;
  }
  result = test_stream_decode_quantum();
  if (result != 0) {
    printf("test_stream_decode_quantum failed: %d\n", result);
    return result;
  }
  result = test_stream_feed_chunk();
  if (result != 0) {
    printf("test_stream_feed_chunk failed: %d\n", result);
    return result;
  }
  result = test_stream_ring_advance_wraps();
  if (result != 0) {
    printf("test_stream_ring_advance_wraps failed: %d\n", result);
    return result;
  }
  result = test_stream_input_room_checks();
  if (result != 0) {
    printf("test_stream_input_room_checks failed: %d\n", result);
    return result;
  }

  return 0;
}
