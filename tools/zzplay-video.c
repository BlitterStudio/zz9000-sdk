/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-video.h"

ZZPlayVideoSinkStatus zzplay_video_sink_check(
    int board_found, uint32_t zorro_version,
    int p96_available, int pip_available)
{
  if (!board_found || zorro_version != 3U) {
    return ZZPLAY_VIDEO_SINK_UNSUPPORTED_BOARD;
  }
  if (!p96_available) {
    return ZZPLAY_VIDEO_SINK_P96_UNAVAILABLE;
  }
  if (!pip_available) {
    return ZZPLAY_VIDEO_SINK_PIP_UNAVAILABLE;
  }
  return ZZPLAY_VIDEO_SINK_READY;
}

int zzplay_video_backend_available(uint32_t flags)
{
  return (flags & ZZPLAY_REQUIRED_VIDEO_FLAGS) ==
         ZZPLAY_REQUIRED_VIDEO_FLAGS;
}

int zzplay_video_result_has_frame(uint32_t flags)
{
  return (flags & ZZ9K_VIDEO_SESSION_RESULT_FRAME_READY) != 0U;
}

ZZPlayVideoResultAction zzplay_video_result_action(uint32_t flags)
{
  if ((flags & ZZ9K_VIDEO_SESSION_RESULT_DONE) != 0U) {
    return ZZPLAY_VIDEO_RESULT_DONE;
  }
  if ((flags & ZZ9K_VIDEO_SESSION_RESULT_NEED_INPUT) != 0U) {
    return ZZPLAY_VIDEO_RESULT_NEED_INPUT;
  }
  if ((flags & (ZZ9K_VIDEO_SESSION_RESULT_FRAME_READY |
                ZZ9K_VIDEO_SESSION_RESULT_HEADER_READY)) != 0U) {
    return ZZPLAY_VIDEO_RESULT_CONTINUE;
  }
  return ZZPLAY_VIDEO_RESULT_INVALID;
}
