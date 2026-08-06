/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-options.h"

#include <stdint.h>
#include <string.h>

static int zzplay_parse_loop_count(const char *value, uint32_t *count)
{
  uint32_t parsed = 0U;

  if (!value || !*value || !count) {
    return 0;
  }
  while (*value) {
    uint32_t digit;

    if (*value < '0' || *value > '9') {
      return 0;
    }
    digit = (uint32_t)(*value - '0');
    if (parsed > (UINT32_MAX - digit) / 10U) {
      return 0;
    }
    parsed = parsed * 10U + digit;
    value++;
  }
  if (parsed == 0U) {
    return 0;
  }
  *count = parsed;
  return 1;
}

static int zzplay_parse_audio_backend(const char *value,
                                      ZZPlayAudioBackend *backend)
{
  if (strcmp(value, "auto") == 0) {
    *backend = ZZPLAY_AUDIO_AUTO;
  } else if (strcmp(value, "ahi") == 0) {
    *backend = ZZPLAY_AUDIO_AHI;
  } else if (strcmp(value, "mhi") == 0) {
    *backend = ZZPLAY_AUDIO_MHI;
  } else if (strcmp(value, "ax") == 0) {
    *backend = ZZPLAY_AUDIO_AX;
  } else if (strcmp(value, "none") == 0) {
    *backend = ZZPLAY_AUDIO_NONE;
  } else {
    return 0;
  }
  return 1;
}

ZZPlayOptionsResult zzplay_options_parse_cli(
    int argc, char **argv, ZZPlayOptions *options)
{
  int i;
  int audio_explicit = 0;

  if (!options || argc < 1 || !argv) {
    return ZZPLAY_OPTIONS_ERROR;
  }
  memset(options, 0, sizeof(*options));
  options->audio_backend = ZZPLAY_AUDIO_AUTO;
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--fps") == 0) {
      options->show_fps = 1;
    } else if (strcmp(argv[i], "--benchmark") == 0) {
      options->show_fps = 1;
      options->uncapped = 1;
    } else if (strcmp(argv[i], "--loop") == 0) {
      options->loop_mode = ZZPLAY_LOOP_FOREVER;
      options->loop_count = 0U;
    } else if (strncmp(argv[i], "--loop=", 7U) == 0) {
      if (!zzplay_parse_loop_count(
              argv[i] + 7U, &options->loop_count)) {
        return ZZPLAY_OPTIONS_ERROR;
      }
      options->loop_mode = ZZPLAY_LOOP_FINITE;
    } else if (strncmp(argv[i], "--audio=", 8U) == 0) {
      if (!zzplay_parse_audio_backend(
              argv[i] + 8U, &options->audio_backend)) {
        return ZZPLAY_OPTIONS_ERROR;
      }
      audio_explicit = 1;
    } else if (strcmp(argv[i], "--help") == 0) {
      return ZZPLAY_OPTIONS_HELP;
    } else if (!options->path) {
      options->path = argv[i];
    } else {
      return ZZPLAY_OPTIONS_ERROR;
    }
  }
  if (options->uncapped && !audio_explicit) {
    options->audio_backend = ZZPLAY_AUDIO_NONE;
  }
  return options->path ? ZZPLAY_OPTIONS_OK : ZZPLAY_OPTIONS_ERROR;
}
