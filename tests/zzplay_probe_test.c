/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "../tools/zzplay-probe.h"
#include "../tools/zzplay-stats.h"
#include "../tools/zzplay-stream.h"

#include <stdint.h>
#include <string.h>

int main(void)
{
  static const uint8_t sequence[] = {
    0xaa, 0x00, 0x00, 0x01, 0xb3, 0x13, 0xe0, 0xf3, 0x13
  };
  ZZPlayVideoInfo info;
  uint32_t accepted_total;
  uint32_t offset;
  uint32_t remaining;

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
  if (!zzplay_video_backend_available(ZZPLAY_REQUIRED_VIDEO_FLAGS) ||
      zzplay_video_backend_available(
          ZZPLAY_REQUIRED_VIDEO_FLAGS &
          ~ZZ9K_SERVICE_FLAG_VIDEO_STREAMING_INPUT)) {
    return 6;
  }
  accepted_total = 0U;
  offset = 0U;
  remaining = 65536U;
  if (!zzplay_advance_input(&offset, &remaining, &accepted_total, 16384U) ||
      offset != 16384U || remaining != 49152U ||
      accepted_total != 16384U ||
      !zzplay_advance_input(&offset, &remaining, &accepted_total, 65536U) ||
      offset != 65536U || remaining != 0U || accepted_total != 65536U) {
    return 7;
  }
  accepted_total = 4096U;
  offset = 1024U;
  remaining = 2048U;
  if (zzplay_advance_input(&offset, &remaining, &accepted_total, 6145U) ||
      offset != 1024U || remaining != 2048U || accepted_total != 4096U) {
    return 8;
  }
  return 0;
}
