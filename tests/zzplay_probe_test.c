/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "../tools/zzplay-probe.h"
#include "../tools/zzplay-stats.h"
#include "../tools/zzplay-stream.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check_file_probe(void)
{
  uint8_t split[4101];
  static const uint8_t header[8] = {
    0x00U, 0x00U, 0x01U, 0xb3U, 0x14U, 0x00U, 0xf0U, 0x13U
  };
  FILE *file;
  ZZPlayVideoInfo info;

  memset(split, 0x55, sizeof(split));
  memcpy(split + 4093U, header, sizeof(header));
  file = tmpfile();
  if (!file) {
    return 0;
  }
  if (fwrite(split, 1U, sizeof(split), file) != sizeof(split) ||
      fflush(file) != 0) {
    fclose(file);
    return 0;
  }
  memset(&info, 0, sizeof(info));
  if (!zzplay_probe_file(file, &info) || ftell(file) != 0L ||
      info.width != 320U || info.height != 240U ||
      info.frame_rate_milli != 25000U ||
      !zzplay_video_info_supported(&info)) {
    fclose(file);
    return 0;
  }
  fclose(file);

  file = tmpfile();
  if (!file) {
    return 0;
  }
  if (fwrite(header, 1U, 7U, file) != 7U || fflush(file) != 0 ||
      zzplay_probe_file(file, &info) || ftell(file) != 0L) {
    fclose(file);
    return 0;
  }
  fclose(file);

  info.width = 15U;
  info.height = 240U;
  info.frame_rate_milli = 25000U;
  return !zzplay_video_info_supported(&info);
}

static int check_program_probe(void)
{
  static const uint8_t program[] = {
    0x00U, 0x00U, 0x01U, 0xbaU,
    0x55U, 0x00U, 0x00U, 0x01U, 0xe0U,
    0x55U, 0x00U, 0x00U, 0x01U, 0xc0U
  };
  ZZPlayVideoInfo info;

  memset(&info, 0, sizeof(info));
  zzplay_probe_mpeg_program(program, sizeof(program), &info);
  return info.is_program_stream && info.has_video_pes &&
         info.has_audio_pes;
}

static int check_stats_and_transport(void)
{
  ZZPlayStatsCore stats;
  ZZPlayTransport transport;

  zzplay_stats_reset(&stats);
  zzplay_stats_record_frame(&stats, 40000U, 12000U);
  zzplay_stats_record_frame(&stats, 40000U, 10000U);
  if (stats.total_frames != 2U || stats.report_frames != 2U ||
      stats.wall_us != 80000U || stats.decode_us != 22000U ||
      stats.report_decode_us != 22000U) {
    return 0;
  }
  zzplay_stats_reset_report(&stats);
  if (stats.total_frames != 2U || stats.report_frames != 0U ||
      stats.report_decode_us != 0U) {
    return 0;
  }
  zzplay_stats_record_sync(&stats, ZZPLAY_SYNC_HOLD, 1801);
  zzplay_stats_record_sync(&stats, ZZPLAY_SYNC_PRESENT, -200);
  zzplay_stats_record_sync(&stats, ZZPLAY_SYNC_DISCARD, -4000);
  if (stats.hold_events != 1U || stats.presented_frames != 1U ||
      stats.discarded_frames != 1U || stats.late_frames != 1U ||
      stats.current_drift_pts != -4000 ||
      stats.max_abs_drift_pts != 4000U) {
    return 0;
  }
  zzplay_stats_record_profile(
      &stats, ZZPLAY_PROFILE_AUDIO_READ, 6000U);
  zzplay_stats_record_profile(
      &stats, ZZPLAY_PROFILE_AUDIO_READ, 4000U);
  zzplay_stats_record_profile(
      &stats, ZZPLAY_PROFILE_PCM_COPY, 1250U);
  zzplay_stats_record_profile(
      &stats, ZZPLAY_PROFILE_FILE_READ, 7000U);
  zzplay_stats_record_profile(
      &stats, ZZPLAY_PROFILE_INPUT_COPY, 2000U);
  zzplay_stats_record_profile(
      &stats, ZZPLAY_PROFILE_AHI_SUBMIT, 3000U);
  if (stats.profile[ZZPLAY_PROFILE_AUDIO_READ].calls != 2U ||
      stats.profile[ZZPLAY_PROFILE_AUDIO_READ].elapsed_us != 10000U ||
      zzplay_stats_profile_average_us(
          &stats, ZZPLAY_PROFILE_AUDIO_READ) != 5000U ||
      stats.profile[ZZPLAY_PROFILE_FILE_READ].calls != 1U ||
      stats.profile[ZZPLAY_PROFILE_INPUT_COPY].calls != 1U ||
      stats.profile[ZZPLAY_PROFILE_AHI_SUBMIT].calls != 1U ||
      zzplay_stats_profile_total_us(&stats) != 23250U) {
    return 0;
  }

  zzplay_transport_init(&transport);
  zzplay_transport_set_chunk(&transport, 4096U, 0);
  if (zzplay_transport_write_flags(&transport) != 0U ||
      !zzplay_transport_advance(&transport, 1024U) ||
      transport.pending_offset != 1024U ||
      transport.pending_length != 3072U) {
    return 0;
  }
  zzplay_transport_set_chunk(&transport, 0U, 1);
  if (zzplay_transport_write_flags(&transport) !=
      ZZ9K_VIDEO_SESSION_WRITE_EOF) {
    return 0;
  }
  transport.eof_sent = 1;
  return zzplay_transport_write_flags(&transport) == 0U;
}

int main(void)
{
  static const uint8_t sequence[] = {
    0xaa, 0x00, 0x00, 0x01, 0xb3, 0x13, 0xe0, 0xf3, 0x13
  };
  ZZPlayVideoInfo info;
  uint32_t accepted_total;
  uint32_t offset;
  uint32_t remaining;

  memset(&info, 0, sizeof(info));
  if (!zzplay_probe_mpeg_sequence(sequence, sizeof(sequence), &info)) {
    return 1;
  }
  if (info.width != 318U || info.height != 243U ||
      info.frame_rate_milli != 25000U) {
    return 2;
  }
  if (zzplay_probe_mpeg_sequence(sequence, 7U, &info)) {
    return 3;
  }
  if (zzplay_mpeg_frame_rate_milli(4U) != 29970U ||
      zzplay_mpeg_frame_rate_milli(9U) != 0U) {
    return 4;
  }
  if (zzplay_fps_milli(50U, 2000000U) != 25000U ||
      zzplay_fps_milli(60U, 2002002U) != 29970U ||
      zzplay_fps_milli(0U, 1000000U) != 0U ||
      zzplay_fps_milli(1U, 0U) != 0U) {
    return 5;
  }
  if (!zzplay_video_backend_available(ZZPLAY_REQUIRED_VIDEO_FLAGS) ||
      zzplay_video_backend_available(
          ZZPLAY_REQUIRED_VIDEO_FLAGS &
          ~ZZ9K_SERVICE_FLAG_VIDEO_STREAMING_INPUT)) {
    return 6;
  }
  accepted_total = 0U;
  offset = 0U;
  remaining = 65536U;
  if (!zzplay_advance_input(&offset, &remaining, &accepted_total, 16384U) ||
      offset != 16384U || remaining != 49152U ||
      accepted_total != 16384U ||
      !zzplay_advance_input(&offset, &remaining, &accepted_total, 65536U) ||
      offset != 65536U || remaining != 0U || accepted_total != 65536U) {
    return 7;
  }
  accepted_total = 4096U;
  offset = 1024U;
  remaining = 2048U;
  if (zzplay_advance_input(&offset, &remaining, &accepted_total, 6145U) ||
      offset != 1024U || remaining != 2048U || accepted_total != 4096U) {
    return 8;
  }
  if (zzplay_video_result_action(ZZ9K_VIDEO_SESSION_RESULT_HEADER_READY) !=
          ZZPLAY_VIDEO_RESULT_CONTINUE ||
      zzplay_video_result_action(ZZ9K_VIDEO_SESSION_RESULT_FRAME_READY |
                                 ZZ9K_VIDEO_SESSION_RESULT_NEED_INPUT) !=
          ZZPLAY_VIDEO_RESULT_NEED_INPUT ||
      !zzplay_video_result_has_frame(
          ZZ9K_VIDEO_SESSION_RESULT_FRAME_READY |
          ZZ9K_VIDEO_SESSION_RESULT_NEED_INPUT) ||
      zzplay_video_result_action(ZZ9K_VIDEO_SESSION_RESULT_FRAME_READY |
                                 ZZ9K_VIDEO_SESSION_RESULT_DONE) !=
          ZZPLAY_VIDEO_RESULT_DONE ||
      !zzplay_video_result_has_frame(
          ZZ9K_VIDEO_SESSION_RESULT_FRAME_READY |
          ZZ9K_VIDEO_SESSION_RESULT_DONE) ||
      zzplay_video_result_action(0U) != ZZPLAY_VIDEO_RESULT_INVALID) {
    return 9;
  }
  if (!check_file_probe()) {
    return 10;
  }
  if (!check_stats_and_transport()) {
    return 11;
  }
  if (!check_program_probe()) {
    return 12;
  }
  return 0;
}
