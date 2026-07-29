/* Host-testable input normalization for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_CONTROLS_H
#define ZZPLAY_CONTROLS_H

#include "zzplay-core.h"

ZZPlayStopReason zzplay_control_stop_reason(int ctrl_c,
                                            int window_close);

#endif /* ZZPLAY_CONTROLS_H */
