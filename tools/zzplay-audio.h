/* Audio-backend policy and the sink contract for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_AUDIO_H
#define ZZPLAY_AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef enum ZZPlayAudioBackend {
  ZZPLAY_AUDIO_AUTO = 0,
  ZZPLAY_AUDIO_AHI,
  ZZPLAY_AUDIO_MHI,
  ZZPLAY_AUDIO_AX,
  ZZPLAY_AUDIO_NONE
} ZZPlayAudioBackend;

typedef enum ZZPlayMediaAudio {
  ZZPLAY_MEDIA_AUDIO_NONE = 0,
  ZZPLAY_MEDIA_AUDIO_MP2,
  ZZPLAY_MEDIA_AUDIO_MP3
} ZZPlayMediaAudio;

typedef enum ZZPlayBackendAvailability {
  ZZPLAY_BACKEND_MISSING = 0,
  ZZPLAY_BACKEND_FREE,
  ZZPLAY_BACKEND_BUSY
} ZZPlayBackendAvailability;

typedef enum ZZPlayBackendStatus {
  ZZPLAY_BACKEND_OK = 0,
  ZZPLAY_BACKEND_UNSUPPORTED,
  ZZPLAY_BACKEND_MISSING_RESULT,
  ZZPLAY_BACKEND_BUSY_RESULT
} ZZPlayBackendStatus;

typedef struct ZZPlayAudioAvailability {
  ZZPlayBackendAvailability ahi;
  ZZPlayBackendAvailability mhi;
  ZZPlayBackendAvailability ax;
} ZZPlayAudioAvailability;

typedef struct ZZPlayBackendDecision {
  ZZPlayBackendStatus status;
  ZZPlayAudioBackend selected;
  int fell_back;
} ZZPlayBackendDecision;

typedef struct ZZPlayAudioSinkOps {
  int (*open)(void *user, uint32_t sample_rate, uint32_t channels);
  int (*write)(void *user, const void *samples, size_t length);
  void (*close)(void *user);
} ZZPlayAudioSinkOps;

ZZPlayBackendDecision zzplay_audio_select(
    ZZPlayMediaAudio media,
    ZZPlayAudioBackend requested,
    const ZZPlayAudioAvailability *availability);

#endif /* ZZPLAY_AUDIO_H */
