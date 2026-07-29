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
#include "zzplay-ahi.h"
#include "zzplay-audio.h"
#include "zzplay-controls.h"
#include "zzplay-core.h"
#include "zzplay-media.h"
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
#define ZZPLAY_PCM_BYTES (128U * 1024U)
#define ZZPLAY_PCM_LOW_WATER (16U * 1024U)
#define ZZPLAY_PCM_HIGH_WATER (96U * 1024U)
#define ZZPLAY_AHI_PERIODS_PER_SECOND 50U
#define ZZPLAY_SYNC_POLL_US 2000U
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
  ZZ9KSharedBuffer pcm;
  ZZPlayPCMRing pcm_ring;
  ZZPlayAHISink ahi;
  struct ZZPlayTimer timer;
  struct ZZPlayStats stats;
  struct Window *window;
  struct BitMap *bitmap;
  uint32_t session;
  uint32_t frames;
  uint32_t final_underruns;
  uint64_t audio_origin_pts;
  uint64_t final_audio_frames;
  ZZ9KMediaSessionAudioResult audio_result;
  ZZPlaySyncPolicy sync_policy;
  uint8_t audio_enabled;
  uint8_t audio_prepared;
  uint8_t audio_started;
  uint8_t audio_status_known;
  uint8_t audio_refresh_needed;
  uint8_t frame_held;
};

static uint32_t zzplay_elapsed_us(const TimeVal_Type *start,
                                  const TimeVal_Type *end);

static void zzplay_usage(FILE *stream)
{
  fprintf(stream,
          "%s\n"
          "Usage: zzplay [--fps|--benchmark] "
          "[--audio=auto|ahi|mhi|ax|none] "
          "<mpeg1-program-stream>\n"
          "  --fps        rolling paced-playback and decode-call FPS\n"
          "  --benchmark  disable pacing and audio unless requested\n"
          "  --audio=...  select program-audio output (AX/MHI reject MP2)\n",
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
                              ZZ9KMediaSessionMainResult *result)
{
  return zz9k_media_session_decode(ctx, session, 0U, result);
}

static const char *zzplay_audio_backend_name(ZZPlayAudioBackend backend)
{
  switch (backend) {
    case ZZPLAY_AUDIO_AHI:
      return "AHI";
    case ZZPLAY_AUDIO_MHI:
      return "MHI";
    case ZZPLAY_AUDIO_AX:
      return "direct AX";
    case ZZPLAY_AUDIO_NONE:
      return "disabled";
    case ZZPLAY_AUDIO_AUTO:
    default:
      return "AUTO";
  }
}

static int zzplay_audio_prepare_from_result(
    struct ZZPlayRuntime *runtime,
    const ZZ9KMediaSessionAudioResult *audio)
{
  uint32_t period_frames;

  if (runtime->audio_prepared) {
    if (audio->sample_rate != runtime->ahi.sample_rate ||
        audio->channels != runtime->ahi.channels ||
        audio->sample_format != ZZ9K_AUDIO_SAMPLE_FORMAT_S16BE) {
      return ZZ9K_STATUS_UNSUPPORTED;
    }
    return ZZ9K_STATUS_OK;
  }
  if (audio->sample_rate == 0U) {
    return ZZ9K_STATUS_OK;
  }
  if ((audio->channels != 1U && audio->channels != 2U) ||
      audio->sample_format != ZZ9K_AUDIO_SAMPLE_FORMAT_S16BE) {
    return ZZ9K_STATUS_UNSUPPORTED;
  }
  period_frames =
      audio->sample_rate / ZZPLAY_AHI_PERIODS_PER_SECOND;
  if (period_frames == 0U) {
    period_frames = 1U;
  }
  if (!zzplay_ahi_prepare(
          &runtime->ahi, audio->sample_rate, audio->channels,
          period_frames)) {
    return ZZ9K_STATUS_IO_ERROR;
  }
  runtime->audio_prepared = 1U;
  (void)zzplay_resource_acquire(
      &runtime->core.resources, ZZPLAY_RESOURCE_AUDIO_SINK);
  printf("zzplay: audio path MP2 decode -> AHI S16BE, "
         "%lu Hz, %lu channel%s\n",
         (unsigned long)audio->sample_rate,
         (unsigned long)audio->channels,
         audio->channels == 1U ? "" : "s");
  return ZZ9K_STATUS_OK;
}

static int zzplay_audio_result_valid(
    const struct ZZPlayRuntime *runtime,
    const ZZ9KMediaSessionAudioResult *audio)
{
  return audio->pcm_acknowledged ==
             runtime->pcm_ring.acknowledged &&
         audio->pcm_produced >= audio->pcm_acknowledged &&
         audio->pcm_produced - audio->pcm_acknowledged <=
             runtime->pcm_ring.capacity;
}

static int zzplay_audio_query_origin(struct ZZPlayRuntime *runtime)
{
  ZZ9KMediaSessionStatusResult status_result;
  int status;

  if (runtime->audio_origin_pts != ZZ9K_MEDIA_NO_PTS) {
    return ZZ9K_STATUS_OK;
  }
  status = zz9k_media_session_status(
      runtime->ctx, runtime->session, ZZ9K_MEDIA_STATUS_AUDIO,
      0U, &status_result);
  if (status != ZZ9K_STATUS_OK) {
    return status;
  }
  runtime->audio_origin_pts = status_result.value[3];
  return ZZ9K_STATUS_OK;
}

static int zzplay_audio_pump(struct ZZPlayRuntime *runtime,
                             int flush_tail,
                             int refresh_status)
{
  ZZ9KMediaSessionAudioResult audio;
  uint64_t available;
  size_t period_bytes;
  int status;

  if (!runtime->audio_enabled) {
    return ZZ9K_STATUS_OK;
  }
  if (runtime->audio_prepared && !zzplay_ahi_poll(&runtime->ahi)) {
    return ZZ9K_STATUS_IO_ERROR;
  }
  if (refresh_status || !runtime->audio_status_known) {
    status = zz9k_media_session_audio_read(
        runtime->ctx, runtime->session,
        runtime->pcm_ring.acknowledged, 0U, &audio);
    if (status != ZZ9K_STATUS_OK) {
      return status;
    }
    if (!zzplay_audio_result_valid(runtime, &audio)) {
      return ZZ9K_STATUS_INTERNAL_ERROR;
    }
    runtime->audio_result = audio;
    runtime->audio_status_known = 1U;
  }
  status = zzplay_audio_prepare_from_result(
      runtime, &runtime->audio_result);
  if (status != ZZ9K_STATUS_OK || !runtime->audio_prepared) {
    return status;
  }
  status = zzplay_audio_query_origin(runtime);
  if (status != ZZ9K_STATUS_OK) {
    return status;
  }

  period_bytes =
      (size_t)runtime->ahi.period_frames * runtime->ahi.frame_bytes;
  for (;;) {
    void *destination;
    size_t capacity;
    size_t copied;

    available = zzplay_pcm_ring_available(
        &runtime->pcm_ring, runtime->audio_result.pcm_produced);
    if (available == 0U ||
        (!flush_tail && available < period_bytes)) {
      break;
    }
    destination = zzplay_ahi_acquire_buffer(
        &runtime->ahi, &capacity);
    if (!destination) {
      break;
    }
    copied = zzplay_pcm_ring_copy(
        &runtime->pcm_ring, runtime->audio_result.pcm_produced,
        destination, capacity, runtime->ahi.frame_bytes);
    if (copied == 0U ||
        !zzplay_ahi_submit_buffer(&runtime->ahi, copied) ||
        !zzplay_pcm_ring_acknowledge(
            &runtime->pcm_ring,
            runtime->audio_result.pcm_produced, copied)) {
      return ZZ9K_STATUS_INTERNAL_ERROR;
    }
    status = zz9k_media_session_audio_read(
        runtime->ctx, runtime->session,
        runtime->pcm_ring.acknowledged, 0U, &audio);
    if (status != ZZ9K_STATUS_OK) {
      return status;
    }
    if (!zzplay_audio_result_valid(runtime, &audio)) {
      return ZZ9K_STATUS_INTERNAL_ERROR;
    }
    runtime->audio_result = audio;
    runtime->audio_status_known = 1U;
  }
  return ZZ9K_STATUS_OK;
}

static uint32_t zzplay_sync_wait_us(int64_t drift_pts,
                                    uint64_t hold_ahead_pts)
{
  uint64_t excess;
  uint64_t usec;

  if (drift_pts <= 0 ||
      (uint64_t)drift_pts <= hold_ahead_pts) {
    return 0U;
  }
  excess = (uint64_t)drift_pts - hold_ahead_pts;
  if (excess >=
      ((uint64_t)ZZPLAY_SYNC_POLL_US * 90000U) / 1000000U) {
    return ZZPLAY_SYNC_POLL_US;
  }
  usec = excess * 1000000ULL / 90000U;
  if (usec < ZZPLAY_SYNC_POLL_US) {
    return (uint32_t)usec;
  }
  return ZZPLAY_SYNC_POLL_US;
}

static int zzplay_retire_held_frame(
    struct ZZPlayRuntime *runtime,
    ZZ9KMediaSessionMainResult *result,
    uint32_t frame_period_us,
    uint32_t decode_us,
    int *retired)
{
  ZZPlaySyncDecision decision = ZZPLAY_SYNC_PRESENT;
  int64_t drift = 0;
  int status;

  *retired = 0;
  if (!runtime->audio_started && runtime->audio_prepared &&
      runtime->ahi.clock.queued_frames != 0U) {
    if (zzplay_sync_audio_may_start(
            result->video_pts, runtime->audio_origin_pts,
            runtime->sync_policy.drop_late_pts)) {
      if (!zzplay_ahi_play(&runtime->ahi)) {
        return ZZ9K_STATUS_IO_ERROR;
      }
      runtime->audio_started = 1U;
    }
  }
  if (!runtime->options.uncapped && runtime->audio_enabled &&
      runtime->audio_started &&
      runtime->audio_origin_pts != ZZ9K_MEDIA_NO_PTS &&
      result->video_pts != ZZ9K_MEDIA_NO_PTS) {
    uint64_t master_pts = zzplay_audio_clock_pts(
        &runtime->ahi.clock, runtime->audio_origin_pts);

    decision = zzplay_sync_decide(
        &runtime->sync_policy, result->video_pts, master_pts,
        &drift);
    if (decision == ZZPLAY_SYNC_HOLD) {
      zzplay_stats_record_sync(
          &runtime->stats.core, decision, drift);
      zzplay_wait_us(
          &runtime->timer,
          zzplay_sync_wait_us(
              drift, runtime->sync_policy.hold_ahead_pts));
      return ZZ9K_STATUS_OK;
    }
  } else if (!runtime->options.uncapped) {
    zzplay_wait_us(
        &runtime->timer,
        zzplay_pacing_wait_us(
            frame_period_us, decode_us, 0));
  }

  if (decision == ZZPLAY_SYNC_DISCARD) {
    status = zz9k_media_session_discard(
        runtime->ctx, runtime->session, 0U, result);
  } else {
    status = zz9k_media_session_present(
        runtime->ctx, runtime->session, 0U, result);
  }
  if (status == ZZ9K_STATUS_OK) {
    zzplay_stats_record_sync(
        &runtime->stats.core, decision, drift);
  }
  if (status == ZZ9K_STATUS_BUSY) {
    zzplay_wait_us(&runtime->timer, ZZPLAY_SYNC_POLL_US);
    return ZZ9K_STATUS_OK;
  }
  if (status == ZZ9K_STATUS_OK) {
    *retired = 1;
  }
  return status;
}

static int zzplay_drain_audio(struct ZZPlayRuntime *runtime)
{
  int draining = 0;
  int refresh_status = 1;

  if (!runtime->audio_enabled) {
    return ZZ9K_STATUS_OK;
  }
  if (runtime->audio_prepared) {
    zzplay_ahi_mark_end_of_stream(&runtime->ahi);
  }
  for (;;) {
    ZZPlayStopReason stop_reason;
    int status = zzplay_audio_pump(
        runtime, 1, refresh_status);

    if (status != ZZ9K_STATUS_OK) {
      return status;
    }
    refresh_status = 0;
    if (runtime->audio_prepared) {
      zzplay_ahi_mark_end_of_stream(&runtime->ahi);
    }
    if (!runtime->audio_started && runtime->audio_prepared &&
        runtime->ahi.clock.queued_frames != 0U) {
      if (!zzplay_ahi_play(&runtime->ahi)) {
        return ZZ9K_STATUS_IO_ERROR;
      }
      runtime->audio_started = 1U;
    }
    if (runtime->pcm_ring.acknowledged ==
            runtime->audio_result.pcm_produced &&
        !draining) {
      if (!runtime->audio_started) {
        if (runtime->ahi.clock.queued_frames == 0U) {
          return ZZ9K_STATUS_OK;
        }
        if (!zzplay_ahi_play(&runtime->ahi)) {
          return ZZ9K_STATUS_IO_ERROR;
        }
        runtime->audio_started = 1U;
      }
      if (!zzplay_ahi_begin_drain(&runtime->ahi)) {
        return ZZ9K_STATUS_IO_ERROR;
      }
      draining = 1;
    }
    if (draining && zzplay_ahi_drained(&runtime->ahi)) {
      return ZZ9K_STATUS_OK;
    }
    stop_reason = zzplay_poll_stop(runtime->window);
    if (stop_reason != ZZPLAY_STOP_NONE) {
      zzplay_core_stop(&runtime->core, stop_reason);
      return ZZ9K_STATUS_CANCELLED;
    }
    zzplay_wait_us(&runtime->timer, ZZPLAY_SYNC_POLL_US);
  }
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
    case ZZPLAY_RESOURCE_AUDIO_SINK:
      zzplay_ahi_close(&runtime->ahi);
      runtime->audio_prepared = 0U;
      runtime->audio_started = 0U;
      break;
    case ZZPLAY_RESOURCE_VIDEO_SESSION:
      if (runtime->session != 0U && runtime->ctx) {
        ZZ9KMediaSessionMainResult result;
        int status;
        unsigned retry;

        if (runtime->frame_held) {
          (void)zz9k_media_session_discard(
              runtime->ctx, runtime->session, 0U, &result);
          runtime->frame_held = 0U;
        }
        status = ZZ9K_STATUS_BUSY;
        for (retry = 0U;
             retry < 16U && status == ZZ9K_STATUS_BUSY;
             retry++) {
          status = zz9k_media_session_close(
              runtime->ctx, runtime->session, 0U, &result);
        }
        runtime->session = 0U;
        return status;
      }
      break;
    case ZZPLAY_RESOURCE_PCM_BUFFER:
      if (runtime->pcm.handle != 0U && runtime->ctx) {
        (void)zz9k_free_shared(runtime->ctx, runtime->pcm.handle);
      }
      memset(&runtime->pcm, 0, sizeof(runtime->pcm));
      memset(&runtime->pcm_ring, 0, sizeof(runtime->pcm_ring));
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
  ZZ9KMediaSessionBeginDesc begin;
  ZZ9KMediaSessionWriteDesc write;
  ZZ9KMediaSessionMainResult result;
  ZZPlayBackendDecision audio_decision;
  uint32_t frame_period_us;
  uint32_t held_decode_us = 0U;
  int media_done = 0;
  LONG pip_error = 0;
  int cleanup_status;

  memset(&runtime, 0, sizeof(runtime));
  memset(&info, 0, sizeof(info));
  memset(&board, 0, sizeof(board));
  runtime.audio_origin_pts = ZZ9K_MEDIA_NO_PTS;
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
  if (!info.is_program_stream || !info.has_video_pes) {
    fprintf(stderr,
            "zzplay: MPEG-1 elementary streams are not supported; "
            "a Program Stream is required\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_INVALID_INPUT,
                ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }
  if (!info.has_audio_pes &&
      runtime.options.audio_backend != ZZPLAY_AUDIO_AUTO &&
      runtime.options.audio_backend != ZZPLAY_AUDIO_NONE) {
    fprintf(stderr,
            "zzplay: the Program Stream has no supported MP2 audio\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_INVALID_INPUT,
                ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }
  if (info.has_audio_pes) {
    ZZPlayAudioAvailability availability;

    memset(&availability, 0, sizeof(availability));
    availability.ahi = ZZPLAY_BACKEND_FREE;
    availability.mhi = ZZPLAY_BACKEND_MISSING;
    availability.ax = ZZPLAY_BACKEND_FREE;
    audio_decision = zzplay_audio_select(
        ZZPLAY_MEDIA_AUDIO_MP2, runtime.options.audio_backend,
        &availability);
    if (audio_decision.status != ZZPLAY_BACKEND_OK) {
      fprintf(stderr,
              "zzplay: audio backend %s cannot play Program "
              "Stream MP2 (status %u)\n",
              zzplay_audio_backend_name(
                  runtime.options.audio_backend),
              (unsigned)audio_decision.status);
      zzplay_fail(&runtime, ZZPLAY_FAILURE_CAPABILITY,
                  ZZ9K_STATUS_UNSUPPORTED);
      goto cleanup;
    }
    runtime.audio_enabled =
        audio_decision.selected == ZZPLAY_AUDIO_AHI;
  } else {
    memset(&audio_decision, 0, sizeof(audio_decision));
    audio_decision.status = ZZPLAY_BACKEND_OK;
    audio_decision.selected = ZZPLAY_AUDIO_NONE;
    printf("zzplay: warning: video-only Program Stream\n");
  }
  printf("zzplay: MPEG-1/PS %lux%lu, %lu.%03lu fps, audio %s\n",
         (unsigned long)info.width, (unsigned long)info.height,
         (unsigned long)(info.frame_rate_milli / 1000U),
         (unsigned long)(info.frame_rate_milli % 1000U),
         zzplay_audio_backend_name(audio_decision.selected));

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
      (caps.capability_bits &
       (ZZ9K_CAP_VIDEO_DECODE | ZZ9K_CAP_MEDIA_SESSION)) !=
          (ZZ9K_CAP_VIDEO_DECODE | ZZ9K_CAP_MEDIA_SESSION) ||
      (runtime.audio_enabled &&
       (caps.capability_bits & ZZ9K_CAP_AUDIO_DECODE) == 0U)) {
    fprintf(stderr,
            "zzplay: firmware does not advertise the required "
            "media decode services\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_CAPABILITY,
                ZZ9K_STATUS_UNSUPPORTED);
    goto cleanup;
  }
  cleanup_status = zz9k_query_service(
      runtime.ctx, ZZ9K_SERVICE_VIDEO, &service);
  if (cleanup_status != ZZ9K_STATUS_OK ||
      !zzplay_video_backend_available(service.flags) ||
      (service.flags & ZZ9K_SERVICE_FLAG_VIDEO_MEDIA_SESSION) == 0U ||
      (runtime.audio_enabled &&
       (service.flags & ZZ9K_SERVICE_FLAG_VIDEO_MEDIA_MP2) == 0U)) {
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
  if (runtime.audio_enabled) {
    cleanup_status = zz9k_alloc_shared(
        runtime.ctx, ZZPLAY_PCM_BYTES, 64U,
        ZZ9K_ALLOC_HOST_WINDOW, &runtime.pcm);
    if (runtime.pcm.handle != 0U) {
      (void)zzplay_resource_acquire(
          &runtime.core.resources, ZZPLAY_RESOURCE_PCM_BUFFER);
    }
    if (cleanup_status != ZZ9K_STATUS_OK || !runtime.pcm.data) {
      fprintf(stderr,
              "zzplay: PCM ring allocation failed: %s\n",
              zz9k_status_name(cleanup_status));
      zzplay_fail(&runtime, ZZPLAY_FAILURE_ALLOCATION,
                  cleanup_status);
      goto cleanup;
    }
    zzplay_pcm_ring_init(
        &runtime.pcm_ring, runtime.pcm.data, runtime.pcm.length);
  }
  memset(&begin, 0, sizeof(begin));
  begin.video_codec = ZZ9K_VIDEO_CODEC_MPEG1;
  begin.container = ZZ9K_VIDEO_CONTAINER_MPEG_PS;
  begin.width = info.width;
  begin.height = info.height;
  begin.output_format = ZZ9K_VIDEO_OUTPUT_DIRECT_OVERLAY;
  begin.audio_codec = runtime.audio_enabled
                          ? ZZ9K_MEDIA_AUDIO_MP2
                          : ZZ9K_MEDIA_AUDIO_NONE;
  if (runtime.audio_enabled) {
    begin.pcm_ring_handle = runtime.pcm.handle;
    begin.pcm_ring_capacity = runtime.pcm.length;
    begin.pcm_low_water_bytes = ZZPLAY_PCM_LOW_WATER;
    begin.pcm_high_water_bytes = ZZPLAY_PCM_HIGH_WATER;
  }
  memset(&result, 0, sizeof(result));
  cleanup_status = zz9k_media_session_begin(
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
  runtime.audio_refresh_needed = runtime.audio_enabled;
  if (!zzplay_timer_open(&runtime.timer)) {
    fprintf(stderr, "zzplay: cannot open timer.device\n");
    zzplay_fail(&runtime, ZZPLAY_FAILURE_TIMER, ZZ9K_STATUS_IO_ERROR);
    goto cleanup;
  }
  (void)zzplay_resource_acquire(
      &runtime.core.resources, ZZPLAY_RESOURCE_TIMER);

  frame_period_us = zzplay_frame_period_us(info.frame_rate_milli);
  zzplay_sync_policy_init(
      &runtime.sync_policy, info.frame_rate_milli, 1000U);
  if (runtime.options.show_fps) {
    printf("zzplay: FPS reporting enabled%s\n",
           runtime.options.uncapped ? " (uncapped benchmark)" : "");
    zzplay_stats_start(&runtime.stats);
  }
  printf("zzplay: frame path direct planar overlay\n");
  (void)zzplay_core_begin_prebuffer(&runtime.core);
  (void)zzplay_core_start(&runtime.core);

  while (runtime.core.state == ZZPLAY_STATE_PLAYING) {
    ZZPlayStopReason stop_reason;
    ZZPlayMediaAction action;

    stop_reason = zzplay_poll_stop(runtime.window);
    if (stop_reason != ZZPLAY_STOP_NONE) {
      zzplay_core_stop(&runtime.core, stop_reason);
      break;
    }
    cleanup_status = zzplay_audio_pump(
        &runtime, 0, runtime.audio_refresh_needed);
    if (cleanup_status != ZZ9K_STATUS_OK) {
      fprintf(stderr, "zzplay: audio output failed: %s\n",
              zz9k_status_name(cleanup_status));
      zzplay_fail(&runtime, ZZPLAY_FAILURE_IO, cleanup_status);
      break;
    }
    runtime.audio_refresh_needed = 0U;

    if (runtime.frame_held) {
      int retired;

      cleanup_status = zzplay_retire_held_frame(
          &runtime, &result, frame_period_us, held_decode_us,
          &retired);
      if (cleanup_status != ZZ9K_STATUS_OK) {
        fprintf(stderr,
                "zzplay: frame presentation failed: %s\n",
                zz9k_status_name(cleanup_status));
        zzplay_fail(&runtime, ZZPLAY_FAILURE_SESSION,
                    cleanup_status);
        break;
      }
      if (retired) {
        runtime.frame_held = 0U;
        if (runtime.options.show_fps) {
          zzplay_stats_frame(&runtime.stats, held_decode_us);
        }
      }
      continue;
    }
    if (media_done) {
      break;
    }

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

    if (transport.pending_length != 0U ||
        (transport.eof && !transport.eof_sent)) {
      memset(&write, 0, sizeof(write));
      write.session = runtime.session;
      write.src_handle = runtime.input.handle;
      write.src_offset = transport.pending_offset;
      write.src_length = transport.pending_length;
      write.flags = zzplay_transport_write_flags(&transport);
      cleanup_status = zz9k_media_session_write(
          runtime.ctx, &write, &result);
      if (runtime.audio_enabled) {
        runtime.audio_refresh_needed = 1U;
      }
      if (cleanup_status != ZZ9K_STATUS_OK &&
          cleanup_status != ZZ9K_STATUS_BUSY) {
        fprintf(stderr, "zzplay: stream write failed: %s\n",
                zz9k_status_name(cleanup_status));
        zzplay_fail(&runtime, ZZPLAY_FAILURE_IO,
                    cleanup_status);
        break;
      }
      if (cleanup_status == ZZ9K_STATUS_OK) {
        if (write.src_length != 0U) {
          if (!zzplay_transport_advance(
                  &transport, result.bytes_accepted)) {
            fprintf(stderr,
                    "zzplay: firmware reported invalid input "
                    "progress\n");
            zzplay_fail(&runtime, ZZPLAY_FAILURE_PROTOCOL,
                        ZZ9K_STATUS_INTERNAL_ERROR);
            break;
          }
        } else {
          transport.eof_sent = 1;
        }
      }
    }

    {
      TimeVal_Type started;
      TimeVal_Type ended;

      GetSysTime(&started);
      cleanup_status = zzplay_decode_once(
          runtime.ctx, runtime.session, &result);
      GetSysTime(&ended);
      held_decode_us = zzplay_elapsed_us(&started, &ended);
    }
    if (runtime.audio_enabled) {
      runtime.audio_refresh_needed = 1U;
    }
    if (cleanup_status == ZZ9K_STATUS_BUSY) {
      zzplay_wait_us(&runtime.timer, ZZPLAY_SYNC_POLL_US);
      continue;
    }
    if (cleanup_status != ZZ9K_STATUS_OK) {
      fprintf(stderr, "zzplay: media decode failed: %s\n",
              zz9k_status_name(cleanup_status));
      zzplay_fail(&runtime, ZZPLAY_FAILURE_SESSION,
                  cleanup_status);
      break;
    }
    if (result.frame_rate_num != 0U &&
        result.frame_rate_den != 0U) {
      uint64_t usec =
          ((uint64_t)1000000U * result.frame_rate_den) /
          result.frame_rate_num;

      frame_period_us =
          usec > 0xffffffffULL ? 0xffffffffU : (uint32_t)usec;
      zzplay_sync_policy_init(
          &runtime.sync_policy, result.frame_rate_num,
          result.frame_rate_den);
    }
    action = zzplay_media_result_action(result.flags);
    if (action == ZZPLAY_MEDIA_FRAME_HELD) {
      runtime.frames++;
      runtime.frame_held = 1U;
      continue;
    }
    if (action == ZZPLAY_MEDIA_DONE) {
      media_done = 1;
      continue;
    }
    if (action == ZZPLAY_MEDIA_NEED_INPUT &&
        transport.eof && transport.eof_sent &&
        transport.pending_length == 0U) {
      fprintf(stderr, "zzplay: truncated stream at end of input\n");
      zzplay_fail(&runtime, ZZPLAY_FAILURE_IO,
                  ZZ9K_STATUS_IO_ERROR);
      break;
    }
  }

  if (runtime.core.state == ZZPLAY_STATE_PLAYING && media_done) {
    (void)zzplay_core_begin_drain(&runtime.core);
    cleanup_status = zzplay_drain_audio(&runtime);
    if (cleanup_status == ZZ9K_STATUS_OK) {
      zzplay_core_stop(&runtime.core, ZZPLAY_STOP_EOF);
    } else if (cleanup_status != ZZ9K_STATUS_CANCELLED) {
      fprintf(stderr, "zzplay: audio drain failed: %s\n",
              zz9k_status_name(cleanup_status));
      zzplay_fail(&runtime, ZZPLAY_FAILURE_IO, cleanup_status);
    }
  }

cleanup:
  if (runtime.audio_prepared) {
    runtime.final_audio_frames =
        zzplay_ahi_played_frames(&runtime.ahi);
    runtime.final_underruns = runtime.ahi.clock.underruns;
  }
  cleanup_status = zzplay_resources_release_all(
      &runtime.core.resources, zzplay_release_resource, &runtime);
  if (runtime.core.state != ZZPLAY_STATE_ERROR &&
      cleanup_status != ZZ9K_STATUS_OK) {
    zzplay_fail(&runtime, ZZPLAY_FAILURE_SESSION, cleanup_status);
  }
  if (runtime.core.state != ZZPLAY_STATE_ERROR) {
    printf("zzplay: %lu decoded, %lu presented, %lu discarded "
           "frames",
           (unsigned long)runtime.frames,
           (unsigned long)runtime.stats.core.presented_frames,
           (unsigned long)runtime.stats.core.discarded_frames);
    if (runtime.audio_enabled) {
      printf(", %llu audio frames played, %lu underruns",
             (unsigned long long)runtime.final_audio_frames,
             (unsigned long)runtime.final_underruns);
    }
    printf("\n");
    if (runtime.stats.core.max_abs_drift_pts != 0U) {
      printf("zzplay: A/V drift current %ld ms, max %lu ms, "
             "%lu hold polls, %lu late frames\n",
             (long)(runtime.stats.core.current_drift_pts / 90),
             (unsigned long)(
                 runtime.stats.core.max_abs_drift_pts / 90U),
             (unsigned long)runtime.stats.core.hold_events,
             (unsigned long)runtime.stats.core.late_frames);
    }
    if (runtime.options.show_fps) {
      zzplay_stats_finish(&runtime.stats);
    }
    return 0;
  }
  return 20;
}
