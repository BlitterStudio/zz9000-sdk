/* Player lifecycle and resource ownership for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_CORE_H
#define ZZPLAY_CORE_H

#include <stdint.h>

typedef enum ZZPlayState {
  ZZPLAY_STATE_PREPARING = 0,
  ZZPLAY_STATE_PLAYING,
  ZZPLAY_STATE_STOPPED,
  ZZPLAY_STATE_ERROR
} ZZPlayState;

typedef enum ZZPlayStopReason {
  ZZPLAY_STOP_NONE = 0,
  ZZPLAY_STOP_CTRL_C,
  ZZPLAY_STOP_WINDOW_CLOSE,
  ZZPLAY_STOP_EOF
} ZZPlayStopReason;

typedef enum ZZPlayFailure {
  ZZPLAY_FAILURE_NONE = 0,
  ZZPLAY_FAILURE_INVALID_INPUT,
  ZZPLAY_FAILURE_UNSUPPORTED_BOARD,
  ZZPLAY_FAILURE_P96,
  ZZPLAY_FAILURE_PIP,
  ZZPLAY_FAILURE_SDK,
  ZZPLAY_FAILURE_CAPABILITY,
  ZZPLAY_FAILURE_ALLOCATION,
  ZZPLAY_FAILURE_SESSION,
  ZZPLAY_FAILURE_TIMER,
  ZZPLAY_FAILURE_IO,
  ZZPLAY_FAILURE_PROTOCOL
} ZZPlayFailure;

typedef enum ZZPlayResource {
  ZZPLAY_RESOURCE_INPUT_FILE = 0,
  ZZPLAY_RESOURCE_P96_LIBRARY,
  ZZPLAY_RESOURCE_VIDEO_WINDOW,
  ZZPLAY_RESOURCE_SDK_CONTEXT,
  ZZPLAY_RESOURCE_INPUT_BUFFER,
  ZZPLAY_RESOURCE_VIDEO_SESSION,
  ZZPLAY_RESOURCE_TIMER,
  ZZPLAY_RESOURCE_COUNT
} ZZPlayResource;

typedef struct ZZPlayResources {
  uint32_t acquired;
} ZZPlayResources;

typedef struct ZZPlayCore {
  ZZPlayState state;
  ZZPlayStopReason stop_reason;
  ZZPlayFailure failure;
  int status;
  ZZPlayResources resources;
} ZZPlayCore;

typedef int (*ZZPlayReleaseResource)(void *user,
                                     ZZPlayResource resource);

void zzplay_core_init(ZZPlayCore *core);
int zzplay_core_start(ZZPlayCore *core);
void zzplay_core_stop(ZZPlayCore *core, ZZPlayStopReason reason);
void zzplay_core_fail(ZZPlayCore *core, ZZPlayFailure failure, int status);
int zzplay_core_is_terminal(const ZZPlayCore *core);
int zzplay_resource_acquire(ZZPlayResources *resources,
                            ZZPlayResource resource);
int zzplay_resource_is_acquired(const ZZPlayResources *resources,
                                ZZPlayResource resource);
int zzplay_resources_release_all(ZZPlayResources *resources,
                                 ZZPlayReleaseResource release,
                                 void *user);

#endif /* ZZPLAY_CORE_H */
