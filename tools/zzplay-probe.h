/* MPEG sequence-header and input-file probing for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_PROBE_H
#define ZZPLAY_PROBE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define ZZPLAY_MAX_WIDTH 1920U
#define ZZPLAY_MAX_HEIGHT 1080U

typedef struct ZZPlayVideoInfo {
  uint32_t width;
  uint32_t height;
  uint32_t frame_rate_milli;
  int is_program_stream;
  int has_video_pes;
  int has_audio_pes;
} ZZPlayVideoInfo;

uint32_t zzplay_mpeg_frame_rate_milli(uint8_t code);
int zzplay_probe_mpeg_sequence(const uint8_t *data,
                               size_t length,
                               ZZPlayVideoInfo *info);
void zzplay_probe_mpeg_program(const uint8_t *data,
                               size_t length,
                               ZZPlayVideoInfo *info);
int zzplay_probe_file(FILE *file, ZZPlayVideoInfo *info);
int zzplay_video_info_supported(const ZZPlayVideoInfo *info);

#endif /* ZZPLAY_PROBE_H */
