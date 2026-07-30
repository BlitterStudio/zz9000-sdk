/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "../tools/zzplay-audio.h"
#include "../tools/zzplay-video.h"

#include <string.h>

static int check_video_preflight(void)
{
  if (zzplay_video_sink_check(1, 3U, 1, 1) !=
          ZZPLAY_VIDEO_SINK_READY ||
      zzplay_video_sink_check(0, 0U, 0, 0) !=
          ZZPLAY_VIDEO_SINK_UNSUPPORTED_BOARD ||
      zzplay_video_sink_check(1, 2U, 0, 0) !=
          ZZPLAY_VIDEO_SINK_UNSUPPORTED_BOARD ||
      zzplay_video_sink_check(1, 3U, 0, 0) !=
          ZZPLAY_VIDEO_SINK_P96_UNAVAILABLE ||
      zzplay_video_sink_check(1, 3U, 1, 0) !=
          ZZPLAY_VIDEO_SINK_PIP_UNAVAILABLE) {
    return 0;
  }
  return 1;
}

static int check_selection(ZZPlayMediaAudio media,
                           ZZPlayAudioBackend requested,
                           ZZPlayBackendAvailability ahi,
                           ZZPlayBackendAvailability mhi,
                           ZZPlayBackendAvailability ax,
                           ZZPlayBackendStatus expected_status,
                           ZZPlayAudioBackend expected_backend,
                           int expected_fallback)
{
  ZZPlayAudioAvailability availability;
  ZZPlayBackendDecision decision;

  memset(&availability, 0, sizeof(availability));
  availability.ahi = ahi;
  availability.mhi = mhi;
  availability.ax = ax;
  decision = zzplay_audio_select(media, requested, &availability);
  return decision.status == expected_status &&
         decision.selected == expected_backend &&
         decision.fell_back == expected_fallback;
}

static int check_audio_policy(void)
{
  if (!check_selection(ZZPLAY_MEDIA_AUDIO_NONE, ZZPLAY_AUDIO_AUTO,
                       ZZPLAY_BACKEND_MISSING, ZZPLAY_BACKEND_MISSING,
                       ZZPLAY_BACKEND_MISSING, ZZPLAY_BACKEND_OK,
                       ZZPLAY_AUDIO_NONE, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP3, ZZPLAY_AUDIO_AUTO,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_FREE,
                       ZZPLAY_BACKEND_MISSING, ZZPLAY_BACKEND_OK,
                       ZZPLAY_AUDIO_MHI, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP3, ZZPLAY_AUDIO_AUTO,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_BUSY,
                       ZZPLAY_BACKEND_MISSING, ZZPLAY_BACKEND_OK,
                       ZZPLAY_AUDIO_AHI, 1) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP2, ZZPLAY_AUDIO_AUTO,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_FREE,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_OK,
                       ZZPLAY_AUDIO_AX, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP2, ZZPLAY_AUDIO_MHI,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_FREE,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_UNSUPPORTED,
                       ZZPLAY_AUDIO_NONE, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP2, ZZPLAY_AUDIO_AX,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_MISSING,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_OK,
                       ZZPLAY_AUDIO_AX, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP2, ZZPLAY_AUDIO_AUTO,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_FREE,
                       ZZPLAY_BACKEND_MISSING, ZZPLAY_BACKEND_OK,
                       ZZPLAY_AUDIO_AHI, 1) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP2, ZZPLAY_AUDIO_AUTO,
                       ZZPLAY_BACKEND_BUSY, ZZPLAY_BACKEND_FREE,
                       ZZPLAY_BACKEND_BUSY, ZZPLAY_BACKEND_BUSY_RESULT,
                       ZZPLAY_AUDIO_NONE, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP3, ZZPLAY_AUDIO_MHI,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_BUSY,
                       ZZPLAY_BACKEND_MISSING, ZZPLAY_BACKEND_BUSY_RESULT,
                       ZZPLAY_AUDIO_NONE, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP3, ZZPLAY_AUDIO_MHI,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_MISSING,
                       ZZPLAY_BACKEND_MISSING, ZZPLAY_BACKEND_MISSING_RESULT,
                       ZZPLAY_AUDIO_NONE, 0) ||
      !check_selection(ZZPLAY_MEDIA_AUDIO_MP3, ZZPLAY_AUDIO_NONE,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_FREE,
                       ZZPLAY_BACKEND_FREE, ZZPLAY_BACKEND_OK,
                       ZZPLAY_AUDIO_NONE, 0)) {
    return 0;
  }
  return 1;
}

int main(void)
{
  if (!check_video_preflight()) {
    return 1;
  }
  if (!check_audio_policy()) {
    return 2;
  }
  return 0;
}
