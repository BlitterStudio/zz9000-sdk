/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "../tools/zzplay-probe.h"
#include "../tools/zzplay-stats.h"

#include <stdint.h>
#include <string.h>

int main(void)
{
  static const uint8_t sequence[] = {
    0xaa, 0x00, 0x00, 0x01, 0xb3, 0x13, 0xe0, 0xf3, 0x13
  };
  ZZPlayVideoInfo info;

  memset(&info, 0, sizeof(info));
  if (!zzplay_probe_mpeg_sequence(sequence, sizeof(sequence), &info)) {
    return 1;
  }
  if (info.width != 318U || info.height != 243U ||
      info.frame_rate_milli != 25000U) {
    return 2;
  }
  if (zzplay_probe_mpeg_sequence(sequence, 7U, &info)) {
    return 3;
  }
  if (zzplay_mpeg_frame_rate_milli(4U) != 29970U ||
      zzplay_mpeg_frame_rate_milli(9U) != 0U) {
    return 4;
  }
  if (zzplay_fps_milli(50U, 2000000U) != 25000U ||
      zzplay_fps_milli(60U, 2002002U) != 29970U ||
      zzplay_fps_milli(0U, 1000000U) != 0U ||
      zzplay_fps_milli(1U, 0U) != 0U) {
    return 5;
  }
  return 0;
}
