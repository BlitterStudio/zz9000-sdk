/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-mp3-transport.h"

uint32_t zzplay_mp3_pcm_ack_batch_bytes(uint32_t pcm_capacity)
{
  if (pcm_capacity < 2U) {
    return 0U;
  }
  if (pcm_capacity < ZZPLAY_MP3_DEFAULT_PCM_CAPACITY) {
    return (pcm_capacity / 2U) & ~1UL;
  }
  return 192UL * 1024UL;
}

int zzplay_mp3_pcm_ack_due(uint32_t pending_ack,
                           uint32_t pcm_capacity,
                           int force)
{
  if (pending_ack == 0U) {
    return 0;
  }
  return force || pending_ack >=
                      zzplay_mp3_pcm_ack_batch_bytes(pcm_capacity);
}

uint32_t zzplay_mp3_decode_quantum_bytes(uint32_t requested,
                                         uint32_t pcm_capacity)
{
  uint32_t quantum;

  if (pcm_capacity < 2U || requested == 0U) {
    return 0U;
  }
  quantum = requested & ~1UL;
  return quantum != 0U && quantum < pcm_capacity ? quantum : 0U;
}

uint32_t zzplay_mp3_feed_chunk_bytes(uint32_t requested,
                                     uint32_t decode_quantum)
{
  uint32_t chunk;

  if (requested != 0U) {
    chunk = requested & ~1UL;
  } else if (decode_quantum == 0U) {
    chunk = ZZPLAY_MP3_FEED_MAX_BYTES;
  } else {
    chunk = (decode_quantum / 4U) & ~1UL;
    if (chunk < ZZPLAY_MP3_FEED_MIN_BYTES) {
      chunk = ZZPLAY_MP3_FEED_MIN_BYTES;
    }
  }
  return chunk != 0U && chunk <= ZZPLAY_MP3_FEED_MAX_BYTES
             ? chunk
             : 0U;
}

uint32_t zzplay_mp3_ring_advance(uint32_t offset,
                                 uint32_t bytes,
                                 uint32_t capacity)
{
  if (capacity == 0U) {
    return offset;
  }
  bytes %= capacity;
  if (bytes >= capacity - offset) {
    return bytes - (capacity - offset);
  }
  return offset + bytes;
}

uint32_t zzplay_mp3_input_buffered(uint32_t bytes_fed,
                                   uint32_t bytes_consumed)
{
  return bytes_consumed >= bytes_fed
             ? 0U
             : bytes_fed - bytes_consumed;
}

int zzplay_mp3_input_room_low(uint32_t bytes_fed,
                              uint32_t bytes_consumed,
                              uint32_t capacity,
                              uint32_t next_feed_bytes)
{
  uint32_t buffered;

  if (capacity == 0U || next_feed_bytes == 0U ||
      next_feed_bytes > capacity) {
    return 0;
  }
  buffered = zzplay_mp3_input_buffered(bytes_fed, bytes_consumed);
  return buffered > capacity - next_feed_bytes;
}
