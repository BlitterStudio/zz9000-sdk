/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-stats.h"

#include <string.h>

void zzplay_stats_reset(ZZPlayStatsCore *stats)
{
  if (stats) {
    memset(stats, 0, sizeof(*stats));
  }
}

void zzplay_stats_record_frame(ZZPlayStatsCore *stats,
                               uint32_t wall_us,
                               uint32_t decode_us)
{
  if (!stats) {
    return;
  }
  stats->wall_us += wall_us;
  stats->decode_us += decode_us;
  stats->report_decode_us += decode_us;
  stats->report_frames++;
  stats->total_frames++;
}

void zzplay_stats_reset_report(ZZPlayStatsCore *stats)
{
  if (!stats) {
    return;
  }
  stats->report_decode_us = 0U;
  stats->report_frames = 0U;
}

uint32_t zzplay_fps_milli(uint32_t frames, uint64_t elapsed_us)
{
  uint64_t value;

  if (frames == 0U || elapsed_us == 0U) {
    return 0U;
  }
  value = ((uint64_t)frames * 1000000000ULL + elapsed_us / 2U) /
          elapsed_us;
  return value > 0xffffffffULL ? 0xffffffffU : (uint32_t)value;
}
