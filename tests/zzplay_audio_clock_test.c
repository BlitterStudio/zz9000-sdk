/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "../tools/zzplay-audio-clock.h"

#include <stdint.h>

static int check_completion_clock(void)
{
  ZZPlayAudioClock clock;

  zzplay_audio_clock_prepare(&clock, 48000U, 4096U);
  if (clock.state != ZZPLAY_AUDIO_CLOCK_PREBUFFERING ||
      !zzplay_audio_clock_queue(&clock, 960U) ||
      clock.played_frames != 0U ||
      zzplay_audio_clock_pts(&clock, 9000U) != 9000U ||
      !zzplay_audio_clock_play(&clock)) {
    return 0;
  }
  if (!zzplay_audio_clock_complete(&clock, 960U, 960U) ||
      clock.played_frames != 960U ||
      clock.queued_frames != 0U ||
      zzplay_audio_clock_pts(&clock, 9000U) != 10800U) {
    return 0;
  }
  return 1;
}

static int check_pause_drain_and_short_completion(void)
{
  ZZPlayAudioClock clock;

  zzplay_audio_clock_prepare(&clock, 44100U, 4096U);
  if (!zzplay_audio_clock_queue(&clock, 1000U) ||
      !zzplay_audio_clock_play(&clock) ||
      !zzplay_audio_clock_pause(&clock) ||
      zzplay_audio_clock_complete(&clock, 1000U, 1000U) ||
      clock.played_frames != 0U ||
      !zzplay_audio_clock_resume(&clock) ||
      !zzplay_audio_clock_complete(&clock, 1000U, 900U) ||
      clock.played_frames != 900U ||
      clock.discarded_frames != 100U ||
      !zzplay_audio_clock_begin_drain(&clock) ||
      !zzplay_audio_clock_drained(&clock)) {
    return 0;
  }
  return 1;
}

static int check_bounds_and_lifecycle(void)
{
  ZZPlayAudioClock clock;

  zzplay_audio_clock_prepare(&clock, 32000U, 100U);
  if (zzplay_audio_clock_queue(&clock, 101U) ||
      !zzplay_audio_clock_queue(&clock, 60U) ||
      zzplay_audio_clock_queue(&clock, 41U) ||
      !zzplay_audio_clock_play(&clock)) {
    return 0;
  }
  zzplay_audio_clock_note_underrun(&clock);
  if (clock.underruns != 1U) {
    return 0;
  }
  zzplay_audio_clock_stop(&clock);
  if (clock.state != ZZPLAY_AUDIO_CLOCK_STOPPED ||
      clock.queued_frames != 0U ||
      clock.discarded_frames != 60U) {
    return 0;
  }
  zzplay_audio_clock_close(&clock);
  return clock.state == ZZPLAY_AUDIO_CLOCK_CLOSED;
}

int main(void)
{
  if (!check_completion_clock()) {
    return 1;
  }
  if (!check_pause_drain_and_short_completion()) {
    return 2;
  }
  if (!check_bounds_and_lifecycle()) {
    return 3;
  }
  return 0;
}
