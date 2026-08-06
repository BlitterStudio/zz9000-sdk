/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "zzplay-statuswin.h"

#include <exec/types.h>
#include <intuition/intuition.h>

#include <proto/exec.h>
#include <proto/intuition.h>

#include <stdio.h>
#include <string.h>

/* Keep only the final path component so the title stays readable. */
static const char *zzplay_statuswin_basename(const char *path)
{
  const char *last = path;
  const char *scan;

  if (!path) {
    return "";
  }
  for (scan = path; *scan; scan++) {
    if (*scan == '/' || *scan == ':') {
      last = scan + 1;
    }
  }
  return last;
}

static void zzplay_statuswin_refresh(ZZPlayStatusWindow *status)
{
  struct Window *window = (struct Window *)status->window;
  char previous[sizeof(status->title)];

  if (!window) {
    return;
  }
  strcpy(previous, status->title);
  sprintf(status->title, "zzplay - %s - %s%s",
          status->backend[0] ? status->backend : "starting",
          status->paused ? "paused" : "playing",
          status->loop ? " - loop" : "");
  if (strcmp(previous, status->title) != 0) {
    SetWindowTitles(window, (CONST_STRPTR)status->title,
                    (CONST_STRPTR)~0UL);
  }
}

int zzplay_statuswin_open(ZZPlayStatusWindow *status, const char *path)
{
  struct Window *window;
  const char *name;

  if (!status) {
    return 0;
  }
  memset(status, 0, sizeof(*status));
  if (!IntuitionBase) {
    return 0;
  }
  name = zzplay_statuswin_basename(path);
  sprintf(status->title, "zzplay - %.60s", name);
  window = OpenWindowTags(
      0,
      WA_Title, (ULONG)status->title,
      WA_InnerWidth, 320UL,
      WA_InnerHeight, 12UL,
      WA_PubScreenName, (ULONG)"Workbench",
      WA_Activate, TRUE,
      WA_DragBar, TRUE,
      WA_CloseGadget, TRUE,
      WA_DepthGadget, TRUE,
      WA_SimpleRefresh, TRUE,
      WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_VANILLAKEY,
      TAG_DONE);
  if (!window) {
    return 0;
  }
  status->window = window;
  return 1;
}

void zzplay_statuswin_close(ZZPlayStatusWindow *status)
{
  if (!status || !status->window) {
    return;
  }
  CloseWindow((struct Window *)status->window);
  status->window = 0;
}

ZZPlayControlAction zzplay_statuswin_poll(ZZPlayStatusWindow *status)
{
  struct Window *window;
  struct IntuiMessage *message;
  ZZPlayControlInput input;
  ZZPlayControlAction action;

  if (!status) {
    return ZZPLAY_CONTROL_NONE;
  }
  memset(&input, 0, sizeof(input));
  /* Consume the break here for the same reason the video path does: a
   * pending CTRL-C would otherwise surface inside a later stdio call and
   * bypass the normal teardown. */
  input.ctrl_c = (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0U;
  window = (struct Window *)status->window;
  while (window &&
         (message = (struct IntuiMessage *)GetMsg(window->UserPort))) {
    if (message->Class == IDCMP_CLOSEWINDOW) {
      input.window_close = 1;
    } else if (message->Class == IDCMP_VANILLAKEY) {
      ZZPlayControlAction key_action =
          zzplay_control_action_from_key((unsigned)message->Code);

      if (key_action != ZZPLAY_CONTROL_NONE && input.key == 0U) {
        input.key = (unsigned)message->Code;
      }
    }
    ReplyMsg((struct Message *)message);
  }
  action = zzplay_control_resolve(&input);
  if (zzplay_control_is_stop(action)) {
    status->stop = 1;
  } else if (action == ZZPLAY_CONTROL_TOGGLE_PAUSE) {
    status->paused = !status->paused;
  } else if (action == ZZPLAY_CONTROL_TOGGLE_LOOP) {
    status->loop = !status->loop;
  }
  /* Fullscreen is meaningless without video; ignore it rather than
   * pretending it did something. */
  zzplay_statuswin_refresh(status);
  return action;
}

void zzplay_statuswin_set_backend(ZZPlayStatusWindow *status,
                                  const char *backend)
{
  if (!status || !backend) {
    return;
  }
  strncpy(status->backend, backend, sizeof(status->backend) - 1U);
  status->backend[sizeof(status->backend) - 1U] = '\0';
  zzplay_statuswin_refresh(status);
}
