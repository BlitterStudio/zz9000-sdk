/* Small Intuition status window for playback with no video (U6, D3).
 *
 * Standalone MP3 has no PIP window, so U5 had to defer pause/resume: there
 * was nowhere for a keypress to land. This gives that path the same control
 * surface and key bindings as the video window.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef ZZPLAY_STATUSWIN_H
#define ZZPLAY_STATUSWIN_H

#include "zzplay-controls.h"

typedef struct ZZPlayStatusWindow {
  void *window;        /* struct Window * */
  char backend[24];
  char title[128];
  int paused;
  int stop;
  int loop;
  unsigned long ticks;
} ZZPlayStatusWindow;

/* Opens on the default public screen. Returns 0 if a window could not be
 * opened; callers must still work headless in that case. */
int zzplay_statuswin_open(ZZPlayStatusWindow *status, const char *path);
void zzplay_statuswin_close(ZZPlayStatusWindow *status);

/* Drain input and update internal state. Also honours Ctrl-C so a shell
 * launch behaves identically with or without the window. */
ZZPlayControlAction zzplay_statuswin_poll(ZZPlayStatusWindow *status);

void zzplay_statuswin_set_backend(ZZPlayStatusWindow *status,
                                  const char *backend);

/* The ZZPlayMP3Controls callbacks live in zzplay.c so they can also consult
 * the SIGINT flag libnix raises for a Ctrl-C taken during I/O. */

#endif /* ZZPLAY_STATUSWIN_H */
