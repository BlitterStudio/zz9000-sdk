/* Streaming-input helpers shared by zzplay and its native regression test.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_STREAM_H
#define ZZPLAY_STREAM_H

#include "zz9k/abi.h"

#include <stdint.h>

#define ZZPLAY_REQUIRED_VIDEO_FLAGS                                      \
  (ZZ9K_SERVICE_FLAG_VIDEO_MPEG1 | ZZ9K_SERVICE_FLAG_VIDEO_MPEG_PS |    \
   ZZ9K_SERVICE_FLAG_VIDEO_DIRECT_OVERLAY |                              \
   ZZ9K_SERVICE_FLAG_VIDEO_STREAMING_INPUT)

static inline int zzplay_video_backend_available(uint32_t flags)
{
  return (flags & ZZPLAY_REQUIRED_VIDEO_FLAGS) ==
         ZZPLAY_REQUIRED_VIDEO_FLAGS;
}

static inline int zzplay_advance_input(uint32_t *offset,
                                       uint32_t *remaining,
                                       uint32_t *accepted_total,
                                       uint32_t reported_total)
{
  uint32_t accepted;

  /* The firmware reports a session-cumulative total, so advance this shared
   * buffer range by only the bytes accepted by the latest WRITE. */
  if (!offset || !remaining || !accepted_total ||
      reported_total < *accepted_total) {
    return 0;
  }
  accepted = reported_total - *accepted_total;
  if (accepted > *remaining) {
    return 0;
  }
  *offset += accepted;
  *remaining -= accepted;
  *accepted_total = reported_total;
  return 1;
}

#endif /* ZZPLAY_STREAM_H */
