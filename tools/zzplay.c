/*
 * zzplay: first standalone client for the ZZ9000 streaming video service.
 *
 * MPEG-1 Program Stream is the first backend, but all mailbox interaction is
 * expressed as codec/container/output descriptors so later backends do not
 * require a new player protocol.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k/sdk.h"
#include "zzplay-probe.h"
#include "zzplay-stats.h"

#include <devices/timer.h>
#include <exec/libraries.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <libraries/Picasso96.h>
#include <proto/exec.h>
#include <proto/Picasso96.h>
#include <proto/timer.h>
#include <utility/tagitem.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZZPLAY_INPUT_BYTES (64U * 1024U)
#define ZZPLAY_PROBE_BYTES (256U * 1024U)
#define ZZPLAY_MAX_WIDTH 1920U
#define ZZPLAY_MAX_HEIGHT 1080U
#define ZZPLAY_FPS_REPORT_US 2000000U

struct Library *P96Base;
struct Device *TimerBase;

static const char zzplay_version[] = "$VER: zzplay 0.2 (10.07.2026)";

struct ZZPlayTimer {
  struct MsgPort *port;
  struct timerequest *request;
};

struct ZZPlayStats {
  TimeVal_Type last_sample;
  TimeVal_Type report_started;
  uint64_t wall_us;
  uint64_t decode_us;
  uint64_t report_decode_us;
  uint32_t report_frames;
};

static uint32_t zzplay_elapsed_us(const TimeVal_Type *start,
                                  const TimeVal_Type *end);

static void zzplay_usage(FILE *stream)
{
  fprintf(stream,
          "%s\n"
          "Usage: zzplay [--fps|--benchmark] <mpeg1-program-stream>\n"
          "  --fps        rolling paced-playback and decode-call FPS\n"
          "  --benchmark  disable pacing and report uncapped throughput\n",
          zzplay_version + 6);
}

static void zzplay_print_fps(const char *label, uint32_t playback_milli,
                             uint32_t decode_milli)
{
  printf("zzplay: %s %lu.%03lu fps playback, "
         "%lu.%03lu fps decode-call\n",
         label,
         (unsigned long)(playback_milli / 1000U),
         (unsigned long)(playback_milli % 1000U),
         (unsigned long)(decode_milli / 1000U),
         (unsigned long)(decode_milli % 1000U));
}

static void zzplay_stats_start(struct ZZPlayStats *stats)
{
  memset(stats, 0, sizeof(*stats));
  GetSysTime(&stats->last_sample);
  stats->report_started = stats->last_sample;
}

static void zzplay_stats_frame(struct ZZPlayStats *stats,
                               uint32_t decode_us)
{
  TimeVal_Type now;
  uint32_t sample_us;
  uint32_t report_us;

  GetSysTime(&now);
  sample_us = zzplay_elapsed_us(&stats->last_sample, &now);
  stats->wall_us += sample_us;
  stats->decode_us += decode_us;
  stats->report_decode_us += decode_us;
  stats->report_frames++;
  stats->last_sample = now;

  report_us = zzplay_elapsed_us(&stats->report_started, &now);
  if (report_us >= ZZPLAY_FPS_REPORT_US) {
    zzplay_print_fps("current",
                     zzplay_fps_milli(stats->report_frames, report_us),
                     zzplay_fps_milli(stats->report_frames,
                                      stats->report_decode_us));
    stats->report_started = now;
    stats->report_decode_us = 0U;
    stats->report_frames = 0U;
  }
}

static void zzplay_stats_finish(const struct ZZPlayStats *stats,
                                uint32_t frames)
{
  if (frames == 0U || stats->wall_us == 0U) {
    printf("zzplay: average fps unavailable\n");
    return;
  }
  zzplay_print_fps("average", zzplay_fps_milli(frames, stats->wall_us),
                   zzplay_fps_milli(frames, stats->decode_us));
}

static int zzplay_timer_open(struct ZZPlayTimer *timer)
{
  memset(timer, 0, sizeof(*timer));
  timer->port = CreateMsgPort();
  if (!timer->port) {
    return 0;
  }
  timer->request = (struct timerequest *)CreateIORequest(
      timer->port, sizeof(*timer->request));
  if (!timer->request) {
    DeleteMsgPort(timer->port);
    timer->port = 0;
    return 0;
  }
  if (OpenDevice(TIMERNAME, UNIT_MICROHZ,
                 (struct IORequest *)timer->request, 0) != 0) {
    DeleteIORequest((struct IORequest *)timer->request);
    DeleteMsgPort(timer->port);
    memset(timer, 0, sizeof(*timer));
    return 0;
  }
  TimerBase = (struct Device *)timer->request->tr_node.io_Device;
  return 1;
}

static void zzplay_timer_close(struct ZZPlayTimer *timer)
{
  if (!timer) {
    return;
  }
  if (timer->request) {
    CloseDevice((struct IORequest *)timer->request);
    DeleteIORequest((struct IORequest *)timer->request);
  }
  if (timer->port) {
    DeleteMsgPort(timer->port);
  }
  memset(timer, 0, sizeof(*timer));
  TimerBase = 0;
}

static void zzplay_wait_us(struct ZZPlayTimer *timer, uint32_t usec)
{
  if (!timer || !timer->request || usec == 0U) {
    return;
  }
  timer->request->tr_node.io_Command = TR_ADDREQUEST;
  timer->request->tr_time.tv_secs = usec / 1000000U;
  timer->request->tr_time.tv_micro = usec % 1000000U;
  DoIO((struct IORequest *)timer->request);
}

static uint32_t zzplay_elapsed_us(const TimeVal_Type *start,
                                  const TimeVal_Type *end)
{
  uint32_t seconds;
  int32_t micros;

  if (end->tv_secs < start->tv_secs) {
    return 0U;
  }
  seconds = end->tv_secs - start->tv_secs;
  micros = (int32_t)end->tv_micro - (int32_t)start->tv_micro;
  if (micros < 0) {
    if (seconds == 0U) {
      return 0U;
    }
    seconds--;
    micros += 1000000L;
  }
  if (seconds > 4294U) {
    return 0xffffffffU;
  }
  return seconds * 1000000U + (uint32_t)micros;
}

static int zzplay_probe_file(FILE *file, ZZPlayVideoInfo *info)
{
  uint8_t *buffer;
  size_t carry = 0U;
  size_t total = 0U;
  int found = 0;

  buffer = (uint8_t *)malloc(4096U + 7U);
  if (!buffer) {
    return 0;
  }
  while (total < ZZPLAY_PROBE_BYTES) {
    size_t want = 4096U;
    size_t got;
    size_t available;

    if (want > ZZPLAY_PROBE_BYTES - total) {
      want = ZZPLAY_PROBE_BYTES - total;
    }
    got = fread(buffer + carry, 1U, want, file);
    available = carry + got;
    if (zzplay_probe_mpeg_sequence(buffer, available, info)) {
      found = 1;
      break;
    }
    total += got;
    if (got == 0U) {
      break;
    }
    carry = available < 7U ? available : 7U;
    memmove(buffer, buffer + available - carry, carry);
  }
  free(buffer);
  if (fseek(file, 0L, SEEK_SET) != 0) {
    return 0;
  }
  clearerr(file);
  return found;
}

static int zzplay_window_stop(struct Window *window)
{
  struct IntuiMessage *message;
  int stop = 0;

  if ((SetSignal(0L, 0L) & SIGBREAKF_CTRL_C) != 0U) {
    stop = 1;
  }
  while (window && (message = (struct IntuiMessage *)GetMsg(window->UserPort))) {
    if (message->Class == IDCMP_CLOSEWINDOW) {
      stop = 1;
    }
    ReplyMsg((struct Message *)message);
  }
  return stop;
}

static struct Window *zzplay_open_pip(const ZZPlayVideoInfo *info,
                                      struct BitMap **bitmap,
                                      LONG *pip_error)
{
  struct TagItem open_tags[16];
  struct TagItem get_tags[2];
  struct Window *window;
  ULONG bitmap_value = 0U;
  unsigned i = 0U;

  *bitmap = 0;
  *pip_error = 0;
  open_tags[i].ti_Tag = P96PIP_SourceFormat;
  open_tags[i++].ti_Data = RGBFB_YUV422CGX;
  open_tags[i].ti_Tag = P96PIP_SourceWidth;
  open_tags[i++].ti_Data = info->width;
  open_tags[i].ti_Tag = P96PIP_SourceHeight;
  open_tags[i++].ti_Data = info->height;
  open_tags[i].ti_Tag = P96PIP_Type;
  open_tags[i++].ti_Data = P96PIPT_MemoryWindow;
  open_tags[i].ti_Tag = P96PIP_ErrorCode;
  open_tags[i++].ti_Data = (ULONG)pip_error;
  /* Request an exact video-area size. WA_Width/Height include borders and
   * silently force scaling even when the user has not resized the window. */
  open_tags[i].ti_Tag = WA_InnerWidth;
  open_tags[i++].ti_Data = info->width;
  open_tags[i].ti_Tag = WA_InnerHeight;
  open_tags[i++].ti_Data = info->height;
  open_tags[i].ti_Tag = WA_Title;
  open_tags[i++].ti_Data = (ULONG)"ZZ9000 zzplay";
  open_tags[i].ti_Tag = WA_PubScreenName;
  open_tags[i++].ti_Data = (ULONG)"Workbench";
  open_tags[i].ti_Tag = WA_Activate;
  open_tags[i++].ti_Data = TRUE;
  open_tags[i].ti_Tag = WA_DragBar;
  open_tags[i++].ti_Data = TRUE;
  open_tags[i].ti_Tag = WA_CloseGadget;
  open_tags[i++].ti_Data = TRUE;
  open_tags[i].ti_Tag = WA_DepthGadget;
  open_tags[i++].ti_Data = TRUE;
  open_tags[i].ti_Tag = WA_SizeGadget;
  open_tags[i++].ti_Data = TRUE;
  open_tags[i].ti_Tag = WA_IDCMP;
  open_tags[i++].ti_Data = IDCMP_CLOSEWINDOW;
  open_tags[i].ti_Tag = TAG_DONE;
  open_tags[i].ti_Data = 0U;

  window = p96PIP_OpenTagList(open_tags);
  if (!window) {
    return 0;
  }
  get_tags[0].ti_Tag = P96PIP_SourceBitMap;
  get_tags[0].ti_Data = (ULONG)&bitmap_value;
  get_tags[1].ti_Tag = TAG_DONE;
  get_tags[1].ti_Data = 0U;
  if (p96PIP_GetTagList(window, get_tags) == 0 || bitmap_value == 0U) {
    p96PIP_Close(window);
    return 0;
  }
  *bitmap = (struct BitMap *)bitmap_value;
  return window;
}

static int zzplay_decode_once(ZZ9KContext *ctx, uint32_t session,
                              ZZ9KVideoSessionResult *result)
{
  ZZ9KVideoSessionDecodeDesc decode;
  memset(&decode, 0, sizeof(decode));
  decode.session = session;
  return zz9k_video_session_decode(ctx, &decode, result);
}

int main(int argc, char **argv)
{
  const char *path = 0;
  FILE *file = 0;
  ZZPlayVideoInfo info;
  ZZ9KBoard board;
  ZZ9KContext *ctx = 0;
  ZZ9KCaps caps;
  ZZ9KServiceInfo service;
  ZZ9KSharedBuffer input;
  ZZ9KVideoSessionBeginDesc begin;
  ZZ9KVideoSessionWriteDesc write;
  ZZ9KVideoSessionResult result;
  struct ZZPlayTimer timer;
  struct ZZPlayStats stats;
  struct Window *window = 0;
  struct BitMap *bitmap = 0;
  uint32_t session = 0U;
  uint32_t frame_period_us;
  uint32_t frames = 0U;
  LONG pip_error = 0;
  int status = ZZ9K_STATUS_OK;
  int timer_open = 0;
  int stop = 0;
  int done = 0;
  int show_fps = 0;
  int uncapped = 0;
  int i;

  memset(&info, 0, sizeof(info));
  memset(&board, 0, sizeof(board));
  memset(&input, 0, sizeof(input));
  memset(&timer, 0, sizeof(timer));
  memset(&stats, 0, sizeof(stats));
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--fps") == 0) {
      show_fps = 1;
    } else if (strcmp(argv[i], "--benchmark") == 0) {
      show_fps = 1;
      uncapped = 1;
    } else if (strcmp(argv[i], "--help") == 0) {
      zzplay_usage(stdout);
      return 0;
    } else if (!path) {
      path = argv[i];
    } else {
      zzplay_usage(stderr);
      return 20;
    }
  }
  if (!path) {
    zzplay_usage(stderr);
    return 20;
  }
  file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "zzplay: cannot open %s\n", path);
    return 20;
  }
  if (!zzplay_probe_file(file, &info) || info.width < 16U ||
      info.height < 16U || info.width > ZZPLAY_MAX_WIDTH ||
      info.height > ZZPLAY_MAX_HEIGHT) {
    fprintf(stderr, "zzplay: no supported MPEG sequence header found\n");
    status = ZZ9K_STATUS_UNSUPPORTED;
    goto cleanup;
  }
  printf("zzplay: MPEG-1/PS %lux%lu, %lu.%03lu fps\n",
         (unsigned long)info.width, (unsigned long)info.height,
         (unsigned long)(info.frame_rate_milli / 1000U),
         (unsigned long)(info.frame_rate_milli % 1000U));

  if (zz9k_find_board(&board) != ZZ9K_STATUS_OK ||
      board.zorro_version != 3U) {
    fprintf(stderr, "zzplay: the P96 video window currently requires Zorro 3\n");
    status = ZZ9K_STATUS_UNSUPPORTED;
    goto cleanup;
  }
  P96Base = OpenLibrary((CONST_STRPTR)"Picasso96API.library", 2U);
  if (!P96Base) {
    fprintf(stderr, "zzplay: cannot open Picasso96API.library\n");
    status = ZZ9K_STATUS_UNSUPPORTED;
    goto cleanup;
  }
  window = zzplay_open_pip(&info, &bitmap, &pip_error);
  if (!window) {
    fprintf(stderr, "zzplay: cannot open P96 PIP window (error %ld)\n",
            (long)pip_error);
    status = ZZ9K_STATUS_UNSUPPORTED;
    goto cleanup;
  }
  status = zz9k_open(&ctx);
  if (status != ZZ9K_STATUS_OK) {
    fprintf(stderr, "zzplay: SDK open failed: %s\n", zz9k_status_name(status));
    goto cleanup;
  }
  status = zz9k_query_caps(ctx, &caps);
  if (status != ZZ9K_STATUS_OK ||
      (caps.capability_bits & ZZ9K_CAP_VIDEO_DECODE) == 0U) {
    fprintf(stderr, "zzplay: firmware does not advertise video decode\n");
    status = ZZ9K_STATUS_UNSUPPORTED;
    goto cleanup;
  }
  status = zz9k_query_service(ctx, ZZ9K_SERVICE_VIDEO, &service);
  if (status != ZZ9K_STATUS_OK ||
      (service.flags & (ZZ9K_SERVICE_FLAG_VIDEO_MPEG1 |
                        ZZ9K_SERVICE_FLAG_VIDEO_MPEG_PS |
                        ZZ9K_SERVICE_FLAG_VIDEO_DIRECT_OVERLAY)) !=
          (ZZ9K_SERVICE_FLAG_VIDEO_MPEG1 |
           ZZ9K_SERVICE_FLAG_VIDEO_MPEG_PS |
           ZZ9K_SERVICE_FLAG_VIDEO_DIRECT_OVERLAY)) {
    fprintf(stderr, "zzplay: required MPEG-1/PS direct-overlay backend is unavailable\n");
    status = ZZ9K_STATUS_UNSUPPORTED;
    goto cleanup;
  }
  status = zz9k_alloc_shared(ctx, ZZPLAY_INPUT_BYTES, 64U,
                             ZZ9K_ALLOC_HOST_WINDOW, &input);
  if (status != ZZ9K_STATUS_OK || !input.data) {
    fprintf(stderr, "zzplay: input buffer allocation failed: %s\n",
            zz9k_status_name(status));
    goto cleanup;
  }
  memset(&begin, 0, sizeof(begin));
  begin.codec = ZZ9K_VIDEO_CODEC_MPEG1;
  begin.container = ZZ9K_VIDEO_CONTAINER_MPEG_PS;
  begin.width = info.width;
  begin.height = info.height;
  begin.output_format = ZZ9K_VIDEO_OUTPUT_DIRECT_OVERLAY;
  status = zz9k_video_session_begin(ctx, &begin, &result);
  if (status != ZZ9K_STATUS_OK) {
    fprintf(stderr, "zzplay: session begin failed: %s\n",
            zz9k_status_name(status));
    goto cleanup;
  }
  session = result.session;
  timer_open = zzplay_timer_open(&timer);
  if (!timer_open) {
    fprintf(stderr, "zzplay: cannot open timer.device\n");
    status = ZZ9K_STATUS_IO_ERROR;
    goto cleanup;
  }
  frame_period_us = 1000000000U / info.frame_rate_milli;
  if (show_fps) {
    printf("zzplay: FPS reporting enabled%s\n",
           uncapped ? " (uncapped benchmark)" : "");
    zzplay_stats_start(&stats);
  }
  printf("zzplay: frame path direct planar overlay\n");

  while (!stop && !done) {
    size_t got = fread((void *)(uintptr_t)input.data, 1U, input.length, file);
    int eof = got < input.length;

    if (ferror(file)) {
      fprintf(stderr, "zzplay: input read failed\n");
      status = ZZ9K_STATUS_IO_ERROR;
      break;
    }
    memset(&write, 0, sizeof(write));
    write.session = session;
    write.src_handle = input.handle;
    write.src_length = (uint32_t)got;
    write.flags = eof ? ZZ9K_VIDEO_SESSION_WRITE_EOF : 0U;
    status = zz9k_video_session_write(ctx, &write, &result);
    if (status != ZZ9K_STATUS_OK) {
      fprintf(stderr, "zzplay: stream write failed: %s\n",
              zz9k_status_name(status));
      break;
    }

    for (;;) {
      TimeVal_Type started;
      TimeVal_Type ended;
      uint32_t elapsed = 0U;

      stop = zzplay_window_stop(window);
      if (stop) {
        break;
      }
      if (timer_open) {
        GetSysTime(&started);
      }
      status = zzplay_decode_once(ctx, session, &result);
      if (status != ZZ9K_STATUS_OK) {
        fprintf(stderr, "zzplay: frame decode failed: %s\n",
                zz9k_status_name(status));
        stop = 1;
        break;
      }
      if ((result.flags & ZZ9K_VIDEO_SESSION_RESULT_DONE) != 0U) {
        done = 1;
        break;
      }
      if ((result.flags & ZZ9K_VIDEO_SESSION_RESULT_NEED_INPUT) != 0U) {
        if (eof) {
          fprintf(stderr, "zzplay: truncated stream at end of input\n");
          status = ZZ9K_STATUS_IO_ERROR;
          stop = 1;
        }
        break;
      }
      if ((result.flags & ZZ9K_VIDEO_SESSION_RESULT_FRAME_READY) == 0U) {
        fprintf(stderr, "zzplay: decoder returned an unknown state\n");
        status = ZZ9K_STATUS_INTERNAL_ERROR;
        stop = 1;
        break;
      }
      frames++;
      if (result.frame_rate_milli != 0U) {
        frame_period_us = 1000000000U / result.frame_rate_milli;
      }
      if (timer_open && !uncapped) {
        GetSysTime(&ended);
        elapsed = zzplay_elapsed_us(&started, &ended);
        if (elapsed < frame_period_us) {
          zzplay_wait_us(&timer, frame_period_us - elapsed);
        }
      }
      if (show_fps) {
        if (uncapped) {
          GetSysTime(&ended);
          elapsed = zzplay_elapsed_us(&started, &ended);
        }
        zzplay_stats_frame(&stats, elapsed);
      }
    }
  }

cleanup:
  if (session != 0U && ctx) {
    ZZ9KVideoSessionResult close_result;
    int close_status = zz9k_video_session_close(ctx, session, 0U,
                                                &close_result);
    if (status == ZZ9K_STATUS_OK && close_status != ZZ9K_STATUS_OK) {
      status = close_status;
    }
  }
  if (timer_open) {
    zzplay_timer_close(&timer);
  }
  if (input.handle != 0U && ctx) {
    (void)zz9k_free_shared(ctx, input.handle);
  }
  if (ctx) {
    zz9k_close(ctx);
  }
  if (window) {
    p96PIP_Close(window);
  }
  if (P96Base) {
    CloseLibrary(P96Base);
    P96Base = 0;
  }
  if (file) {
    fclose(file);
  }
  if (status == ZZ9K_STATUS_OK) {
    printf("zzplay: %lu frames\n", (unsigned long)frames);
    if (show_fps) {
      zzplay_stats_finish(&stats, frames);
    }
    return 0;
  }
  return 20;
}
