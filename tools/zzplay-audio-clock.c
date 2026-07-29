/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-audio-clock.h"

#include <string.h>

#define ZZPLAY_PTS_HZ 90000U

void zzplay_audio_clock_prepare(ZZPlayAudioClock *clock,
                                uint32_t sample_rate,
                                uint32_t queue_limit_frames)
{
  if (!clock) {
    return;
  }
  memset(clock, 0, sizeof(*clock));
  clock->sample_rate = sample_rate;
  clock->queue_limit_frames = queue_limit_frames;
  clock->state = sample_rate == 0U
                     ? ZZPLAY_AUDIO_CLOCK_CLOSED
                     : ZZPLAY_AUDIO_CLOCK_PREBUFFERING;
}

int zzplay_audio_clock_queue(ZZPlayAudioClock *clock, uint32_t frames)
{
  if (!clock || frames == 0U ||
      (clock->state != ZZPLAY_AUDIO_CLOCK_PREBUFFERING &&
       clock->state != ZZPLAY_AUDIO_CLOCK_PLAYING &&
       clock->state != ZZPLAY_AUDIO_CLOCK_PAUSED)) {
    return 0;
  }
  if (clock->queue_limit_frames != 0U &&
      ((uint64_t)frames > clock->queue_limit_frames ||
       clock->queued_frames >
           (uint64_t)clock->queue_limit_frames - frames)) {
    return 0;
  }
  clock->submitted_frames += frames;
  clock->queued_frames += frames;
  return 1;
}

int zzplay_audio_clock_play(ZZPlayAudioClock *clock)
{
  if (!clock || clock->state != ZZPLAY_AUDIO_CLOCK_PREBUFFERING ||
      clock->queued_frames == 0U) {
    return 0;
  }
  clock->state = ZZPLAY_AUDIO_CLOCK_PLAYING;
  return 1;
}

int zzplay_audio_clock_pause(ZZPlayAudioClock *clock)
{
  if (!clock || clock->state != ZZPLAY_AUDIO_CLOCK_PLAYING) {
    return 0;
  }
  clock->state = ZZPLAY_AUDIO_CLOCK_PAUSED;
  return 1;
}

int zzplay_audio_clock_resume(ZZPlayAudioClock *clock)
{
  if (!clock || clock->state != ZZPLAY_AUDIO_CLOCK_PAUSED) {
    return 0;
  }
  clock->state = ZZPLAY_AUDIO_CLOCK_PLAYING;
  return 1;
}

int zzplay_audio_clock_begin_drain(ZZPlayAudioClock *clock)
{
  if (!clock ||
      (clock->state != ZZPLAY_AUDIO_CLOCK_PLAYING &&
       clock->state != ZZPLAY_AUDIO_CLOCK_PREBUFFERING)) {
    return 0;
  }
  clock->state = ZZPLAY_AUDIO_CLOCK_DRAINING;
  return 1;
}

int zzplay_audio_clock_complete(ZZPlayAudioClock *clock,
                                uint32_t submitted_frames,
                                uint32_t played_frames)
{
  uint32_t discarded;

  if (!clock || submitted_frames == 0U ||
      played_frames > submitted_frames ||
      clock->queued_frames < submitted_frames ||
      (clock->state != ZZPLAY_AUDIO_CLOCK_PLAYING &&
       clock->state != ZZPLAY_AUDIO_CLOCK_DRAINING)) {
    return 0;
  }
  discarded = submitted_frames - played_frames;
  clock->queued_frames -= submitted_frames;
  clock->played_frames += played_frames;
  clock->discarded_frames += discarded;
  return 1;
}

void zzplay_audio_clock_note_underrun(ZZPlayAudioClock *clock)
{
  if (clock && clock->state == ZZPLAY_AUDIO_CLOCK_PLAYING) {
    clock->underruns++;
  }
}

int zzplay_audio_clock_drained(const ZZPlayAudioClock *clock)
{
  return clock && clock->state == ZZPLAY_AUDIO_CLOCK_DRAINING &&
         clock->queued_frames == 0U;
}

void zzplay_audio_clock_stop(ZZPlayAudioClock *clock)
{
  if (!clock || clock->state == ZZPLAY_AUDIO_CLOCK_CLOSED) {
    return;
  }
  clock->discarded_frames += clock->queued_frames;
  clock->queued_frames = 0U;
  clock->state = ZZPLAY_AUDIO_CLOCK_STOPPED;
}

void zzplay_audio_clock_close(ZZPlayAudioClock *clock)
{
  if (!clock) {
    return;
  }
  zzplay_audio_clock_stop(clock);
  clock->state = ZZPLAY_AUDIO_CLOCK_CLOSED;
}

uint64_t zzplay_audio_clock_pts(const ZZPlayAudioClock *clock,
                                uint64_t origin_pts)
{
  uint64_t seconds;
  uint64_t remainder;

  if (!clock || clock->sample_rate == 0U) {
    return origin_pts;
  }
  seconds = clock->played_frames / clock->sample_rate;
  remainder = clock->played_frames % clock->sample_rate;
  return origin_pts + seconds * ZZPLAY_PTS_HZ +
         (remainder * ZZPLAY_PTS_HZ) / clock->sample_rate;
}
