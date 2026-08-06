/* Host-testable input normalization for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_CONTROLS_H
#define ZZPLAY_CONTROLS_H

#include "zzplay-core.h"

typedef enum ZZPlayControlAction {
  ZZPLAY_CONTROL_NONE = 0,
  ZZPLAY_CONTROL_TOGGLE_PAUSE,
  ZZPLAY_CONTROL_STOP_CTRL_C,
  ZZPLAY_CONTROL_STOP_WINDOW
} ZZPlayControlAction;

ZZPlayControlAction zzplay_control_action(int ctrl_c,
                                          int window_close,
                                          int toggle_pause);
ZZPlayStopReason zzplay_control_stop_reason_from_action(
    ZZPlayControlAction action);

#endif /* ZZPLAY_CONTROLS_H */
