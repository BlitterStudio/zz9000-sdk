/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-probe.h"

#include <stdlib.h>
#include <string.h>

#define ZZPLAY_PROBE_BYTES (256U * 1024U)
#define ZZPLAY_PROBE_CHUNK 4096U
#define ZZPLAY_PROBE_CARRY 7U

uint32_t zzplay_mpeg_frame_rate_milli(uint8_t code)
{
  static const uint32_t rates[16] = {
    0U, 23976U, 24000U, 25000U, 29970U, 30000U, 50000U, 59940U,
    60000U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
  };

  return rates[code & 0x0fU];
}

int zzplay_probe_mpeg_sequence(const uint8_t *data,
                               size_t length,
                               ZZPlayVideoInfo *info)
{
  size_t i;

  if (!data || !info || length < 8U) {
    return 0;
  }
  for (i = 0U; i + 8U <= length; i++) {
    uint32_t width;
    uint32_t height;
    uint32_t rate;

    if (data[i] != 0x00U || data[i + 1U] != 0x00U ||
        data[i + 2U] != 0x01U || data[i + 3U] != 0xb3U) {
      continue;
    }
    width = ((uint32_t)data[i + 4U] << 4) |
            (uint32_t)(data[i + 5U] >> 4);
    height = ((uint32_t)(data[i + 5U] & 0x0fU) << 8) |
             (uint32_t)data[i + 6U];
    rate = zzplay_mpeg_frame_rate_milli(data[i + 7U]);
    if (width == 0U || height == 0U || rate == 0U) {
      continue;
    }
    info->width = width;
    info->height = height;
    info->frame_rate_milli = rate;
    return 1;
  }
  return 0;
}

int zzplay_probe_file(FILE *file, ZZPlayVideoInfo *info)
{
  uint8_t *buffer;
  size_t carry = 0U;
  size_t total = 0U;
  int found = 0;

  if (!file || !info || fseek(file, 0L, SEEK_SET) != 0) {
    return 0;
  }
  buffer = (uint8_t *)malloc(ZZPLAY_PROBE_CHUNK + ZZPLAY_PROBE_CARRY);
  if (!buffer) {
    return 0;
  }
  while (total < ZZPLAY_PROBE_BYTES) {
    size_t want = ZZPLAY_PROBE_CHUNK;
    size_t got;
    size_t available;

    if (want > ZZPLAY_PROBE_BYTES - total) {
      want = ZZPLAY_PROBE_BYTES - total;
    }
    got = fread(buffer + carry, 1U, want, file);
    available = carry + got;
    if (zzplay_probe_mpeg_sequence(buffer, available, info)) {
      found = 1;
      break;
    }
    total += got;
    if (got == 0U) {
      break;
    }
    carry = available < ZZPLAY_PROBE_CARRY
                ? available
                : ZZPLAY_PROBE_CARRY;
    memmove(buffer, buffer + available - carry, carry);
  }
  free(buffer);
  if (fseek(file, 0L, SEEK_SET) != 0) {
    clearerr(file);
    return 0;
  }
  clearerr(file);
  return found;
}

int zzplay_video_info_supported(const ZZPlayVideoInfo *info)
{
  return info && info->width >= 16U && info->height >= 16U &&
         info->width <= ZZPLAY_MAX_WIDTH &&
         info->height <= ZZPLAY_MAX_HEIGHT &&
         info->frame_rate_milli != 0U;
}
