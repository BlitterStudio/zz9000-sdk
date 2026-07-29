/* Playback pacing arithmetic for zzplay.
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_SYNC_H
#define ZZPLAY_SYNC_H

#include <stdint.h>

uint32_t zzplay_frame_period_us(uint32_t frame_rate_milli);
uint32_t zzplay_pacing_wait_us(uint32_t frame_period_us,
                               uint32_t elapsed_us,
                               int uncapped);

#endif /* ZZPLAY_SYNC_H */
