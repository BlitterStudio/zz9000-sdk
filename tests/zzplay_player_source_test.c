/*
 * Source guard for opening the P96 PIP window at first presentation.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
  FILE *file;
  long length;
  char *source;

  file = fopen(path, "rb");
  if (!file) {
    return 0;
  }
  if (fseek(file, 0L, SEEK_END) != 0 ||
      (length = ftell(file)) < 0L ||
      fseek(file, 0L, SEEK_SET) != 0) {
    fclose(file);
    return 0;
  }
  source = (char *)malloc((size_t)length + 1U);
  if (!source) {
    fclose(file);
    return 0;
  }
  if (fread(source, 1U, (size_t)length, file) !=
      (size_t)length) {
    free(source);
    fclose(file);
    return 0;
  }
  source[length] = '\0';
  fclose(file);
  return source;
}

int main(int argc, char **argv)
{
  char *source;
  char *retire;
  char *start_ensure;
  char *present_ensure;
  char *present;
  int ok;

  if (argc != 2) {
    return 2;
  }
  source = read_file(argv[1]);
  if (!source) {
    return 2;
  }
  retire = strstr(source, "static int zzplay_retire_held_frame(");
  start_ensure =
      retire ? strstr(retire, "zzplay_ensure_pip(runtime)") : 0;
  present_ensure =
      start_ensure
          ? strstr(start_ensure + 1, "zzplay_ensure_pip(runtime)")
          : 0;
  present = present_ensure
                ? strstr(present_ensure, "zz9k_media_session_present(")
                : 0;
  ok = retire && start_ensure && present_ensure && present &&
       strstr(source, "runtime.video_info = info;") &&
       strstr(source, "signal(SIGINT, zzplay_sigint_handler)") &&
       strstr(source, "SetSignal(0L, SIGBREAKF_CTRL_C)") &&
       strstr(source, "IDCMP_VANILLAKEY") &&
       strstr(source, "zzplay_audio_pause(runtime)") &&
       strstr(source, "zzplay_audio_resume(runtime)") &&
       strstr(source, "zzplay_core_pause(&runtime->core)") &&
       strstr(source, "zzplay_core_resume(&runtime->core)") &&
       strstr(source, "zzplay_restart_session(&runtime") &&
       strstr(source, "zzplay_resource_release(") &&
       !strstr(source, "zzplay_open_pip(&info");
  if (!ok) {
    printf("P96 first-frame and lifecycle wiring is incomplete\n");
  }
  free(source);
  return ok ? 0 : 1;
}
