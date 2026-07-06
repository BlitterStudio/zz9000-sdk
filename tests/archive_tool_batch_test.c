/*
 * Unit tests for the zz9k-archive LHA batch-offload driver internals.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define ZZ9K_ARCHIVE_NO_MAIN 1
#include "../tools/zz9k-archive.c"

#include <stdio.h>
#include <string.h>

static void make_entry(ZZ9KArchiveEntry *entry, uint32_t method,
                       uint32_t compressed, uint32_t uncompressed)
{
  memset(entry, 0, sizeof(*entry));
  strcpy(entry->name, "file.bin");
  entry->method = method;
  entry->compressed_size = compressed;
  entry->uncompressed_size = uncompressed;
  entry->data_offset = 0U;
  entry->crc32 = 0x1234U;
  entry->flags = ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32;
}

static int test_eligibility(void)
{
  ZZ9KArchiveEntry entry;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 200U);
  if (!zz9k_archive_lha_batch_member_eligible(&entry, 1000U)) return 1;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH0, 100U, 100U);
  if (zz9k_archive_lha_batch_member_eligible(&entry, 1000U)) return 2;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 200U);
  entry.is_dir = 1U;
  if (zz9k_archive_lha_batch_member_eligible(&entry, 1000U)) return 3;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH5, 0U, 200U);
  if (zz9k_archive_lha_batch_member_eligible(&entry, 1000U)) return 4;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 0U);
  if (zz9k_archive_lha_batch_member_eligible(&entry, 1000U)) return 5;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 200U);
  entry.data_offset = 950U; /* runs past the archive */
  if (zz9k_archive_lha_batch_member_eligible(&entry, 1000U)) return 6;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 200U);
  strcpy(entry.name, "../evil");
  if (zz9k_archive_lha_batch_member_eligible(&entry, 1000U)) return 7;

  return 0;
}

static int test_plan_chunk_packs_by_budget_and_cap(void)
{
  ZZ9KArchiveEntry entries[5];
  uint32_t members[5] = {0U, 1U, 2U, 3U, 4U};
  ZZ9KLhaBatchChunk chunk;
  uint32_t i;

  for (i = 0U; i < 5U; i++) {
    make_entry(&entries[i], ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 300U);
  }

  /* blob budget fits 2.5 members -> chunk of 2 */
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 5U, 0U,
                                        ZZ9K_BATCH_MODE_TEST, 64U, 250U, 0U,
                                        &chunk) != 2U) {
    return 1;
  }
  if (chunk.first != 0U || chunk.count != 2U || chunk.blob_length != 200U) {
    return 2;
  }

  /* member cap of 3 binds before the budget */
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 5U, 0U,
                                        ZZ9K_BATCH_MODE_TEST, 3U, 10000U, 0U,
                                        &chunk) != 3U) {
    return 3;
  }

  /* extract: output budget fits 2 members */
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 5U, 0U,
                                        ZZ9K_BATCH_MODE_EXTRACT, 64U, 10000U,
                                        700U, &chunk) != 2U) {
    return 4;
  }
  if (chunk.output_length != 600U) return 5;

  /* resume mid-list */
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 5U, 3U,
                                        ZZ9K_BATCH_MODE_TEST, 64U, 10000U, 0U,
                                        &chunk) != 2U) {
    return 6;
  }
  if (chunk.first != 3U) return 7;

  /* an oversize member packs nothing (caller advances past it) */
  entries[0].compressed_size = 999U;
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 5U, 0U,
                                        ZZ9K_BATCH_MODE_TEST, 64U, 500U, 0U,
                                        &chunk) != 0U) {
    return 8;
  }
  return 0;
}

static int test_plan_chunk_test_mode_uncompressed_cap(void)
{
  ZZ9KArchiveEntry entries[4];
  uint32_t members[4] = {0U, 1U, 2U, 3U};
  ZZ9KLhaBatchChunk chunk;
  uint32_t i;

  for (i = 0U; i < 4U; i++) {
    make_entry(&entries[i], ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 300U);
  }

  /* cap fits 2.5 members' uncompressed output -> chunk of 2 */
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 4U, 0U,
                                        ZZ9K_BATCH_MODE_TEST, 64U, 10000U,
                                        750U, &chunk) != 2U) {
    return 1;
  }
  if (chunk.output_length != 600U) return 2;

  /* 0 = unbounded (existing TEST behavior preserved) */
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 4U, 0U,
                                        ZZ9K_BATCH_MODE_TEST, 64U, 10000U,
                                        0U, &chunk) != 4U) {
    return 3;
  }

  /* an oversize member alone packs nothing (caller advances) */
  entries[0].uncompressed_size = 999U;
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 4U, 0U,
                                        ZZ9K_BATCH_MODE_TEST, 64U, 10000U,
                                        750U, &chunk) != 0U) {
    return 4;
  }
  return 0;
}

static int test_write_tables_lays_out_descriptors(void)
{
  ZZ9KArchiveEntry entries[2];
  uint32_t members[2] = {0U, 1U};
  ZZ9KLhaBatchChunk chunk;
  ZZ9KBatchLayout layout;
  ZZ9KBatchMemberDesc desc;
  uint8_t image[ZZ9K_BATCH_HEADER_SIZE + 2U * ZZ9K_BATCH_DESC_SIZE];
  ZZ9KBatchParsedHeader parsed;

  make_entry(&entries[0], ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 300U);
  make_entry(&entries[1], ZZ9K_ARCHIVE_LHA_METHOD_LH7, 50U, 200U);
  entries[1].flags = 0U; /* no stored CRC */

  if (!zz9k_batch_layout_init(&layout, ZZ9K_BATCH_MODE_EXTRACT, 2U, 150U,
                              500U)) {
    return 1;
  }
  if (zz9k_archive_lha_batch_plan_chunk(entries, members, 2U, 0U,
                                        ZZ9K_BATCH_MODE_EXTRACT, 2U, 150U,
                                        500U, &chunk) != 2U) {
    return 2;
  }
  zz9k_archive_lha_batch_write_tables(entries, members, &chunk, &layout,
                                      image);

  if (zz9k_batch_parse_header(image, layout.total_size, &parsed) !=
      ZZ9K_STATUS_OK) {
    return 3;
  }
  if (parsed.member_count != 2U || parsed.blob_length != 150U) return 4;

  zz9k_batch_read_desc(image + ZZ9K_BATCH_HEADER_SIZE, &desc);
  if (desc.algorithm != ZZ9K_COMPRESSION_LH5) return 5;
  if (desc.src_offset != 0U || desc.src_length != 100U) return 6;
  if (desc.dst_offset != 0U || desc.uncompressed_size != 300U) return 7;
  if (desc.expected_crc != 0x1234U ||
      desc.flags != ZZ9K_BATCH_MEMBER_FLAG_HAVE_CRC) {
    return 8;
  }

  zz9k_batch_read_desc(image + ZZ9K_BATCH_HEADER_SIZE + ZZ9K_BATCH_DESC_SIZE,
                       &desc);
  if (desc.algorithm != ZZ9K_COMPRESSION_LH7) return 9;
  if (desc.src_offset != 100U || desc.src_length != 50U) return 10;
  if (desc.dst_offset != 300U || desc.uncompressed_size != 200U) return 11;
  if (desc.flags != 0U) return 12;
  return 0;
}

static int test_judge_maps_results_to_states(void)
{
  ZZ9KArchiveEntry entry;
  ZZ9KBatchMemberResult result;

  make_entry(&entry, ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 300U);
  entry.crc32 = 0xbeefU;
  zz9k_lha_diag_reset();

  result.status = ZZ9K_STATUS_OK;
  result.bytes_written = 300U;
  result.checksum = 0xbeefU;
  if (zz9k_archive_lha_batch_judge(&entry, &result) != ZZ9K_LHA_BATCH_DONE) {
    return 1;
  }
  if (zz9k_lha_diag_batched != 1UL) return 2;

  result.checksum = 0xdeadU; /* CRC miss */
  if (zz9k_archive_lha_batch_judge(&entry, &result) != ZZ9K_LHA_BATCH_SW) {
    return 3;
  }
  if (zz9k_lha_diag_sw_crc_miss != 1UL) return 4;

  result.checksum = 0xbeefU;
  result.bytes_written = 299U; /* size miss */
  if (zz9k_archive_lha_batch_judge(&entry, &result) != ZZ9K_LHA_BATCH_SW) {
    return 5;
  }
  if (zz9k_lha_diag_sw_crc_miss != 2UL) return 6;

  result.bytes_written = 300U;
  result.status = ZZ9K_STATUS_BAD_REQUEST; /* member decode failed */
  if (zz9k_archive_lha_batch_judge(&entry, &result) != ZZ9K_LHA_BATCH_SW) {
    return 7;
  }
  if (zz9k_lha_diag_sw_codec_fail != 1UL) return 8;

  /* no stored CRC: only the size is judged */
  entry.flags = 0U;
  result.status = ZZ9K_STATUS_OK;
  result.checksum = 0x0U;
  if (zz9k_archive_lha_batch_judge(&entry, &result) != ZZ9K_LHA_BATCH_DONE) {
    return 9;
  }
  return 0;
}

static int test_batch_run_no_service_leaves_all_none(void)
{
  ZZ9KArchiveEntry entries[2];
  uint8_t data[256];
  uint8_t state[2] = {9U, 9U};
  ZZ9KServiceInfo service;

  memset(data, 0, sizeof(data));
  make_entry(&entries[0], ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 300U);
  make_entry(&entries[1], ZZ9K_ARCHIVE_LHA_METHOD_LH5, 100U, 300U);
  memset(&service, 0, sizeof(service));

  /* no context */
  state[0] = state[1] = 9U;
  zz9k_archive_lha_batch_run(0, &service, data, sizeof(data), entries, 2U,
                             "t", 0, state);
  if (state[0] != 9U || state[1] != 9U) return 1; /* untouched */

  /* context but no batch capability advertised */
  service.flags = ZZ9K_SERVICE_FLAG_CODEC_LZH;
  zz9k_archive_lha_batch_run((ZZ9KContext *)&service /* non-NULL dummy */,
                             &service, data, sizeof(data), entries, 2U, "t",
                             0, state);
  if (state[0] != 9U || state[1] != 9U) return 2;

  /* list command never batches */
  service.flags = ZZ9K_SERVICE_FLAG_CODEC_LZH |
                  ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_BATCH;
  zz9k_archive_lha_batch_run((ZZ9KContext *)&service, &service, data,
                             sizeof(data), entries, 2U, "l", 0, state);
  if (state[0] != 9U || state[1] != 9U) return 3;

  return 0;
}

int main(void)
{
  int rc;

  if ((rc = test_eligibility()) != 0) return 100 + rc;
  if ((rc = test_plan_chunk_packs_by_budget_and_cap()) != 0) return 200 + rc;
  if ((rc = test_write_tables_lays_out_descriptors()) != 0) return 300 + rc;
  if ((rc = test_judge_maps_results_to_states()) != 0) return 400 + rc;
  if ((rc = test_batch_run_no_service_leaves_all_none()) != 0) return 500 + rc;
  if ((rc = test_plan_chunk_test_mode_uncompressed_cap()) != 0) {
    return 600 + rc;
  }
  return 0;
}
