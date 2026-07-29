/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-options.h"

#include <string.h>

ZZPlayOptionsResult zzplay_options_parse_cli(
    int argc, char **argv, ZZPlayOptions *options)
{
  int i;

  if (!options || argc < 1 || !argv) {
    return ZZPLAY_OPTIONS_ERROR;
  }
  memset(options, 0, sizeof(*options));
  options->audio_backend = ZZPLAY_AUDIO_NONE;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--fps") == 0) {
      options->show_fps = 1;
    } else if (strcmp(argv[i], "--benchmark") == 0) {
      options->show_fps = 1;
      options->uncapped = 1;
    } else if (strcmp(argv[i], "--help") == 0) {
      return ZZPLAY_OPTIONS_HELP;
    } else if (!options->path) {
      options->path = argv[i];
    } else {
      return ZZPLAY_OPTIONS_ERROR;
    }
  }
  return options->path ? ZZPLAY_OPTIONS_OK : ZZPLAY_OPTIONS_ERROR;
}
