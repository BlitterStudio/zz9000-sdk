/* FPS and timing statistics for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_STATS_H
#define ZZPLAY_STATS_H

#include <stdint.h>

typedef struct ZZPlayStatsCore {
  uint64_t wall_us;
  uint64_t decode_us;
  uint64_t report_decode_us;
  uint32_t report_frames;
  uint32_t total_frames;
} ZZPlayStatsCore;

void zzplay_stats_reset(ZZPlayStatsCore *stats);
void zzplay_stats_record_frame(ZZPlayStatsCore *stats,
                               uint32_t wall_us,
                               uint32_t decode_us);
void zzplay_stats_reset_report(ZZPlayStatsCore *stats);
uint32_t zzplay_fps_milli(uint32_t frames, uint64_t elapsed_us);

#endif /* ZZPLAY_STATS_H */
