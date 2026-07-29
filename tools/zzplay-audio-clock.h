/*
 * Completion-derived audio clock for zzplay sinks.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZPLAY_AUDIO_CLOCK_H
#define ZZPLAY_AUDIO_CLOCK_H

#include <stdint.h>

typedef enum ZZPlayAudioClockState {
  ZZPLAY_AUDIO_CLOCK_CLOSED = 0,
  ZZPLAY_AUDIO_CLOCK_PREBUFFERING,
  ZZPLAY_AUDIO_CLOCK_PLAYING,
  ZZPLAY_AUDIO_CLOCK_PAUSED,
  ZZPLAY_AUDIO_CLOCK_DRAINING,
  ZZPLAY_AUDIO_CLOCK_STOPPED
} ZZPlayAudioClockState;

typedef struct ZZPlayAudioClock {
  uint32_t sample_rate;
  uint32_t queue_limit_frames;
  uint64_t submitted_frames;
  uint64_t played_frames;
  uint64_t discarded_frames;
  uint64_t queued_frames;
  uint32_t underruns;
  ZZPlayAudioClockState state;
} ZZPlayAudioClock;

void zzplay_audio_clock_prepare(ZZPlayAudioClock *clock,
                                uint32_t sample_rate,
                                uint32_t queue_limit_frames);
int zzplay_audio_clock_queue(ZZPlayAudioClock *clock, uint32_t frames);
int zzplay_audio_clock_play(ZZPlayAudioClock *clock);
int zzplay_audio_clock_pause(ZZPlayAudioClock *clock);
int zzplay_audio_clock_resume(ZZPlayAudioClock *clock);
int zzplay_audio_clock_begin_drain(ZZPlayAudioClock *clock);
int zzplay_audio_clock_complete(ZZPlayAudioClock *clock,
                                uint32_t submitted_frames,
                                uint32_t played_frames);
void zzplay_audio_clock_note_underrun(ZZPlayAudioClock *clock);
int zzplay_audio_clock_drained(const ZZPlayAudioClock *clock);
void zzplay_audio_clock_stop(ZZPlayAudioClock *clock);
void zzplay_audio_clock_close(ZZPlayAudioClock *clock);
uint64_t zzplay_audio_clock_pts(const ZZPlayAudioClock *clock,
                                uint64_t origin_pts);

#endif /* ZZPLAY_AUDIO_CLOCK_H */
