/* First-class standalone MP3 playback for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_MP3_H
#define ZZPLAY_MP3_H

#include "zzplay-options.h"
#include "zzplay-probe.h"

typedef int (*ZZPlayMP3StopRequested)(void *user);

int zzplay_mp3_run(const char *path,
                   const ZZPlayMP3Info *info,
                   const ZZPlayOptions *options,
                   ZZPlayMP3StopRequested stop_requested,
                   void *user);

#endif /* ZZPLAY_MP3_H */
