/*
 * Small, host-testable MPEG sequence-header probe used by zzplay.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZPLAY_PROBE_H
#define ZZPLAY_PROBE_H

#include <stddef.h>
#include <stdint.h>

typedef struct ZZPlayVideoInfo {
  uint32_t width;
  uint32_t height;
  uint32_t frame_rate_milli;
} ZZPlayVideoInfo;

static inline uint32_t zzplay_mpeg_frame_rate_milli(uint8_t code)
{
  static const uint32_t rates[16] = {
    0U, 23976U, 24000U, 25000U, 29970U, 30000U, 50000U, 59940U,
    60000U, 0U, 0U, 0U, 0U, 0U, 0U, 0U
  };

  return rates[code & 0x0fU];
}

/* Find the first MPEG sequence header (00 00 01 b3). A caller streaming
 * chunks should retain the final seven bytes between calls so a split start
 * code and its four-byte body can be seen in the next probe. */
static inline int zzplay_probe_mpeg_sequence(const uint8_t *data,
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
    width = ((uint32_t)data[i + 4U] << 4) | (data[i + 5U] >> 4);
    height = ((uint32_t)(data[i + 5U] & 0x0fU) << 8) | data[i + 6U];
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

#endif /* ZZPLAY_PROBE_H */
