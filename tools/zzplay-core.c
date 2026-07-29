/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-core.h"
#include "zzplay-controls.h"

#include <string.h>

void zzplay_core_init(ZZPlayCore *core)
{
  if (!core) {
    return;
  }
  memset(core, 0, sizeof(*core));
  core->state = ZZPLAY_STATE_PREPARING;
}

int zzplay_core_start(ZZPlayCore *core)
{
  if (!core ||
      (core->state != ZZPLAY_STATE_PREPARING &&
       core->state != ZZPLAY_STATE_PREBUFFERING)) {
    return 0;
  }
  core->state = ZZPLAY_STATE_PLAYING;
  return 1;
}

int zzplay_core_begin_prebuffer(ZZPlayCore *core)
{
  if (!core || core->state != ZZPLAY_STATE_PREPARING) {
    return 0;
  }
  core->state = ZZPLAY_STATE_PREBUFFERING;
  return 1;
}

int zzplay_core_pause(ZZPlayCore *core)
{
  if (!core || core->state != ZZPLAY_STATE_PLAYING) {
    return 0;
  }
  core->state = ZZPLAY_STATE_PAUSED;
  return 1;
}

int zzplay_core_resume(ZZPlayCore *core)
{
  if (!core || core->state != ZZPLAY_STATE_PAUSED) {
    return 0;
  }
  core->state = ZZPLAY_STATE_PLAYING;
  return 1;
}

int zzplay_core_begin_drain(ZZPlayCore *core)
{
  if (!core ||
      (core->state != ZZPLAY_STATE_PLAYING &&
       core->state != ZZPLAY_STATE_PREBUFFERING)) {
    return 0;
  }
  core->state = ZZPLAY_STATE_DRAINING;
  return 1;
}

int zzplay_core_begin_loop(ZZPlayCore *core)
{
  if (!core || core->state != ZZPLAY_STATE_DRAINING) {
    return 0;
  }
  core->state = ZZPLAY_STATE_LOOPING;
  return 1;
}

int zzplay_core_restart_loop(ZZPlayCore *core)
{
  if (!core || core->state != ZZPLAY_STATE_LOOPING) {
    return 0;
  }
  core->state = ZZPLAY_STATE_PREBUFFERING;
  return 1;
}

void zzplay_core_stop(ZZPlayCore *core, ZZPlayStopReason reason)
{
  if (!core || core->state == ZZPLAY_STATE_ERROR) {
    return;
  }
  core->state = ZZPLAY_STATE_STOPPED;
  core->stop_reason = reason;
}

void zzplay_core_fail(ZZPlayCore *core, ZZPlayFailure failure, int status)
{
  if (!core) {
    return;
  }
  core->state = ZZPLAY_STATE_ERROR;
  core->failure = failure;
  core->status = status;
}

int zzplay_core_is_terminal(const ZZPlayCore *core)
{
  return core &&
         (core->state == ZZPLAY_STATE_STOPPED ||
          core->state == ZZPLAY_STATE_ERROR);
}

int zzplay_resource_acquire(ZZPlayResources *resources,
                            ZZPlayResource resource)
{
  uint32_t bit;

  if (!resources || resource < ZZPLAY_RESOURCE_INPUT_FILE ||
      resource >= ZZPLAY_RESOURCE_COUNT) {
    return 0;
  }
  bit = (uint32_t)1U << (unsigned)resource;
  if ((resources->acquired & bit) != 0U) {
    return 0;
  }
  resources->acquired |= bit;
  return 1;
}

int zzplay_resource_is_acquired(const ZZPlayResources *resources,
                                ZZPlayResource resource)
{
  if (!resources || resource < ZZPLAY_RESOURCE_INPUT_FILE ||
      resource >= ZZPLAY_RESOURCE_COUNT) {
    return 0;
  }
  return (resources->acquired &
          ((uint32_t)1U << (unsigned)resource)) != 0U;
}

int zzplay_resources_release_all(ZZPlayResources *resources,
                                 ZZPlayReleaseResource release,
                                 void *user)
{
  int first_error = 0;
  int index;

  if (!resources || !release) {
    return -1;
  }
  for (index = (int)ZZPLAY_RESOURCE_COUNT - 1; index >= 0; index--) {
    ZZPlayResource resource = (ZZPlayResource)index;
    uint32_t bit = (uint32_t)1U << (unsigned)resource;
    int status;

    if ((resources->acquired & bit) == 0U) {
      continue;
    }
    resources->acquired &= ~bit;
    status = release(user, resource);
    if (first_error == 0 && status != 0) {
      first_error = status;
    }
  }
  return first_error;
}

ZZPlayStopReason zzplay_control_stop_reason(int ctrl_c,
                                            int window_close)
{
  if (ctrl_c) {
    return ZZPLAY_STOP_CTRL_C;
  }
  if (window_close) {
    return ZZPLAY_STOP_WINDOW_CLOSE;
  }
  return ZZPLAY_STOP_NONE;
}
