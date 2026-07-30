/* Command-line options for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_OPTIONS_H
#define ZZPLAY_OPTIONS_H

#include "zzplay-audio.h"

typedef enum ZZPlayOptionsResult {
  ZZPLAY_OPTIONS_OK = 0,
  ZZPLAY_OPTIONS_HELP,
  ZZPLAY_OPTIONS_ERROR
} ZZPlayOptionsResult;

typedef enum ZZPlayLoopMode {
  ZZPLAY_LOOP_NONE = 0,
  ZZPLAY_LOOP_FINITE,
  ZZPLAY_LOOP_FOREVER
} ZZPlayLoopMode;

typedef struct ZZPlayOptions {
  const char *path;
  ZZPlayAudioBackend audio_backend;
  uint32_t loop_count;
  ZZPlayLoopMode loop_mode;
  int show_fps;
  int uncapped;
} ZZPlayOptions;

ZZPlayOptionsResult zzplay_options_parse_cli(
    int argc, char **argv, ZZPlayOptions *options);

#endif /* ZZPLAY_OPTIONS_H */
