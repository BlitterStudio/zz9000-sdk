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
#include "zzplay-controls.h"
#include "zzplay-core.h"
#include "zzplay-options.h"
#include "zzplay-probe.h"
#include "zzplay-stats.h"
#include "zzplay-stream.h"
#include "zzplay-sync.h"
#include "zzplay-video.h"

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
#include <string.h>

#define ZZPLAY_INPUT_BYTES (64U * 1024U)
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
  ZZPlayStatsCore core;
};

struct ZZPlayRuntime {
  ZZPlayCore core;
  ZZPlayOptions options;
  FILE *file;
  ZZ9KContext *ctx;
  ZZ9KSharedBuffer input;
  struct ZZPlayTimer timer;
  struct ZZPlayStats stats;
  struct Window *window;
  struct BitMap *bitmap;
  uint32_t session;
  uint32_t frames;
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
  zzplay_stats_reset(&stats->core);
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
  zzplay_stats_record_frame(&stats->core, sample_us, decode_us);
  stats->last_sample = now;

  report_us = zzplay_elapsed_us(&stats->report_started, &now);
  if (report_us >= ZZPLAY_FPS_REPORT_US) {
    zzplay_print_fps(
        "current",
        zzplay_fps_milli(stats->core.report_frames, report_us),
        zzplay_fps_milli(stats->core.report_frames,
                         stats->core.report_decode_us));
    stats->report_started = now;
    zzplay_stats_reset_report(&stats->core);
  }
}

static void zzplay_stats_finish(const struct ZZPlayStats *stats)
{
  if (stats->core.total_frames == 0U || stats->core.wall_us == 0U) {
    printf("zzplay: average fps unavailable\n");
    return;
  }
  zzplay_print_fps(
      "average",
      zzplay_fps_milli(stats->core.total_frames, stats->core.wall_us),
      zzplay_fps_milli(stats->core.total_frames, stats->core.decode_us));
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
  if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
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

static ZZPlayStopReason zzplay_poll_stop(struct Window *window)
{
  struct IntuiMessage *message;
  int ctrl_c;
  int window_close = 0;

  ctrl_c = (SetSignal(0L, 0L) & SIGBREAKF_CTRL_C) != 0U;
  while (window &&
         (message = (struct IntuiMessage *)GetMsg(window->UserPort))) {
    if (message->Class == IDCMP_CLOSEWINDOW) {
      window_close = 1;
    }
    ReplyMsg((struct Message *)message);
  }
  return zzplay_control_stop_reason(ctrl_c, window_close);
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

static void zzplay_fail(struct ZZPlayRuntime *runtime,
                        ZZPlayFailure failure,
                        int status)
{
  zzplay_core_fail(&runtime->core, failure, status);
}

static int zzplay_release_resource(void *user,
                                   ZZPlayResource resource)
{
  struct ZZPlayRuntime *runtime = (struct ZZPlayRuntime *)user;

  switch (resource) {
    case ZZPLAY_RESOURCE_TIMER:
      zzplay_timer_close(&runtime->timer);
      break;
    case ZZPLAY_RESOURCE_VIDEO_SESSION:
      if (runtime->session != 0U && runtime->ctx) {
        ZZ9KVideoSessionResult result;
        int status = zz9k_video_session_close(
            runtime->ctx, runtime->session, 0U, &result);
        runtime->session = 0U;
        return status;
      }
      break;
    case ZZPLAY_RESOURCE_INPUT_BUFFER:
      if (runtime->input.handle != 0U && runtime->ctx) {
        (void)zz9k_free_shared(runtime->ctx, runtime->input.handle);
      }
      memset(&runtime->input, 0, sizeof(runtime->input));
      break;
    case ZZPLAY_RESOURCE_SDK_CONTEXT:
      if (runtime->ctx) {
        zz9k_close(runtime->ctx);
        runtime->ctx = 0;
      }
      break;
    case ZZPLAY_RESOURCE_VIDEO_WINDOW:
      if (runtime->window) {
        p96PIP_Close(runtime->window);
        runtime->window = 0;
        runtime->bitmap = 0;
      }
      break;
    case ZZPLAY_RESOURCE_P96_LIBRARY:
      if (P96Base) {
        CloseLibrary(P96Base);
        P96Base = 0;
      }
      break;
    case ZZPLAY_RESOURCE_INPUT_FILE:
      if (runtime->file) {
        (void)fclose(runtime->file);
        runtime->file = 0;
      }
      break;
    default:
      return ZZ9K_STATUS_BAD_REQUEST;
  }
  return ZZ9K_STATUS_OK;
}

int main(int argc, char **argv)
{
  struct ZZPlayRuntime runtime;
  ZZPlayOptionsResult options_result;
  ZZPlayVideoInfo info;
  ZZPlayTransport transport;
  ZZ9KBoard board;
  ZZ9KCaps caps;
  ZZ9KServiceInfo service;
  ZZ9KVideoSessionBeginDesc begin;
  ZZ9KVideoSessionWriteDesc write;
  ZZ9KVideoSessionResult result;
  uint32_t frame_period_us;
  LONG pip_error = 0;
  int cleanup_status;

  memset(&runtime, 0, sizeof(runtime));
  memset(&info, 0, sizeof(info));
  memset(&board, 0, sizeof(board));
  zzplay_core_init(&runtime.core);
  zzplay_transport_init(&transport);

  options_result = zzplay_options_parse_cli(
      argc, argv, &runtime.options);
  if (options_result == ZZPLAY_OPTIONS_HELP) {
    zzplay_usage(stdout);
    return 0;
  }
  if (options_result != ZZPLAY_OPTIONS_OK) {
    zzplay_usage(stderr);
    return 20;
  }

  runtime.file = fopen(runtime.options.path, "rb");
  if (!runtime.file) {
    fprintf(stderr, "zzplay: cannot open %s\n", runtime.options.path);
    return 20;
  }
  (void)zzplay_resource_acquire(
      &runtime.core.resources, ZZPLAY_RESOURCE_INPUT_FILE);

  if (!zzplay_probe_file(runtime.file, &info) ||
      !zzplay_video_info_supported(&info)) {
    fprintf(stderr, "zzplay: no supported MPEG sequence header found\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_INVALID_INPUT,
                ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }
  printf("zzplay: MPEG-1/PS %lux%lu, %lu.%03lu fps\n",
         (unsigned long)info.width, (unsigned long)info.height,
         (unsigned long)(info.frame_rate_milli / 1000U),
         (unsigned long)(info.frame_rate_milli % 1000U));

  if (zz9k_find_board(&board) != ZZ9K_STATUS_OK ||
      board.zorro_version != 3U) {
    fprintf(stderr,
            "zzplay: the P96 video window currently requires Zorro 3\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_UNSUPPORTED_BOARD,
                ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }

  P96Base = OpenLibrary((CONST_STRPTR)"Picasso96API.library", 2U);
  if (!P96Base) {
    fprintf(stderr, "zzplay: cannot open Picasso96API.library\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_P96, ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }
  (void)zzplay_resource_acquire(
      &runtime.core.resources, ZZPLAY_RESOURCE_P96_LIBRARY);

  runtime.window = zzplay_open_pip(&info, &runtime.bitmap, &pip_error);
  if (!runtime.window) {
    fprintf(stderr, "zzplay: cannot open P96 PIP window (error %ld)\n",
            (long)pip_error);
    zzplay_fail(&runtime, ZZPLAY_FAILURE_PIP, ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }
  (void)zzplay_resource_acquire(
      &runtime.core.resources, ZZPLAY_RESOURCE_VIDEO_WINDOW);

  cleanup_status = zz9k_open(&runtime.ctx);
  if (runtime.ctx) {
    (void)zzplay_resource_acquire(
        &runtime.core.resources, ZZPLAY_RESOURCE_SDK_CONTEXT);
  }
  if (cleanup_status != ZZ9K_STATUS_OK) {
    fprintf(stderr, "zzplay: SDK open failed: %s\n",
            zz9k_status_name(cleanup_status));
    zzplay_fail(&runtime, ZZPLAY_FAILURE_SDK, cleanup_status);
    goto cleanup;
  }
  cleanup_status = zz9k_query_caps(runtime.ctx, &caps);
  if (cleanup_status != ZZ9K_STATUS_OK ||
      (caps.capability_bits & ZZ9K_CAP_VIDEO_DECODE) == 0U) {
    fprintf(stderr, "zzplay: firmware does not advertise video decode\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_CAPABILITY,
                ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }
  cleanup_status = zz9k_query_service(
      runtime.ctx, ZZ9K_SERVICE_VIDEO, &service);
  if (cleanup_status != ZZ9K_STATUS_OK ||
      !zzplay_video_backend_available(service.flags)) {
    fprintf(stderr,
            "zzplay: required MPEG-1/PS direct-overlay backend "
            "is unavailable\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_CAPABILITY,
                ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }

  cleanup_status = zz9k_alloc_shared(
      runtime.ctx, ZZPLAY_INPUT_BYTES, 64U,
      ZZ9K_ALLOC_HOST_WINDOW, &runtime.input);
  if (runtime.input.handle != 0U) {
    (void)zzplay_resource_acquire(
        &runtime.core.resources, ZZPLAY_RESOURCE_INPUT_BUFFER);
  }
  if (cleanup_status != ZZ9K_STATUS_OK || !runtime.input.data) {
    fprintf(stderr, "zzplay: input buffer allocation failed: %s\n",
            zz9k_status_name(cleanup_status));
    zzplay_fail(&runtime, ZZPLAY_FAILURE_ALLOCATION, cleanup_status);
    goto cleanup;
  }
  memset(&begin, 0, sizeof(begin));
  begin.codec = ZZ9K_VIDEO_CODEC_MPEG1;
  begin.container = ZZ9K_VIDEO_CONTAINER_MPEG_PS;
  begin.width = info.width;
  begin.height = info.height;
  begin.output_format = ZZ9K_VIDEO_OUTPUT_DIRECT_OVERLAY;
  memset(&result, 0, sizeof(result));
  cleanup_status = zz9k_video_session_begin(
      runtime.ctx, &begin, &result);
  if (result.session != 0U) {
    runtime.session = result.session;
    (void)zzplay_resource_acquire(
        &runtime.core.resources, ZZPLAY_RESOURCE_VIDEO_SESSION);
  }
  if (cleanup_status != ZZ9K_STATUS_OK) {
    fprintf(stderr, "zzplay: session begin failed: %s\n",
            zz9k_status_name(cleanup_status));
    zzplay_fail(&runtime, ZZPLAY_FAILURE_SESSION, cleanup_status);
    goto cleanup;
  }
  if (!zzplay_timer_open(&runtime.timer)) {
    fprintf(stderr, "zzplay: cannot open timer.device\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_TIMER, ZZ9K_STATUS_IO_ERROR);
    goto cleanup;
  }
  (void)zzplay_resource_acquire(
      &runtime.core.resources, ZZPLAY_RESOURCE_TIMER);

  frame_period_us = zzplay_frame_period_us(info.frame_rate_milli);
  if (runtime.options.show_fps) {
    printf("zzplay: FPS reporting enabled%s\n",
           runtime.options.uncapped ? " (uncapped benchmark)" : "");
    zzplay_stats_start(&runtime.stats);
  }
  printf("zzplay: frame path direct planar overlay\n");
  (void)zzplay_core_start(&runtime.core);

  while (runtime.core.state == ZZPLAY_STATE_PLAYING) {
    if (transport.pending_length == 0U && !transport.eof) {
      size_t got = fread((void *)(uintptr_t)runtime.input.data, 1U,
                         runtime.input.length, runtime.file);

      if (ferror(runtime.file)) {
        fprintf(stderr, "zzplay: input read failed\n");
        zzplay_fail(&runtime, ZZPLAY_FAILURE_IO, ZZ9K_STATUS_IO_ERROR);
        break;
      }
      zzplay_transport_set_chunk(
          &transport, (uint32_t)got, got < runtime.input.length);
    }

    memset(&write, 0, sizeof(write));
    write.session = runtime.session;
    write.src_handle = runtime.input.handle;
    write.src_offset = transport.pending_offset;
    write.src_length = transport.pending_length;
    write.flags = zzplay_transport_write_flags(&transport);
    cleanup_status = zz9k_video_session_write(
        runtime.ctx, &write, &result);
    if (cleanup_status != ZZ9K_STATUS_OK) {
      fprintf(stderr, "zzplay: stream write failed: %s\n",
              zz9k_status_name(cleanup_status));
      zzplay_fail(&runtime, ZZPLAY_FAILURE_IO, cleanup_status);
      break;
    }
    if (write.src_length != 0U) {
      if (!zzplay_transport_advance(
              &transport, result.bytes_accepted)) {
        fprintf(stderr,
                "zzplay: firmware reported invalid input progress\n");
        zzplay_fail(&runtime, ZZPLAY_FAILURE_PROTOCOL,
                    ZZ9K_STATUS_INTERNAL_ERROR);
        break;
      }
    } else {
      transport.eof_sent = 1;
    }

    while (runtime.core.state == ZZPLAY_STATE_PLAYING) {
      TimeVal_Type started;
      TimeVal_Type ended;
      ZZPlayStopReason stop_reason;
      ZZPlayVideoResultAction action;
      uint32_t elapsed = 0U;
      uint32_t wait_us;

      stop_reason = zzplay_poll_stop(runtime.window);
      if (stop_reason != ZZPLAY_STOP_NONE) {
        zzplay_core_stop(&runtime.core, stop_reason);
        break;
      }
      GetSysTime(&started);
      cleanup_status = zzplay_decode_once(
          runtime.ctx, runtime.session, &result);
      if (cleanup_status != ZZ9K_STATUS_OK) {
        fprintf(stderr, "zzplay: frame decode failed: %s\n",
                zz9k_status_name(cleanup_status));
        zzplay_fail(&runtime, ZZPLAY_FAILURE_SESSION, cleanup_status);
        break;
      }

      action = zzplay_video_result_action(result.flags);
      if (zzplay_video_result_has_frame(result.flags)) {
        runtime.frames++;
        if (result.frame_rate_milli != 0U) {
          frame_period_us =
              zzplay_frame_period_us(result.frame_rate_milli);
        }
        GetSysTime(&ended);
        elapsed = zzplay_elapsed_us(&started, &ended);
        wait_us = zzplay_pacing_wait_us(
            frame_period_us, elapsed, runtime.options.uncapped);
        zzplay_wait_us(&runtime.timer, wait_us);
        if (runtime.options.show_fps) {
          zzplay_stats_frame(&runtime.stats, elapsed);
        }
      }

      if (action == ZZPLAY_VIDEO_RESULT_DONE) {
        zzplay_core_stop(&runtime.core, ZZPLAY_STOP_EOF);
        break;
      }
      if (action == ZZPLAY_VIDEO_RESULT_NEED_INPUT) {
        if (transport.pending_length != 0U ||
            (transport.eof && !transport.eof_sent)) {
          break;
        }
        if (transport.eof) {
          fprintf(stderr, "zzplay: truncated stream at end of input\n");
          zzplay_fail(&runtime, ZZPLAY_FAILURE_IO,
                      ZZ9K_STATUS_IO_ERROR);
        }
        break;
      }
      if (action == ZZPLAY_VIDEO_RESULT_INVALID) {
        fprintf(stderr,
                "zzplay: decoder returned an unknown state\n");
        zzplay_fail(&runtime, ZZPLAY_FAILURE_PROTOCOL,
                    ZZ9K_STATUS_INTERNAL_ERROR);
        break;
      }
    }
  }

cleanup:
  cleanup_status = zzplay_resources_release_all(
      &runtime.core.resources, zzplay_release_resource, &runtime);
  if (runtime.core.state != ZZPLAY_STATE_ERROR &&
      cleanup_status != ZZ9K_STATUS_OK) {
    zzplay_fail(&runtime, ZZPLAY_FAILURE_SESSION, cleanup_status);
  }
  if (runtime.core.state != ZZPLAY_STATE_ERROR) {
    printf("zzplay: %lu frames\n", (unsigned long)runtime.frames);
    if (runtime.options.show_fps) {
      zzplay_stats_finish(&runtime.stats);
    }
    return 0;
  }
  return 20;
}
