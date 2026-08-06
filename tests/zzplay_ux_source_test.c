/*
 * Structural guards for the U6 runtime UX. These paths are Amiga-only, so
 * they cannot be exercised on the host; what is checked here is that the
 * ordering and cleanup invariants they depend on are still present.
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
  if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) < 0L ||
      fseek(file, 0L, SEEK_SET) != 0) {
    fclose(file);
    return 0;
  }
  source = (char *)malloc((size_t)length + 1U);
  if (!source) {
    fclose(file);
    return 0;
  }
  if (fread(source, 1U, (size_t)length, file) != (size_t)length) {
    free(source);
    fclose(file);
    return 0;
  }
  source[length] = '\0';
  fclose(file);
  return source;
}

/* Offset of `needle` within `haystack`, or -1. */
static long find(const char *haystack, const char *needle)
{
  const char *hit = strstr(haystack, needle);

  return hit ? (long)(hit - haystack) : -1L;
}

static int check_player(const char *path)
{
  char *s = read_file(path);
  long remember;
  long close_pip;
  long reopen;
  int rc = 0;

  if (!s) {
    printf("cannot read %s\n", path);
    return 1;
  }
  /* The fullscreen toggle must save the windowed placement BEFORE closing
   * the window, or the geometry it reads back is already gone. */
  remember = find(s, "zzplay_remember_window(runtime);\n  zzplay_close_pip");
  if (remember < 0L) { rc = 2; goto done; }

  /* Closing must go through the resource stack, not p96PIP_Close directly,
   * so the window is never double-released at cleanup. */
  close_pip = find(s, "static void zzplay_close_pip");
  if (close_pip < 0L) { rc = 3; goto done; }
  if (find(s + close_pip, "zzplay_resource_release") < 0L) { rc = 4; goto done; }

  /* The path report must come from the firmware page, never be inferred. */
  if (find(s, "ZZ9K_MEDIA_STATUS_PRESENTATION") < 0L) { rc = 5; goto done; }
  if (find(s, "zzplay_present_from_status") < 0L) { rc = 6; goto done; }

  /* The status window must be closed on the MP3 return path. */
  reopen = find(s, "zzplay_statuswin_open");
  if (reopen < 0L) { rc = 7; goto done; }
  if (find(s, "zzplay_statuswin_close") < 0L) { rc = 8; goto done; }

  /* Every launch path must unwind; a bare return before zzplay_launch_end
   * would leak the icon/ASL/Intuition handles. */
  if (find(s, "zzplay_launch_end(&launch);\n    return mp3_ok") < 0L) {
    rc = 9;
    goto done;
  }

  /* The screen must be read while the window that describes it still
   * exists. Reading it after the close made the first bench round go
   * borderless at the source size instead of scaling to the display. */
  if (find(s, "zzplay_cache_screen(runtime);\n  zzplay_remember_window") <
      0L) {
    rc = 11;
    goto done;
  }
  /* And a fullscreen request with no known screen must say so rather than
   * silently opening a borderless source-size window. */
  if (find(s, "staying windowed") < 0L) { rc = 12; goto done; }

  /* Fullscreen must not paint a title bar it does not have. */
  if (find(s, "runtime->fullscreen) {\n    runtime->title_dirty = 0U;") < 0L) {
    rc = 13;
    goto done;
  }

done:
  free(s);
  return rc;
}

static int check_mp3(const char *path)
{
  char *s = read_file(path);
  int rc = 0;

  if (!s) {
    printf("cannot read %s\n", path);
    return 1;
  }
  /* Pause must keep servicing AHI, otherwise completed buffers are never
   * reaped and resuming stalls. */
  if (find(s, "while (zzplay_mp3_is_paused(decode))") < 0L) {
    rc = 2;
    goto done;
  }
  if (find(s, "zzplay_ahi_poll(decode->ahi)") < 0L) { rc = 3; goto done; }
  /* A stop request must still be honoured while paused, or Ctrl-C hangs. */
  if (find(s, "while (zzplay_mp3_is_paused(decode)) {\n      if "
              "(zzplay_mp3_should_stop(decode)) {") < 0L) {
    rc = 4;
    goto done;
  }

done:
  free(s);
  return rc;
}

static int check_mhi(const char *path)
{
  char *s = read_file(path);
  int rc = 0;

  if (!s) {
    printf("cannot read %s\n", path);
    return 1;
  }
  /* MHI has a real pause primitive; the MP3 path must use it rather than
   * merely withholding input. */
  if (find(s, "zzplay_mhi_pause(sink)") < 0L) { rc = 2; goto done; }
  if (find(s, "zzplay_mhi_resume(sink)") < 0L) { rc = 3; goto done; }
  /* While paused it must not keep queueing buffers. */
  if (find(s, "if (sink->paused) {") < 0L) { rc = 4; goto done; }

done:
  free(s);
  return rc;
}

int main(int argc, char **argv)
{
  int rc;

  if (argc != 4) {
    printf("usage: %s <zzplay.c> <zzplay-mp3.c> <zzplay-mhi.c>\n", argv[0]);
    return 2;
  }
  rc = check_player(argv[1]);
  if (rc != 0) { printf("player %d\n", rc); return 20 + rc; }
  rc = check_mp3(argv[2]);
  if (rc != 0) { printf("mp3 %d\n", rc); return 50 + rc; }
  rc = check_mhi(argv[3]);
  if (rc != 0) { printf("mhi %d\n", rc); return 80 + rc; }
  printf("zzplay_ux_source_test: all checks passed\n");
  return 0;
}
