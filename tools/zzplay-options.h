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

typedef struct ZZPlayOptions {
  const char *path;
  ZZPlayAudioBackend audio_backend;
  int show_fps;
  int uncapped;
} ZZPlayOptions;

ZZPlayOptionsResult zzplay_options_parse_cli(
    int argc, char **argv, ZZPlayOptions *options);

#endif /* ZZPLAY_OPTIONS_H */
