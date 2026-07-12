/* FPS arithmetic shared by zzplay and its native regression test.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_STATS_H
#define ZZPLAY_STATS_H

#include <stdint.h>

static uint32_t zzplay_fps_milli(uint32_t frames, uint64_t elapsed_us)
{
  uint64_t value;

  if (frames == 0U || elapsed_us == 0U) {
    return 0U;
  }
  value = ((uint64_t)frames * 1000000000ULL + elapsed_us / 2U) /
          elapsed_us;
  return value > 0xffffffffULL ? 0xffffffffU : (uint32_t)value;
}

#endif /* ZZPLAY_STATS_H */
