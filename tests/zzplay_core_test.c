/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "../tools/zzplay-controls.h"
#include "../tools/zzplay-core.h"
#include "../tools/zzplay-options.h"
#include "../tools/zzplay-sync.h"

#include <stddef.h>
#include <stdint.h>

struct ReleaseLog {
  ZZPlayResource entries[ZZPLAY_RESOURCE_COUNT];
  unsigned count;
};

static int record_release(void *user, ZZPlayResource resource)
{
  struct ReleaseLog *log = (struct ReleaseLog *)user;

  if (!log || log->count >= ZZPLAY_RESOURCE_COUNT) {
    return -1;
  }
  log->entries[log->count++] = resource;
  return 0;
}

static int check_options(void)
{
  char *fps_argv[] = {
    (char *)"zzplay", (char *)"--fps", (char *)"movie.mpg"
  };
  char *benchmark_argv[] = {
    (char *)"zzplay", (char *)"--benchmark", (char *)"movie.mpg"
  };
  char *help_argv[] = {
    (char *)"zzplay", (char *)"--help"
  };
  char *audio_argv[] = {
    (char *)"zzplay", (char *)"--audio=ahi", (char *)"movie.mpg"
  };
  char *extra_argv[] = {
    (char *)"zzplay", (char *)"one.mpg", (char *)"two.mpg"
  };
  char *missing_argv[] = {
    (char *)"zzplay"
  };
  ZZPlayOptions options;

  if (zzplay_options_parse_cli(3, fps_argv, &options) !=
          ZZPLAY_OPTIONS_OK ||
      !options.show_fps || options.uncapped ||
      options.audio_backend != ZZPLAY_AUDIO_AUTO ||
      options.path != fps_argv[2]) {
    return 0;
  }
  if (zzplay_options_parse_cli(3, benchmark_argv, &options) !=
          ZZPLAY_OPTIONS_OK ||
      !options.show_fps || !options.uncapped ||
      options.audio_backend != ZZPLAY_AUDIO_NONE ||
      options.path != benchmark_argv[2]) {
    return 0;
  }
  if (zzplay_options_parse_cli(3, audio_argv, &options) !=
          ZZPLAY_OPTIONS_OK ||
      options.audio_backend != ZZPLAY_AUDIO_AHI ||
      options.path != audio_argv[2]) {
    return 0;
  }
  if (zzplay_options_parse_cli(2, help_argv, &options) !=
      ZZPLAY_OPTIONS_HELP) {
    return 0;
  }
  if (zzplay_options_parse_cli(3, extra_argv, &options) !=
          ZZPLAY_OPTIONS_ERROR ||
      zzplay_options_parse_cli(1, missing_argv, &options) !=
          ZZPLAY_OPTIONS_ERROR) {
    return 0;
  }
  return 1;
}

static int check_terminal_states(void)
{
  ZZPlayCore core;

  zzplay_core_init(&core);
  if (core.state != ZZPLAY_STATE_PREPARING ||
      zzplay_core_is_terminal(&core)) {
    return 0;
  }
  if (!zzplay_core_start(&core) ||
      core.state != ZZPLAY_STATE_PLAYING) {
    return 0;
  }
  zzplay_core_stop(&core, ZZPLAY_STOP_CTRL_C);
  if (core.state != ZZPLAY_STATE_STOPPED ||
      core.stop_reason != ZZPLAY_STOP_CTRL_C ||
      !zzplay_core_is_terminal(&core)) {
    return 0;
  }

  zzplay_core_init(&core);
  if (!zzplay_core_start(&core)) {
    return 0;
  }
  zzplay_core_stop(&core, ZZPLAY_STOP_WINDOW_CLOSE);
  if (core.state != ZZPLAY_STATE_STOPPED ||
      core.stop_reason != ZZPLAY_STOP_WINDOW_CLOSE) {
    return 0;
  }

  zzplay_core_init(&core);
  if (!zzplay_core_start(&core)) {
    return 0;
  }
  zzplay_core_stop(&core, ZZPLAY_STOP_EOF);
  if (core.state != ZZPLAY_STATE_STOPPED ||
      core.stop_reason != ZZPLAY_STOP_EOF) {
    return 0;
  }

  zzplay_core_init(&core);
  zzplay_core_fail(&core, ZZPLAY_FAILURE_ALLOCATION, 17);
  if (core.state != ZZPLAY_STATE_ERROR ||
      core.failure != ZZPLAY_FAILURE_ALLOCATION ||
      core.status != 17 ||
      !zzplay_core_is_terminal(&core)) {
    return 0;
  }
  return 1;
}

static int check_extended_lifecycle(void)
{
  ZZPlayCore core;

  zzplay_core_init(&core);
  if (!zzplay_core_begin_prebuffer(&core) ||
      core.state != ZZPLAY_STATE_PREBUFFERING ||
      !zzplay_core_start(&core) ||
      !zzplay_core_pause(&core) ||
      core.state != ZZPLAY_STATE_PAUSED ||
      !zzplay_core_resume(&core) ||
      !zzplay_core_begin_drain(&core) ||
      core.state != ZZPLAY_STATE_DRAINING ||
      !zzplay_core_begin_loop(&core) ||
      !zzplay_core_restart_loop(&core) ||
      core.state != ZZPLAY_STATE_PREBUFFERING) {
    return 0;
  }
  return 1;
}

static int check_controls_and_sync(void)
{
  if (zzplay_control_stop_reason(0, 0) != ZZPLAY_STOP_NONE ||
      zzplay_control_stop_reason(1, 0) != ZZPLAY_STOP_CTRL_C ||
      zzplay_control_stop_reason(0, 1) !=
          ZZPLAY_STOP_WINDOW_CLOSE ||
      zzplay_control_stop_reason(1, 1) != ZZPLAY_STOP_CTRL_C) {
    return 0;
  }
  if (zzplay_frame_period_us(25000U) != 40000U ||
      zzplay_frame_period_us(0U) != 0U ||
      zzplay_pacing_wait_us(40000U, 10000U, 0) != 30000U ||
      zzplay_pacing_wait_us(40000U, 50000U, 0) != 0U ||
      zzplay_pacing_wait_us(40000U, 10000U, 1) != 0U) {
    return 0;
  }
  return 1;
}

static int check_cleanup(void)
{
  ZZPlayCore core;
  struct ReleaseLog log;
  unsigned acquired;

  for (acquired = 0U; acquired <= ZZPLAY_RESOURCE_COUNT; acquired++) {
    unsigned i;

    zzplay_core_init(&core);
    log.count = 0U;
    for (i = 0U; i < acquired; i++) {
      if (!zzplay_resource_acquire(&core.resources,
                                   (ZZPlayResource)i)) {
        return 0;
      }
    }
    if (acquired != ZZPLAY_RESOURCE_COUNT) {
      zzplay_core_fail(&core, ZZPLAY_FAILURE_ALLOCATION, 20);
    }
    if (zzplay_resources_release_all(&core.resources,
                                     record_release, &log) != 0 ||
        log.count != acquired) {
      return 0;
    }
    for (i = 0U; i < acquired; i++) {
      if (log.entries[i] !=
          (ZZPlayResource)(acquired - i - 1U)) {
        return 0;
      }
    }
    if (zzplay_resources_release_all(&core.resources,
                                     record_release, &log) != 0 ||
        log.count != acquired) {
      return 0;
    }
  }
  return 1;
}

int main(void)
{
  if (!check_options()) {
    return 1;
  }
  if (!check_terminal_states()) {
    return 2;
  }
  if (!check_controls_and_sync()) {
    return 3;
  }
  if (!check_cleanup()) {
    return 4;
  }
  if (!check_extended_lifecycle()) {
    return 5;
  }
  return 0;
}
