/*
 * Parser-level tests for zz9k-archive.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define ZZ9K_ARCHIVE_NO_MAIN 1
#include "../tools/zz9k-archive.c"

#include <stdio.h>
#include <string.h>

#include "archive_7z_real_fixtures.h"

#define ZZ9K_ARCHIVE_TEST_7Z_SPLIT_SUBSTREAM_FLAG 0x40000000UL

static int write_test_file(const char *path,
                           const uint8_t *data,
                           uint32_t length);

static void put_le16(uint8_t *dst, uint16_t value)
{
  dst[0] = (uint8_t)(value & 0xffU);
  dst[1] = (uint8_t)((value >> 8) & 0xffU);
}

static void put_le32(uint8_t *dst, uint32_t value)
{
  dst[0] = (uint8_t)(value & 0xffU);
  dst[1] = (uint8_t)((value >> 8) & 0xffU);
  dst[2] = (uint8_t)((value >> 16) & 0xffU);
  dst[3] = (uint8_t)((value >> 24) & 0xffU);
}

static void put_le64(uint8_t *dst, uint64_t value)
{
  uint32_t i;

  for (i = 0U; i < 8U; i++) {
    dst[i] = (uint8_t)((value >> (i * 8U)) & 0xffU);
  }
}

static int make_zip_store(uint8_t *zip, uint32_t *length)
{
  const char *name = "hello.txt";
  const char *data = "hello";
  uint32_t local_offset = 0U;
  uint32_t name_len = 9U;
  uint32_t data_len = 5U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  memcpy(zip + pos, data, data_len); pos += data_len;

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  cd_size = pos - cd_offset;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static int make_zip_store_backslash_name(uint8_t *zip, uint32_t *length)
{
  const char *name = "dir\\hello.txt";
  const char *data = "hello";
  uint32_t local_offset = 0U;
  uint32_t name_len = 13U;
  uint32_t data_len = 5U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  memcpy(zip + pos, data, data_len); pos += data_len;

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  cd_size = pos - cd_offset;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static int make_zip_store_named(uint8_t *zip, uint32_t *length,
                                const char *name, const char *data,
                                uint32_t crc32, uint32_t external_attrs)
{
  uint32_t local_offset = 0U;
  uint32_t name_len = (uint32_t)strlen(name);
  uint32_t data_len = data ? (uint32_t)strlen(data) : 0U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, crc32); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  if (data_len != 0U) {
    memcpy(zip + pos, data, data_len); pos += data_len;
  }

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, crc32); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, external_attrs); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  cd_size = pos - cd_offset;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static uint32_t append_zip_store_local(uint8_t *zip, uint32_t pos,
                                       const char *name, const char *data,
                                       uint32_t crc32,
                                       uint32_t *local_offset)
{
  uint32_t name_len = (uint32_t)strlen(name);
  uint32_t data_len = data ? (uint32_t)strlen(data) : 0U;

  *local_offset = pos;
  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, crc32); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  if (data_len != 0U) {
    memcpy(zip + pos, data, data_len); pos += data_len;
  }
  return pos;
}

static int make_lha_lh0(uint8_t *lha, uint32_t *length)
{
  const char *name = "dir/hello.txt";
  const char *data = "hello";
  uint32_t name_len = (uint32_t)strlen(name);
  uint32_t data_len = (uint32_t)strlen(data);
  uint32_t pos = 0U;
  uint32_t i;
  uint8_t header_size;
  uint8_t checksum = 0U;

  header_size = (uint8_t)(22U + name_len);
  lha[pos++] = header_size;
  lha[pos++] = 0U;
  memcpy(lha + pos, "-lh0-", 5U); pos += 5U;
  put_le32(lha + pos, data_len); pos += 4U;
  put_le32(lha + pos, data_len); pos += 4U;
  put_le32(lha + pos, 0U); pos += 4U;
  lha[pos++] = 0x20U;
  lha[pos++] = 0U;
  lha[pos++] = (uint8_t)name_len;
  memcpy(lha + pos, name, name_len); pos += name_len;
  put_le16(lha + pos, 0U); pos += 2U;
  for (i = 2U; i < 2U + header_size; i++) {
    checksum = (uint8_t)(checksum + lha[i]);
  }
  lha[1] = checksum;
  memcpy(lha + pos, data, data_len); pos += data_len;
  lha[pos++] = 0U;
  *length = pos;
  return 1;
}

static int make_lha_lh5_level1(uint8_t *lha, uint32_t *length)
{
  const char *name = "hello.txt";
  const char *data = "0123456789";
  uint32_t name_len = (uint32_t)strlen(name);
  uint32_t data_len = (uint32_t)strlen(data);
  uint32_t pos = 0U;
  uint32_t i;
  uint8_t header_size;
  uint8_t checksum = 0U;

  memset(lha, 0, 128U);
  header_size = (uint8_t)(25U + name_len);
  lha[pos++] = header_size;
  lha[pos++] = 0U;
  memcpy(lha + pos, "-lh5-", 5U); pos += 5U;
  put_le32(lha + pos, data_len); pos += 4U;
  put_le32(lha + pos, 20U); pos += 4U;
  put_le32(lha + pos, 0U); pos += 4U;
  lha[pos++] = 0x20U;
  lha[pos++] = 1U;
  lha[pos++] = (uint8_t)name_len;
  memcpy(lha + pos, name, name_len); pos += name_len;
  put_le16(lha + pos, 0U); pos += 2U;
  lha[pos++] = 'A';
  put_le16(lha + pos, 0U); pos += 2U;
  for (i = 2U; i < 2U + header_size; i++) {
    checksum = (uint8_t)(checksum + lha[i]);
  }
  lha[1] = checksum;
  memcpy(lha + pos, data, data_len); pos += data_len;
  lha[pos++] = 0U;
  *length = pos;
  return 1;
}

static int make_lha_lh5_level1_ext_name(uint8_t *lha, uint32_t *length)
{
  const char *base_name = "base.tmp";
  const char *ext_dir = "dir/";
  const char *ext_name = "renamed.txt";
  const char *data = "0123456789";
  uint32_t base_name_len = (uint32_t)strlen(base_name);
  uint32_t ext_dir_len = (uint32_t)strlen(ext_dir);
  uint32_t ext_name_len = (uint32_t)strlen(ext_name);
  uint32_t data_len = (uint32_t)strlen(data);
  uint32_t pos = 0U;
  uint32_t i;
  uint32_t dir_ext_size = 1U + ext_dir_len + 2U;
  uint32_t name_ext_size = 1U + ext_name_len + 2U;
  uint8_t header_size;
  uint8_t checksum = 0U;

  memset(lha, 0, 192U);
  header_size = (uint8_t)(25U + base_name_len);
  lha[pos++] = header_size;
  lha[pos++] = 0U;
  memcpy(lha + pos, "-lh5-", 5U); pos += 5U;
  put_le32(lha + pos, data_len); pos += 4U;
  put_le32(lha + pos, 20U); pos += 4U;
  put_le32(lha + pos, 0U); pos += 4U;
  lha[pos++] = 0x20U;
  lha[pos++] = 1U;
  lha[pos++] = (uint8_t)base_name_len;
  memcpy(lha + pos, base_name, base_name_len); pos += base_name_len;
  put_le16(lha + pos, 0U); pos += 2U;
  lha[pos++] = 'A';
  put_le16(lha + pos, (uint16_t)dir_ext_size); pos += 2U;
  for (i = 2U; i < 2U + header_size; i++) {
    checksum = (uint8_t)(checksum + lha[i]);
  }
  lha[1] = checksum;

  lha[pos++] = 0x02U;
  memcpy(lha + pos, ext_dir, ext_dir_len); pos += ext_dir_len;
  put_le16(lha + pos, (uint16_t)name_ext_size); pos += 2U;
  lha[pos++] = 0x01U;
  memcpy(lha + pos, ext_name, ext_name_len); pos += ext_name_len;
  put_le16(lha + pos, 0U); pos += 2U;
  memcpy(lha + pos, data, data_len); pos += data_len;
  lha[pos++] = 0U;
  *length = pos;
  return 1;
}

static uint32_t append_lha_level1_header(uint8_t *lha, uint32_t pos,
                                         const char *method,
                                         const char *base_name,
                                         const char *ext_dir,
                                         const char *ext_name,
                                         const char *data,
                                         uint32_t unpacked_size)
{
  uint32_t base_name_len = (uint32_t)strlen(base_name);
  uint32_t ext_dir_len = ext_dir ? (uint32_t)strlen(ext_dir) : 0U;
  uint32_t ext_name_len = ext_name ? (uint32_t)strlen(ext_name) : 0U;
  uint32_t data_len = data ? (uint32_t)strlen(data) : 0U;
  uint32_t dir_ext_size = ext_dir_len ? 1U + ext_dir_len + 2U : 0U;
  uint32_t name_ext_size = ext_name_len ? 1U + ext_name_len + 2U : 0U;
  uint32_t header_start = pos;
  uint32_t i;
  uint8_t header_size = (uint8_t)(25U + base_name_len);
  uint8_t checksum = 0U;

  lha[pos++] = header_size;
  lha[pos++] = 0U;
  memcpy(lha + pos, method, 5U); pos += 5U;
  put_le32(lha + pos, data_len); pos += 4U;
  put_le32(lha + pos, unpacked_size); pos += 4U;
  put_le32(lha + pos, 0U); pos += 4U;
  lha[pos++] = 0x20U;
  lha[pos++] = 1U;
  lha[pos++] = (uint8_t)base_name_len;
  memcpy(lha + pos, base_name, base_name_len); pos += base_name_len;
  put_le16(lha + pos, 0U); pos += 2U;
  lha[pos++] = 'A';
  put_le16(lha + pos,
           (uint16_t)(dir_ext_size ? dir_ext_size : name_ext_size));
  pos += 2U;
  for (i = header_start + 2U; i < header_start + 2U + header_size; i++) {
    checksum = (uint8_t)(checksum + lha[i]);
  }
  lha[header_start + 1U] = checksum;
  if (dir_ext_size) {
    lha[pos++] = 0x02U;
    memcpy(lha + pos, ext_dir, ext_dir_len); pos += ext_dir_len;
    put_le16(lha + pos, (uint16_t)name_ext_size); pos += 2U;
  }
  if (name_ext_size) {
    lha[pos++] = 0x01U;
    memcpy(lha + pos, ext_name, ext_name_len); pos += ext_name_len;
    put_le16(lha + pos, 0U); pos += 2U;
  }
  if (data_len != 0U) {
    memcpy(lha + pos, data, data_len); pos += data_len;
  }
  return pos;
}

static int make_lha_level1_lhd_and_lh0(uint8_t *lha, uint32_t *length)
{
  uint32_t pos = 0U;

  memset(lha, 0, 256U);
  pos = append_lha_level1_header(lha, pos, "-lhd-", "base",
                                 "dir/", "", 0, 0U);
  pos = append_lha_level1_header(lha, pos, "-lh0-", "base",
                                 "dir/", "stored.txt", "hello", 5U);
  lha[pos++] = 0U;
  *length = pos;
  return 1;
}

static uint32_t append_lha_level2_header(uint8_t *lha, uint32_t pos,
                                         const char *method,
                                         const char *ext_dir,
                                         const char *ext_name,
                                         const char *data,
                                         uint32_t unpacked_size)
{
  uint32_t ext_dir_len = ext_dir ? (uint32_t)strlen(ext_dir) : 0U;
  uint32_t ext_name_len = ext_name ? (uint32_t)strlen(ext_name) : 0U;
  uint32_t data_len = data ? (uint32_t)strlen(data) : 0U;
  uint32_t dir_ext_size = ext_dir_len ? 1U + ext_dir_len + 2U : 0U;
  uint32_t name_ext_size = ext_name_len ? 1U + ext_name_len + 2U : 0U;
  uint32_t header_start = pos;
  uint32_t header_size = 26U + dir_ext_size + name_ext_size;

  put_le16(lha + pos, header_size); pos += 2U;
  memcpy(lha + pos, method, 5U); pos += 5U;
  put_le32(lha + pos, data_len); pos += 4U;
  put_le32(lha + pos, unpacked_size); pos += 4U;
  put_le32(lha + pos, 0U); pos += 4U;
  lha[pos++] = 0U;
  lha[pos++] = 2U;
  put_le16(lha + pos, 0U); pos += 2U;
  lha[pos++] = 'A';
  put_le16(lha + pos,
           (uint16_t)(dir_ext_size ? dir_ext_size : name_ext_size));
  pos += 2U;
  if (dir_ext_size) {
    lha[pos++] = 0x02U;
    memcpy(lha + pos, ext_dir, ext_dir_len); pos += ext_dir_len;
    put_le16(lha + pos, (uint16_t)name_ext_size); pos += 2U;
  }
  if (name_ext_size) {
    lha[pos++] = 0x01U;
    memcpy(lha + pos, ext_name, ext_name_len); pos += ext_name_len;
    put_le16(lha + pos, 0U); pos += 2U;
  }
  if (data_len != 0U) {
    memcpy(lha + pos, data, data_len); pos += data_len;
  }
  return header_start + header_size + data_len;
}

static int make_lha_lh5_level2_ext_name(uint8_t *lha, uint32_t *length)
{
  uint32_t pos = 0U;

  memset(lha, 0, 192U);
  pos = append_lha_level2_header(lha, pos, "-lh5-", "dir/",
                                 "level2.txt", "0123456789", 20U);
  lha[pos++] = 0U;
  *length = pos;
  return 1;
}

static const uint8_t lha_lh5_docker_fixture[] = {
  0x54U, 0x00U, 0x2dU, 0x6cU, 0x68U, 0x35U, 0x2dU, 0xb9U, 0x00U, 0x00U, 0x00U, 0x90U,
  0xe2U, 0x00U, 0x00U, 0x59U, 0xddU, 0x16U, 0x6aU, 0x20U, 0x02U, 0xe1U, 0x14U, 0x55U,
  0x05U, 0x00U, 0x00U, 0xfaU, 0x8bU, 0x0cU, 0x00U, 0x01U, 0x68U, 0x65U, 0x6cU, 0x6cU,
  0x6fU, 0x2eU, 0x74U, 0x78U, 0x74U, 0x1dU, 0x00U, 0x02U, 0x62U, 0x75U, 0x69U, 0x6cU,
  0x64U, 0xffU, 0x6cU, 0x68U, 0x61U, 0x2dU, 0x66U, 0x69U, 0x78U, 0x74U, 0x75U, 0x72U,
  0x65U, 0xffU, 0x73U, 0x72U, 0x63U, 0xffU, 0x64U, 0x69U, 0x72U, 0xffU, 0x05U, 0x00U,
  0x50U, 0xa4U, 0x81U, 0x07U, 0x00U, 0x51U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
  0x01U, 0x00U, 0x4aU, 0xacU, 0xa0U, 0x44U, 0xbfU, 0xdeU, 0x7bU, 0x00U, 0x7bU, 0x00U,
  0x2cU, 0x17U, 0x82U, 0xe4U, 0x60U, 0xe9U, 0x51U, 0x2cU, 0xe7U, 0xb1U, 0xe3U, 0xe0U,
  0xb7U, 0x9aU, 0xacU, 0xc5U, 0xb7U, 0x9fU, 0x62U, 0xafU, 0xccU, 0x6eU, 0x75U, 0x3dU,
  0x7dU, 0x3dU, 0xc5U, 0x79U, 0x89U, 0xe1U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU,
  0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U,
  0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U,
  0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U,
  0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U,
  0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU,
  0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U,
  0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U,
  0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U,
  0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U,
  0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU,
  0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U,
  0xc6U, 0x31U, 0x8cU, 0xffU, 0x00U, 0x00U,
};

static const uint8_t lha_lh6_docker_fixture[] = {
  0x5aU, 0x00U, 0x2dU, 0x6cU, 0x68U, 0x36U, 0x2dU, 0xb8U, 0x00U, 0x00U, 0x00U, 0x90U,
  0xe2U, 0x00U, 0x00U, 0x5cU, 0xe2U, 0x16U, 0x6aU, 0x20U, 0x02U, 0x2dU, 0x69U, 0x55U,
  0x05U, 0x00U, 0x00U, 0xaaU, 0x63U, 0x0eU, 0x00U, 0x01U, 0x70U, 0x61U, 0x79U, 0x6cU,
  0x6fU, 0x61U, 0x64U, 0x2eU, 0x74U, 0x78U, 0x74U, 0x21U, 0x00U, 0x02U, 0x62U, 0x75U,
  0x69U, 0x6cU, 0x64U, 0xffU, 0x6cU, 0x68U, 0x61U, 0x2dU, 0x6dU, 0x65U, 0x74U, 0x68U,
  0x6fU, 0x64U, 0x2dU, 0x66U, 0x69U, 0x78U, 0x74U, 0x75U, 0x72U, 0x65U, 0x73U, 0xffU,
  0x73U, 0x72U, 0x63U, 0xffU, 0x05U, 0x00U, 0x50U, 0xa4U, 0x81U, 0x07U, 0x00U, 0x51U,
  0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x00U, 0x4aU, 0xacU, 0xa2U, 0xd4U,
  0xbfU, 0xdcU, 0x7bU, 0x00U, 0x5cU, 0x2cU, 0x81U, 0xf4U, 0x48U, 0x55U, 0x42U, 0x7eU,
  0x0bU, 0x38U, 0xecU, 0x78U, 0xe8U, 0x0bU, 0xccU, 0xadU, 0xfbU, 0xbdU, 0xb6U, 0xc7U,
  0xd5U, 0xeeU, 0x65U, 0xb3U, 0x3cU, 0x9aU, 0xd4U, 0xbdU, 0xb8U, 0xc7U, 0x53U, 0x46U,
  0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU,
  0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U,
  0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U,
  0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U,
  0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U,
  0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU,
  0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U,
  0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U,
  0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U,
  0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U,
  0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU,
  0x63U, 0x18U, 0xc6U, 0x31U, 0x8cU, 0x63U, 0x18U, 0xc6U, 0x33U, 0xfcU, 0x00U,
};

static const uint8_t lha_lh1_docker_fixture[] = {
  0x24U, 0x6aU, 0x2dU, 0x6cU, 0x68U, 0x31U, 0x2dU, 0x74U, 0x00U, 0x00U, 0x00U, 0x50U,
  0x05U, 0x00U, 0x00U, 0xf3U, 0x63U, 0xbbU, 0x5cU, 0x20U, 0x01U, 0x0bU, 0x70U, 0x61U,
  0x79U, 0x6cU, 0x6fU, 0x61U, 0x64U, 0x2eU, 0x74U, 0x78U, 0x74U, 0x55U, 0x63U, 0x55U,
  0x24U, 0x00U, 0x02U, 0x62U, 0x75U, 0x69U, 0x6cU, 0x64U, 0xffU, 0x6cU, 0x68U, 0x61U,
  0x2dU, 0x6fU, 0x6cU, 0x64U, 0x6dU, 0x65U, 0x74U, 0x68U, 0x6fU, 0x64U, 0x2dU, 0x66U,
  0x69U, 0x78U, 0x74U, 0x75U, 0x72U, 0x65U, 0x73U, 0xffU, 0x73U, 0x72U, 0x63U, 0xffU,
  0x05U, 0x00U, 0x50U, 0xa4U, 0x81U, 0x07U, 0x00U, 0x51U, 0x00U, 0x00U, 0x00U, 0x00U,
  0x07U, 0x00U, 0x54U, 0x2aU, 0xe4U, 0x16U, 0x6aU, 0x00U, 0x00U, 0xfcU, 0x7dU, 0x37U,
  0xbaU, 0xcfU, 0x97U, 0xd4U, 0x10U, 0x00U, 0x07U, 0xfdU, 0xf1U, 0xc2U, 0xc5U, 0xc0U,
  0xfdU, 0x5dU, 0xe5U, 0xbfU, 0x01U, 0x0bU, 0x80U, 0x85U, 0x70U, 0x85U, 0xb0U, 0x85U,
  0x41U, 0x0aU, 0xc2U, 0x16U, 0x04U, 0x2dU, 0x08U, 0x50U, 0x21U, 0x40U, 0x85U, 0x02U,
  0x15U, 0x08U, 0x54U, 0x21U, 0x50U, 0x85U, 0x42U, 0x16U, 0x08U, 0x48U, 0x42U, 0x42U,
  0x12U, 0x10U, 0x90U, 0x84U, 0x84U, 0x24U, 0x21U, 0x58U, 0x08U, 0x00U, 0x00U,
};

static uint32_t append_zip_store_central(uint8_t *zip, uint32_t pos,
                                         const char *name, const char *data,
                                         uint32_t crc32,
                                         uint32_t external_attrs,
                                         uint32_t local_offset)
{
  uint32_t name_len = (uint32_t)strlen(name);
  uint32_t data_len = data ? (uint32_t)strlen(data) : 0U;

  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, crc32); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, external_attrs); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  return pos;
}

static uint32_t append_zip_eocd(uint8_t *zip, uint32_t pos,
                                uint32_t entries,
                                uint32_t cd_offset, uint32_t cd_size)
{
  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, (uint16_t)entries); pos += 2U;
  put_le16(zip + pos, (uint16_t)entries); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  return pos;
}

static int make_zip_root_current_dir_then_file(uint8_t *zip,
                                               uint32_t *length)
{
  const char *root = "./";
  const char *name = "./dir/hello.txt";
  const char *data = "hello";
  uint32_t root_offset;
  uint32_t file_offset;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  pos = append_zip_store_local(zip, pos, root, 0, 0U, &root_offset);
  pos = append_zip_store_local(zip, pos, name, data, 0x3610a686UL,
                               &file_offset);
  cd_offset = pos;
  pos = append_zip_store_central(zip, pos, root, 0, 0U, 0x10U,
                                 root_offset);
  pos = append_zip_store_central(zip, pos, name, data, 0x3610a686UL,
                                 0U, file_offset);
  cd_size = pos - cd_offset;
  pos = append_zip_eocd(zip, pos, 2U, cd_offset, cd_size);

  *length = pos;
  return 1;
}

static int make_zip_store_zip64_extra(uint8_t *zip, uint32_t *length)
{
  const char *name = "hello.txt";
  const char *data = "hello";
  uint32_t local_offset = 0U;
  uint32_t name_len = 9U;
  uint32_t data_len = 5U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  memcpy(zip + pos, data, data_len); pos += data_len;

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  put_le16(zip + pos, 0x0001U); pos += 2U;
  put_le16(zip + pos, 16U); pos += 2U;
  put_le64(zip + pos, data_len); pos += 8U;
  put_le64(zip + pos, data_len); pos += 8U;
  cd_size = pos - cd_offset;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static int make_zip_directory_external_attr(uint8_t *zip, uint32_t *length)
{
  const char *name = "dir";
  uint32_t local_offset = 0U;
  uint32_t name_len = 3U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x10U); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  cd_size = pos - cd_offset;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static int make_zip_store_zip64_local_extra(uint8_t *zip, uint32_t *length)
{
  const char *name = "hello.txt";
  const char *data = "hello";
  uint32_t local_offset = 0U;
  uint32_t name_len = 9U;
  uint32_t data_len = 5U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  put_le16(zip + pos, 0x0001U); pos += 2U;
  put_le16(zip + pos, 16U); pos += 2U;
  put_le64(zip + pos, data_len); pos += 8U;
  put_le64(zip + pos, data_len); pos += 8U;
  memcpy(zip + pos, data, data_len); pos += data_len;

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 20U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  put_le16(zip + pos, 0x0001U); pos += 2U;
  put_le16(zip + pos, 16U); pos += 2U;
  put_le64(zip + pos, data_len); pos += 8U;
  put_le64(zip + pos, data_len); pos += 8U;
  cd_size = pos - cd_offset;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static int make_zip_store_zip64_offset_extra(uint8_t *zip, uint32_t *length)
{
  const char *name = "hello.txt";
  const char *data = "hello";
  uint32_t local_offset = 0U;
  uint32_t name_len = 9U;
  uint32_t data_len = 5U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  memcpy(zip + pos, data, data_len); pos += data_len;

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 28U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  put_le16(zip + pos, 0x0001U); pos += 2U;
  put_le16(zip + pos, 24U); pos += 2U;
  put_le64(zip + pos, data_len); pos += 8U;
  put_le64(zip + pos, data_len); pos += 8U;
  put_le64(zip + pos, local_offset); pos += 8U;
  cd_size = pos - cd_offset;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le16(zip + pos, 1U); pos += 2U;
  put_le32(zip + pos, cd_size); pos += 4U;
  put_le32(zip + pos, cd_offset); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static int make_zip_store_zip64_eocd(uint8_t *zip, uint32_t *length)
{
  const char *name = "hello.txt";
  const char *data = "hello";
  uint32_t local_offset = 0U;
  uint32_t name_len = 9U;
  uint32_t data_len = 5U;
  uint32_t cd_offset;
  uint32_t cd_size;
  uint32_t zip64_eocd_offset;
  uint32_t pos = 0U;

  put_le32(zip + pos, 0x04034b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  memcpy(zip + pos, data, data_len); pos += data_len;

  cd_offset = pos;
  put_le32(zip + pos, 0x02014b50UL); pos += 4U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0x3610a686UL); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le32(zip + pos, data_len); pos += 4U;
  put_le16(zip + pos, (uint16_t)name_len); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, local_offset); pos += 4U;
  memcpy(zip + pos, name, name_len); pos += name_len;
  cd_size = pos - cd_offset;

  zip64_eocd_offset = pos;
  put_le32(zip + pos, 0x06064b50UL); pos += 4U;
  put_le64(zip + pos, 44U); pos += 8U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le16(zip + pos, 45U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le64(zip + pos, 1U); pos += 8U;
  put_le64(zip + pos, 1U); pos += 8U;
  put_le64(zip + pos, cd_size); pos += 8U;
  put_le64(zip + pos, cd_offset); pos += 8U;

  put_le32(zip + pos, 0x07064b50UL); pos += 4U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le64(zip + pos, zip64_eocd_offset); pos += 8U;
  put_le32(zip + pos, 1U); pos += 4U;

  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0xffffU); pos += 2U;
  put_le16(zip + pos, 0xffffU); pos += 2U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le32(zip + pos, 0xffffffffUL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;

  *length = pos;
  return 1;
}

static int make_zip_empty(uint8_t *zip, uint32_t *length)
{
  uint32_t pos = 0U;

  memset(zip, 0, 22U);
  put_le32(zip + pos, 0x06054b50UL); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le16(zip + pos, 0U); pos += 2U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le32(zip + pos, 0U); pos += 4U;
  put_le16(zip + pos, 0U); pos += 2U;
  *length = pos;
  return 1;
}

static void put_tar_octal(uint8_t *dst, uint32_t width, uint32_t value)
{
  char tmp[16];

  memset(dst, '0', width);
  snprintf(tmp, sizeof(tmp), "%0*lo", (int)width - 1,
           (unsigned long)value);
  memcpy(dst, tmp, strlen(tmp));
  dst[width - 1U] = '\0';
}

static void put_tar_base256(uint8_t *dst, uint32_t width, uint32_t value)
{
  uint32_t i;

  memset(dst, 0, width);
  dst[0] = 0x80U;
  for (i = 0U; i < 4U && i < width; i++) {
    dst[width - 1U - i] = (uint8_t)((value >> (i * 8U)) & 0xffU);
  }
}

static void finish_tar_header(uint8_t *header)
{
  uint32_t i;
  uint32_t sum = 0U;

  memset(header + 148U, ' ', 8U);
  for (i = 0U; i < 512U; i++) {
    sum += (uint32_t)header[i];
  }
  put_tar_octal(header + 148U, 8U, sum);
}

static void finish_tar_header_signed_checksum(uint8_t *header)
{
  uint32_t i;
  int32_t sum = 0;

  memset(header + 148U, ' ', 8U);
  for (i = 0U; i < 512U; i++) {
    int8_t value = (int8_t)header[i];
    sum += (int32_t)value;
  }
  put_tar_octal(header + 148U, 8U, (uint32_t)sum);
}

static int make_tar(uint8_t *tar, uint32_t *length)
{
  const char *name = "dir/file.txt";
  const char *data = "hello";
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, name, strlen(name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 5U);
  tar[156U] = '0';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, data, 5U);
  *length = 1536U;
  return 1;
}

static int make_tar_legacy(uint8_t *tar, uint32_t *length)
{
  const char *name = "legacy/file.txt";
  const char *data = "hello";
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, name, strlen(name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 5U);
  tar[156U] = '0';
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, data, 5U);
  *length = 1536U;
  return 1;
}

static int make_tar_single_file(uint8_t *tar, uint32_t *length,
                                const char *name)
{
  const char *data = "hello";
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, name, strlen(name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 5U);
  tar[156U] = '0';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, data, 5U);
  *length = 1536U;
  return 1;
}

static int make_tar_base256_size_file(uint8_t *tar, uint32_t *length,
                                      const char *name)
{
  const char *data = "hello";
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, name, strlen(name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_base256(tar + 124U, 12U, 5U);
  tar[156U] = '0';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, data, 5U);
  *length = 1536U;
  return 1;
}

static int make_tar_signed_checksum_file(uint8_t *tar, uint32_t *length,
                                         const char *name)
{
  const char *data = "hello";
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, name, strlen(name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 5U);
  tar[156U] = '0';
  memcpy(tar + 257U, "ustar", 5U);
  tar[329U] = 0xffU;
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header_signed_checksum(tar);
  memcpy(tar + 512U, data, 5U);
  *length = 1536U;
  return 1;
}

static int make_tar_current_dir_prefixed_file(uint8_t *tar, uint32_t *length,
                                              const char *name)
{
  const char *data = "hello";
  char prefixed[ZZ9K_ARCHIVE_MAX_NAME];
  uint32_t i;

  memset(tar, 0, 2560U);
  memcpy(tar, "./", 2U);
  memcpy(tar + 100U, "0000755", 7U);
  put_tar_octal(tar + 124U, 12U, 0U);
  tar[156U] = '5';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);

  snprintf(prefixed, sizeof(prefixed), "./%s", name);
  memcpy(tar + 512U, prefixed, strlen(prefixed));
  memcpy(tar + 512U + 100U, "0000644", 7U);
  put_tar_octal(tar + 512U + 124U, 12U, 5U);
  tar[512U + 156U] = '0';
  memcpy(tar + 512U + 257U, "ustar", 5U);
  for (i = 512U + 148U; i < 512U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 512U);
  memcpy(tar + 1024U, data, 5U);
  *length = 2560U;
  return 1;
}

static int make_tar_current_dir_component_file(uint8_t *tar,
                                               uint32_t *length)
{
  const char *data = "hello";
  const char *name = "dir/./file.txt";
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, name, strlen(name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 5U);
  tar[156U] = '0';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, data, 5U);
  *length = 1536U;
  return 1;
}

static int make_tar_duplicate_slash_file(uint8_t *tar, uint32_t *length)
{
  const char *data = "hello";
  const char *name = "dir//file.txt";
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, name, strlen(name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 5U);
  tar[156U] = '0';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, data, 5U);
  *length = 1536U;
  return 1;
}

static int make_tar_trailing_slash_directory(uint8_t *tar, uint32_t *length)
{
  const char *dir = "archive_tool_tar_trailing_dir/";
  const char *file = "archive_tool_tar_trailing_dir/file.txt";
  const char *data = "hello";
  uint32_t i;

  memset(tar, 0, 2560U);
  memcpy(tar, dir, strlen(dir));
  memcpy(tar + 100U, "0000755", 7U);
  put_tar_octal(tar + 124U, 12U, 0U);
  tar[156U] = '0';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);

  memcpy(tar + 512U, file, strlen(file));
  memcpy(tar + 512U + 100U, "0000644", 7U);
  put_tar_octal(tar + 512U + 124U, 12U, 5U);
  tar[512U + 156U] = '0';
  memcpy(tar + 512U + 257U, "ustar", 5U);
  for (i = 512U + 148U; i < 512U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 512U);
  memcpy(tar + 1024U, data, 5U);
  *length = 2560U;
  return 1;
}

static int make_tar_gnu_long_name_file(uint8_t *tar, uint32_t *length,
                                       const char *name)
{
  const char *data = "hello";
  const char *long_link = "././@LongLink";
  uint32_t long_len = (uint32_t)strlen(name) + 1U;
  uint32_t i;

  memset(tar, 0, 3072U);
  memcpy(tar, long_link, strlen(long_link));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, long_len);
  tar[156U] = 'L';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, name, strlen(name) + 1U);

  memcpy(tar + 1024U, "short", 5U);
  memcpy(tar + 1024U + 100U, "0000644", 7U);
  put_tar_octal(tar + 1024U + 124U, 12U, 5U);
  tar[1024U + 156U] = '0';
  memcpy(tar + 1024U + 257U, "ustar", 5U);
  for (i = 1024U + 148U; i < 1024U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 1024U);
  memcpy(tar + 1536U, data, 5U);
  *length = 2560U;
  return 1;
}

static int make_tar_root_current_dir_metadata(uint8_t *tar, uint32_t *length)
{
  uint32_t i;

  memset(tar, 0, 1536U);
  memcpy(tar, "./", 2U);
  memcpy(tar + 100U, "0000755", 7U);
  put_tar_octal(tar + 124U, 12U, 0U);
  tar[156U] = '5';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  *length = 1536U;
  return 1;
}

static int make_tar_gnu_long_root_current_dir_metadata(uint8_t *tar,
                                                       uint32_t *length)
{
  uint32_t i;

  memset(tar, 0, 2048U);
  memcpy(tar, "././@LongLink", 12U);
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 3U);
  tar[156U] = 'L';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);
  memcpy(tar + 512U, "./", 3U);

  memcpy(tar + 1024U, "short", 5U);
  memcpy(tar + 1024U + 100U, "0000755", 7U);
  put_tar_octal(tar + 1024U + 124U, 12U, 0U);
  tar[1024U + 156U] = '5';
  memcpy(tar + 1024U + 257U, "ustar", 5U);
  for (i = 1024U + 148U; i < 1024U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 1024U);
  *length = 2048U;
  return 1;
}

static uint32_t put_pax_record(uint8_t *dst, const char *key,
                               const char *value)
{
  char payload[ZZ9K_ARCHIVE_MAX_NAME + 8U];
  char prefix[16];
  uint32_t payload_len;
  uint32_t digits = 1U;
  uint32_t len;

  snprintf(payload, sizeof(payload), "%s=%s\n", key, value);
  payload_len = (uint32_t)strlen(payload);
  for (;;) {
    uint32_t value;
    uint32_t next_digits = 0U;

    len = digits + 1U + payload_len;
    value = len;
    do {
      next_digits++;
      value /= 10U;
    } while (value != 0U);
    if (next_digits == digits) {
      break;
    }
    digits = next_digits;
  }
  snprintf(prefix, sizeof(prefix), "%lu ", (unsigned long)len);
  memcpy(dst, prefix, strlen(prefix));
  memcpy(dst + strlen(prefix), payload, payload_len);
  return len;
}

static uint32_t put_pax_large_comment_record(uint8_t *dst,
                                             uint32_t value_len)
{
  char prefix[16];
  uint32_t payload_len;
  uint32_t digits = 1U;
  uint32_t len;

  payload_len = 8U + value_len + 1U;
  for (;;) {
    uint32_t value;
    uint32_t next_digits = 0U;

    len = digits + 1U + payload_len;
    value = len;
    do {
      next_digits++;
      value /= 10U;
    } while (value != 0U);
    if (next_digits == digits) {
      break;
    }
    digits = next_digits;
  }
  snprintf(prefix, sizeof(prefix), "%lu ", (unsigned long)len);
  memcpy(dst, prefix, strlen(prefix));
  memcpy(dst + strlen(prefix), "comment=", 8U);
  memset(dst + strlen(prefix) + 8U, 'a', value_len);
  dst[len - 1U] = '\n';
  return len;
}

static uint32_t put_pax_path_record(uint8_t *dst, const char *name)
{
  return put_pax_record(dst, "path", name);
}

static int make_tar_pax_path_file(uint8_t *tar, uint32_t *length,
                                  const char *name)
{
  const char *data = "hello";
  uint32_t pax_len;
  uint32_t i;

  memset(tar, 0, 3072U);
  memcpy(tar, "PaxHeaders/path", 15U);
  memcpy(tar + 100U, "0000644", 7U);
  pax_len = put_pax_path_record(tar + 512U, name);
  put_tar_octal(tar + 124U, 12U, pax_len);
  tar[156U] = 'x';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);

  memcpy(tar + 1024U, "short", 5U);
  memcpy(tar + 1024U + 100U, "0000644", 7U);
  put_tar_octal(tar + 1024U + 124U, 12U, 5U);
  tar[1024U + 156U] = '0';
  memcpy(tar + 1024U + 257U, "ustar", 5U);
  for (i = 1024U + 148U; i < 1024U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 1024U);
  memcpy(tar + 1536U, data, 5U);
  *length = 2560U;
  return 1;
}

static int make_tar_pax_root_current_dir_metadata(uint8_t *tar,
                                                  uint32_t *length)
{
  uint32_t pax_len;
  uint32_t i;

  memset(tar, 0, 2048U);
  memcpy(tar, "PaxHeaders/root", 15U);
  memcpy(tar + 100U, "0000644", 7U);
  pax_len = put_pax_path_record(tar + 512U, "./");
  put_tar_octal(tar + 124U, 12U, pax_len);
  tar[156U] = 'x';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);

  memcpy(tar + 1024U, "short", 5U);
  memcpy(tar + 1024U + 100U, "0000755", 7U);
  put_tar_octal(tar + 1024U + 124U, 12U, 0U);
  tar[1024U + 156U] = '5';
  memcpy(tar + 1024U + 257U, "ustar", 5U);
  for (i = 1024U + 148U; i < 1024U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 1024U);
  *length = 2048U;
  return 1;
}

static int make_tar_pax_size_file(uint8_t *tar, uint32_t *length,
                                  const char *name)
{
  const char *data = "hello";
  uint32_t pax_len = 0U;
  uint32_t i;

  memset(tar, 0, 3072U);
  memcpy(tar, "PaxHeaders/size", 15U);
  memcpy(tar + 100U, "0000644", 7U);
  pax_len += put_pax_path_record(tar + 512U + pax_len, name);
  pax_len += put_pax_record(tar + 512U + pax_len, "size", "5");
  put_tar_octal(tar + 124U, 12U, pax_len);
  tar[156U] = 'x';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);

  memcpy(tar + 1024U, "short", 5U);
  memcpy(tar + 1024U + 100U, "0000644", 7U);
  put_tar_octal(tar + 1024U + 124U, 12U, 0U);
  tar[1024U + 156U] = '0';
  memcpy(tar + 1024U + 257U, "ustar", 5U);
  for (i = 1024U + 148U; i < 1024U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 1024U);
  memcpy(tar + 1536U, data, 5U);
  *length = 2560U;
  return 1;
}

static int make_tar_large_pax_size_file(uint8_t *tar, uint32_t *length,
                                        const char *name)
{
  const char *data = "hello";
  uint32_t pax_len = 0U;
  uint32_t i;

  memset(tar, 0, 4096U);
  memcpy(tar, "PaxHeaders/large", 16U);
  memcpy(tar + 100U, "0000644", 7U);
  pax_len += put_pax_path_record(tar + 512U + pax_len, name);
  pax_len += put_pax_large_comment_record(tar + 512U + pax_len, 620U);
  pax_len += put_pax_record(tar + 512U + pax_len, "size", "5");
  put_tar_octal(tar + 124U, 12U, pax_len);
  tar[156U] = 'x';
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);

  memcpy(tar + 1536U, "short", 5U);
  memcpy(tar + 1536U + 100U, "0000644", 7U);
  put_tar_octal(tar + 1536U + 124U, 12U, 0U);
  tar[1536U + 156U] = '0';
  memcpy(tar + 1536U + 257U, "ustar", 5U);
  for (i = 1536U + 148U; i < 1536U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 1536U);
  memcpy(tar + 2048U, data, 5U);
  *length = 3072U;
  return 1;
}

static int make_tar_special_then_file(uint8_t *tar, uint32_t *length,
                                      const char *special_name,
                                      const char *file_name)
{
  const char *data = "hello";
  uint32_t i;

  memset(tar, 0, 3072U);
  memcpy(tar, special_name, strlen(special_name));
  memcpy(tar + 100U, "0000644", 7U);
  put_tar_octal(tar + 124U, 12U, 0U);
  tar[156U] = '2';
  memcpy(tar + 157U, "target", 6U);
  memcpy(tar + 257U, "ustar", 5U);
  for (i = 148U; i < 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar);

  memcpy(tar + 512U, file_name, strlen(file_name));
  memcpy(tar + 512U + 100U, "0000644", 7U);
  put_tar_octal(tar + 512U + 124U, 12U, 5U);
  tar[512U + 156U] = '0';
  memcpy(tar + 512U + 257U, "ustar", 5U);
  for (i = 512U + 148U; i < 512U + 156U; i++) {
    tar[i] = ' ';
  }
  finish_tar_header(tar + 512U);
  memcpy(tar + 1024U, data, 5U);
  *length = 2048U;
  return 1;
}

static int make_lzma_alone(uint8_t *lzma, uint32_t *length,
                            uint64_t unpacked_size)
{
  memset(lzma, 0, 32U);
  lzma[0] = 0x5d;
  put_le32(lzma + 1U, 0x00800000UL);
  put_le64(lzma + 5U, unpacked_size);
  lzma[13U] = 0x00;
  *length = 14U;
  return 1;
}

static uint32_t put_7z_number(uint8_t *dst, uint64_t value)
{
  if (value < 0x80U) {
    dst[0] = (uint8_t)value;
    return 1U;
  }
  return 0U;
}

static void append_utf16_ascii(uint8_t *dst, uint32_t *pos,
                               const char *text)
{
  while (*text != '\0') {
    dst[(*pos)++] = (uint8_t)*text++;
    dst[(*pos)++] = 0U;
  }
  dst[(*pos)++] = 0U;
  dst[(*pos)++] = 0U;
}

static void finish_7z_start_header(uint8_t *archive)
{
  uint32_t header_offset;
  uint32_t header_size;
  uint32_t header_crc;
  uint32_t start_crc;

  header_offset = 32U + (uint32_t)zz9k_archive_get_le64(archive + 12U);
  header_size = (uint32_t)zz9k_archive_get_le64(archive + 20U);
  header_crc = zz9k_archive_crc32(0U, archive + header_offset, header_size);
  put_le32(archive + 28U, header_crc);
  start_crc = zz9k_archive_crc32(0U, archive + 12U, 20U);
  put_le32(archive + 8U, start_crc);
}

static int make_7z_empty_files(uint8_t *archive, uint32_t *length)
{
  uint8_t header[128];
  uint8_t names[64];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "dir/");
  append_utf16_ascii(names, &names_len, "empty.txt");

  header[pos++] = 0x01U; /* kHeader */
  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 2U);

  header[pos++] = 0x0eU; /* kEmptyStream */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0xc0U;

  header[pos++] = 0x0fU; /* kEmptyFile */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x40U;

  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;

  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 192U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, 0U);
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + header_len;
  return 1;
}

static int make_7z_zero_files(uint8_t *archive, uint32_t *length)
{
  uint8_t header[16];
  uint32_t pos = 0U;
  uint32_t header_len;

  header[pos++] = 0x01U; /* kHeader */
  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 0U);
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 64U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, 0U);
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + header_len;
  return 1;
}

static int make_7z_root_current_dir_metadata(uint8_t *archive,
                                             uint32_t *length)
{
  uint8_t header[64];
  uint8_t names[16];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "./");

  header[pos++] = 0x01U; /* kHeader */
  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 1U);

  header[pos++] = 0x0eU; /* kEmptyStream */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x80U;

  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;

  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 96U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, 0U);
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + header_len;
  return 1;
}

static int make_7z_root_current_dir_then_copy_file(uint8_t *archive,
                                                   uint32_t *length)
{
  const uint8_t payload[5] = { 'h', 'e', 'l', 'l', 'o' };
  uint8_t header[192];
  uint8_t names[96];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "./");
  append_utf16_ascii(names, &names_len, "./dir/hello.txt");

  header[pos++] = 0x01U; /* kHeader */

  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U); /* NumCoders */
  header[pos++] = 1U; /* one-byte method id, simple coder */
  header[pos++] = 0U; /* Copy method */
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0x08U; /* kSubStreamsInfo */
  header[pos++] = 0x0aU; /* kCRC */
  header[pos++] = 1U; /* all digests defined */
  put_le32(header + pos, 0x3610a686UL);
  pos += 4U;
  header[pos++] = 0U; /* end SubStreamsInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */

  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 2U);
  header[pos++] = 0x0eU; /* kEmptyStream */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x80U;
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 288U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int make_7z_copy_file_named(uint8_t *archive, uint32_t *length,
                                   const char *name)
{
  const uint8_t payload[5] = { 'h', 'e', 'l', 'l', 'o' };
  uint8_t header[128];
  uint8_t names[64];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, name);

  header[pos++] = 0x01U; /* kHeader */

  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U); /* NumCoders */
  header[pos++] = 1U; /* one-byte method id, simple coder */
  header[pos++] = 0U; /* Copy method */
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0x08U; /* kSubStreamsInfo */
  header[pos++] = 0x0aU; /* kCRC */
  header[pos++] = 1U; /* all digests defined */
  put_le32(header + pos, 0x3610a686UL);
  pos += 4U;
  header[pos++] = 0U; /* end SubStreamsInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */

  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 192U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int make_7z_copy_file(uint8_t *archive, uint32_t *length)
{
  return make_7z_copy_file_named(archive, length, "hello.txt");
}

static int make_7z_copy_file_with_metadata(uint8_t *archive,
                                           uint32_t *length)
{
  make_7z_copy_file(archive, length);
  {
    uint8_t *header = archive + 32U + 5U;
    uint32_t header_len = (uint32_t)zz9k_archive_get_le64(archive + 20U);
    uint32_t insert = header_len - 2U;
    uint8_t metadata[32];
    uint32_t metadata_len = 0U;

    metadata[metadata_len++] = 0x14U; /* kMTime */
    metadata[metadata_len++] = 10U;
    metadata[metadata_len++] = 1U; /* all defined */
    metadata[metadata_len++] = 0U; /* inline */
    memset(metadata + metadata_len, 0, 8U);
    metadata_len += 8U;

    metadata[metadata_len++] = 0x15U; /* kWinAttributes */
    metadata[metadata_len++] = 6U;
    metadata[metadata_len++] = 1U; /* all defined */
    metadata[metadata_len++] = 0U; /* inline */
    put_le32(metadata + metadata_len, 0x20U);
    metadata_len += 4U;

    memmove(header + insert + metadata_len, header + insert,
            header_len - insert);
    memcpy(header + insert, metadata, metadata_len);
    put_le64(archive + 20U, header_len + metadata_len);
    *length += metadata_len;
    finish_7z_start_header(archive);
  }
  return 1;
}

static int make_7z_copy_file_with_unknown_metadata(uint8_t *archive,
                                                   uint32_t *length)
{
  make_7z_copy_file(archive, length);
  {
    uint8_t *header = archive + 32U + 5U;
    uint32_t header_len = (uint32_t)zz9k_archive_get_le64(archive + 20U);
    uint32_t insert = header_len - 2U;
    uint8_t metadata[3];

    metadata[0] = 0x7fU;
    metadata[1] = 1U;
    metadata[2] = 0U;
    memmove(header + insert + sizeof(metadata), header + insert,
            header_len - insert);
    memcpy(header + insert, metadata, sizeof(metadata));
    put_le64(archive + 20U, header_len + sizeof(metadata));
    *length += sizeof(metadata);
    finish_7z_start_header(archive);
  }
  return 1;
}

static int make_7z_copy_encoded_header_file(uint8_t *archive,
                                            uint32_t *length)
{
  const uint8_t payload[5] = { 'h', 'e', 'l', 'l', 'o' };
  uint8_t decoded_header[128];
  uint8_t encoded_header[64];
  uint8_t names[32];
  uint32_t pos = 0U;
  uint32_t encoded_pos = 0U;
  uint32_t names_len = 0U;
  uint32_t decoded_len;
  uint32_t pack_pos_index;
  uint32_t encoded_len;

  append_utf16_ascii(names, &names_len, "hello.txt");

  decoded_header[pos++] = 0x01U; /* kHeader */

  decoded_header[pos++] = 0x04U; /* kMainStreamsInfo */
  decoded_header[pos++] = 0x06U; /* kPackInfo */
  pack_pos_index = pos;
  pos += put_7z_number(decoded_header + pos, 0U); /* patched below */
  pos += put_7z_number(decoded_header + pos, 1U); /* NumPackStreams */
  decoded_header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(decoded_header + pos, sizeof(payload));
  decoded_header[pos++] = 0U; /* end PackInfo */
  decoded_header[pos++] = 0x07U; /* kUnPackInfo */
  decoded_header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(decoded_header + pos, 1U); /* NumFolders */
  decoded_header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(decoded_header + pos, 1U); /* NumCoders */
  decoded_header[pos++] = 1U; /* one-byte method id, simple coder */
  decoded_header[pos++] = 0U; /* Copy method */
  decoded_header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(decoded_header + pos, sizeof(payload));
  decoded_header[pos++] = 0U; /* end UnPackInfo */
  decoded_header[pos++] = 0x08U; /* kSubStreamsInfo */
  decoded_header[pos++] = 0x0aU; /* kCRC */
  decoded_header[pos++] = 1U; /* all digests defined */
  put_le32(decoded_header + pos, 0x3610a686UL);
  pos += 4U;
  decoded_header[pos++] = 0U; /* end SubStreamsInfo */
  decoded_header[pos++] = 0U; /* end MainStreamsInfo */

  decoded_header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(decoded_header + pos, 1U);
  decoded_header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(decoded_header + pos, 1U + names_len);
  decoded_header[pos++] = 0U; /* inline names, not external */
  memcpy(decoded_header + pos, names, names_len);
  pos += names_len;
  decoded_header[pos++] = 0U; /* end FilesInfo properties */
  decoded_header[pos++] = 0U; /* end Header */
  decoded_len = pos;
  decoded_header[pack_pos_index] = (uint8_t)decoded_len;

  encoded_header[encoded_pos++] = 0x17U; /* kEncodedHeader */
  encoded_header[encoded_pos++] = 0x06U; /* kPackInfo */
  encoded_pos += put_7z_number(encoded_header + encoded_pos, 0U);
  encoded_pos += put_7z_number(encoded_header + encoded_pos, 1U);
  encoded_header[encoded_pos++] = 0x09U; /* kSize */
  encoded_pos += put_7z_number(encoded_header + encoded_pos, decoded_len);
  encoded_header[encoded_pos++] = 0U; /* end PackInfo */
  encoded_header[encoded_pos++] = 0x07U; /* kUnPackInfo */
  encoded_header[encoded_pos++] = 0x0bU; /* kFolder */
  encoded_pos += put_7z_number(encoded_header + encoded_pos, 1U);
  encoded_header[encoded_pos++] = 0U; /* inline folders */
  encoded_pos += put_7z_number(encoded_header + encoded_pos, 1U);
  encoded_header[encoded_pos++] = 1U; /* one-byte method id, simple coder */
  encoded_header[encoded_pos++] = 0U; /* Copy method */
  encoded_header[encoded_pos++] = 0x0cU; /* kCodersUnPackSize */
  encoded_pos += put_7z_number(encoded_header + encoded_pos, decoded_len);
  encoded_header[encoded_pos++] = 0x0aU; /* kCRC */
  encoded_header[encoded_pos++] = 1U; /* all digests defined */
  put_le32(encoded_header + encoded_pos,
           zz9k_archive_crc32(0U, decoded_header, decoded_len));
  encoded_pos += 4U;
  encoded_header[encoded_pos++] = 0U; /* end UnPackInfo */
  encoded_header[encoded_pos++] = 0U; /* end StreamsInfo */
  encoded_len = encoded_pos;

  memset(archive, 0, 256U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, decoded_len + sizeof(payload));
  put_le64(archive + 20U, encoded_len);
  memcpy(archive + 32U, decoded_header, decoded_len);
  memcpy(archive + 32U + decoded_len, payload, sizeof(payload));
  memcpy(archive + 32U + decoded_len + sizeof(payload),
         encoded_header, encoded_len);
  finish_7z_start_header(archive);
  *length = 32U + decoded_len + sizeof(payload) + encoded_len;
  return 1;
}

static int make_7z_lzma_encoded_header_descriptor(uint8_t *header,
                                                  uint32_t *length)
{
  const uint8_t props[5] = { 0x5dU, 0x00U, 0x00U, 0x80U, 0x00U };
  uint32_t pos = 0U;

  header[pos++] = 0x17U; /* kEncodedHeader */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U);
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, 6U);
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x23U; /* three-byte method id + attributes */
  header[pos++] = 0x03U;
  header[pos++] = 0x01U;
  header[pos++] = 0x01U;
  pos += put_7z_number(header + pos, sizeof(props));
  memcpy(header + pos, props, sizeof(props));
  pos += sizeof(props);
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, 42U);
  header[pos++] = 0x0aU; /* kCRC */
  header[pos++] = 1U; /* all digests defined */
  put_le32(header + pos, 0x12345678UL);
  pos += 4U;
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0U; /* end StreamsInfo */
  *length = pos;
  return 1;
}

static int make_7z_copy_two_file_substream(uint8_t *archive,
                                           uint32_t *length)
{
  const uint8_t payload[8] = {
    'h', 'e', 'l', 'l', 'o', 'b', 'y', 'e'
  };
  const uint8_t hello[5] = { 'h', 'e', 'l', 'l', 'o' };
  const uint8_t bye[3] = { 'b', 'y', 'e' };
  uint8_t header[192];
  uint8_t names[64];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "hello.txt");
  append_utf16_ascii(names, &names_len, "bye.txt");

  header[pos++] = 0x01U; /* kHeader */

  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U); /* NumCoders */
  header[pos++] = 1U; /* one-byte method id, simple coder */
  header[pos++] = 0U; /* Copy method */
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0x08U; /* kSubStreamsInfo */
  header[pos++] = 0x0dU; /* kNumUnPackStream */
  pos += put_7z_number(header + pos, 2U);
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(hello));
  header[pos++] = 0x0aU; /* kCRC */
  header[pos++] = 1U; /* all digests defined */
  put_le32(header + pos, zz9k_archive_crc32(0U, hello, sizeof(hello)));
  pos += 4U;
  put_le32(header + pos, zz9k_archive_crc32(0U, bye, sizeof(bye)));
  pos += 4U;
  header[pos++] = 0U; /* end SubStreamsInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */

  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 2U);
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 256U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int make_7z_deflate_two_file_substream(uint8_t *archive,
                                              uint32_t *length)
{
  const uint8_t payload[10] = {
    0xcbU, 0x48U, 0xcdU, 0xc9U, 0xc9U,
    0x4fU, 0xaaU, 0x4cU, 0x05U, 0x00U
  };
  const uint8_t hello[5] = { 'h', 'e', 'l', 'l', 'o' };
  const uint8_t bye[3] = { 'b', 'y', 'e' };
  uint8_t header[192];
  uint8_t names[64];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "hello.txt");
  append_utf16_ascii(names, &names_len, "bye.txt");

  header[pos++] = 0x01U; /* kHeader */

  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U); /* NumCoders */
  header[pos++] = 0x03U; /* three-byte method id */
  header[pos++] = 0x04U;
  header[pos++] = 0x01U;
  header[pos++] = 0x08U;
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, sizeof(hello) + sizeof(bye));
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0x08U; /* kSubStreamsInfo */
  header[pos++] = 0x0dU; /* kNumUnPackStream */
  pos += put_7z_number(header + pos, 2U);
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(hello));
  header[pos++] = 0x0aU; /* kCRC */
  header[pos++] = 1U; /* all digests defined */
  put_le32(header + pos, zz9k_archive_crc32(0U, hello, sizeof(hello)));
  pos += 4U;
  put_le32(header + pos, zz9k_archive_crc32(0U, bye, sizeof(bye)));
  pos += 4U;
  header[pos++] = 0U; /* end SubStreamsInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */

  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 2U);
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 256U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int make_7z_lzma_file(uint8_t *archive, uint32_t *length)
{
  const uint8_t payload[6] = { 0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U };
  const uint8_t props[5] = { 0x5dU, 0x00U, 0x00U, 0x80U, 0x00U };
  uint8_t header[160];
  uint8_t names[32];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "packed.bin");

  header[pos++] = 0x01U; /* kHeader */

  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U); /* NumCoders */
  header[pos++] = 0x23U; /* three-byte method id + attributes */
  header[pos++] = 0x03U;
  header[pos++] = 0x01U;
  header[pos++] = 0x01U;
  pos += put_7z_number(header + pos, sizeof(props));
  memcpy(header + pos, props, sizeof(props));
  pos += sizeof(props);
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, 11U);
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */

  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 224U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int make_7z_deflate_file(uint8_t *archive, uint32_t *length)
{
  const uint8_t payload[5] = { 0x4bU, 0xcbU, 0xcfU, 0x07U, 0x00U };
  uint8_t header[160];
  uint8_t names[32];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "deflate.bin");

  header[pos++] = 0x01U; /* kHeader */

  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U); /* NumCoders */
  header[pos++] = 0x03U; /* three-byte method id */
  header[pos++] = 0x04U;
  header[pos++] = 0x01U;
  header[pos++] = 0x08U;
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, 3U);
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0x08U; /* kSubStreamsInfo */
  header[pos++] = 0x0aU; /* kCRC */
  header[pos++] = 1U; /* all digests defined */
  put_le32(header + pos, zz9k_archive_crc32(0U, (const uint8_t *)"foo", 3U));
  pos += 4U;
  header[pos++] = 0U; /* end SubStreamsInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */

  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 224U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int make_7z_lzma2_file(uint8_t *archive, uint32_t *length)
{
  const uint8_t payload[] = {
    0x01U, 0x00U, 0x4fU, 0x5aU, 0x5aU, 0x39U, 0x30U, 0x30U,
    0x30U, 0x20U, 0x53U, 0x44U, 0x4bU, 0x20U, 0x63U, 0x6fU,
    0x64U, 0x65U, 0x63U, 0x20U, 0x73U, 0x65U, 0x72U, 0x76U,
    0x69U, 0x63U, 0x65U, 0x3aU, 0x20U, 0x72U, 0x65U, 0x70U,
    0x65U, 0x61U, 0x74U, 0x61U, 0x62U, 0x6cU, 0x65U, 0x20U,
    0x4cU, 0x5aU, 0x4dU, 0x41U, 0x32U, 0x20U, 0x69U, 0x6eU,
    0x66U, 0x6cU, 0x61U, 0x74U, 0x65U, 0x20U, 0x70U, 0x61U,
    0x74U, 0x68U, 0x20U, 0x66U, 0x6fU, 0x72U, 0x20U, 0x6fU,
    0x72U, 0x64U, 0x69U, 0x6eU, 0x61U, 0x72U, 0x79U, 0x20U,
    0x37U, 0x7aU, 0x20U, 0x70U, 0x61U, 0x79U, 0x6cU, 0x6fU,
    0x61U, 0x64U, 0x73U, 0x00U
  };
  const uint8_t props[1] = { 0x10U };
  uint8_t header[224];
  uint8_t names[32];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "lzma2.bin");

  header[pos++] = 0x01U; /* kHeader */

  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  pos += put_7z_number(header + pos, 1U); /* NumCoders */
  header[pos++] = 0x21U; /* one-byte method id + attributes */
  header[pos++] = 0x21U; /* LZMA2 method */
  pos += put_7z_number(header + pos, sizeof(props));
  memcpy(header + pos, props, sizeof(props));
  pos += sizeof(props);
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, 80U);
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */

  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 256U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int make_7z_with_folder_bytes(uint8_t *archive,
                                     uint32_t *length,
                                     const uint8_t *folder,
                                     uint32_t folder_len)
{
  const uint8_t payload[1] = { 0U };
  uint8_t header[192];
  uint8_t names[32];
  uint32_t pos = 0U;
  uint32_t names_len = 0U;
  uint32_t header_len;

  append_utf16_ascii(names, &names_len, "packed.bin");

  header[pos++] = 0x01U; /* kHeader */
  header[pos++] = 0x04U; /* kMainStreamsInfo */
  header[pos++] = 0x06U; /* kPackInfo */
  pos += put_7z_number(header + pos, 0U); /* PackPos */
  pos += put_7z_number(header + pos, 1U); /* NumPackStreams */
  header[pos++] = 0x09U; /* kSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end PackInfo */
  header[pos++] = 0x07U; /* kUnPackInfo */
  header[pos++] = 0x0bU; /* kFolder */
  pos += put_7z_number(header + pos, 1U); /* NumFolders */
  header[pos++] = 0U; /* inline folders */
  memcpy(header + pos, folder, folder_len);
  pos += folder_len;
  header[pos++] = 0x0cU; /* kCodersUnPackSize */
  pos += put_7z_number(header + pos, sizeof(payload));
  header[pos++] = 0U; /* end UnPackInfo */
  header[pos++] = 0U; /* end MainStreamsInfo */
  header[pos++] = 0x05U; /* kFilesInfo */
  pos += put_7z_number(header + pos, 1U);
  header[pos++] = 0x11U; /* kName */
  pos += put_7z_number(header + pos, 1U + names_len);
  header[pos++] = 0U; /* inline names, not external */
  memcpy(header + pos, names, names_len);
  pos += names_len;
  header[pos++] = 0U; /* end FilesInfo properties */
  header[pos++] = 0U; /* end Header */
  header_len = pos;

  memset(archive, 0, 256U);
  archive[0] = 0x37U;
  archive[1] = 0x7aU;
  archive[2] = 0xbcU;
  archive[3] = 0xafU;
  archive[4] = 0x27U;
  archive[5] = 0x1cU;
  archive[6] = 0U;
  archive[7] = 4U;
  put_le64(archive + 12U, sizeof(payload));
  put_le64(archive + 20U, header_len);
  memcpy(archive + 32U, payload, sizeof(payload));
  memcpy(archive + 32U + sizeof(payload), header, header_len);
  finish_7z_start_header(archive);
  *length = 32U + sizeof(payload) + header_len;
  return 1;
}

static int test_detect_formats(void)
{
  uint8_t gzip_data[18] = {
    0x1f, 0x8b, 0x08, 0x00, 0, 0, 0, 0, 0, 0xff,
    0, 0, 0, 0, 5, 0, 0, 0
  };
  uint8_t seven_z[8] = {
    0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c, 0x00, 0x04
  };
  uint8_t lzma[32];
  uint8_t zip[160];
  uint8_t empty_zip[22];
  uint8_t tar[1536];
  uint8_t legacy_tar[1536];
  uint8_t empty_tar[1024];
  uint8_t lha[128];
  uint8_t lha_lh5[128];
  uint32_t lzma_len;
  uint32_t zip_len;
  uint32_t tar_len;
  uint32_t legacy_tar_len;
  uint32_t lha_len;
  uint32_t lha_lh5_len;

  make_lzma_alone(lzma, &lzma_len, 79U);
  make_zip_store(zip, &zip_len);
  make_zip_empty(empty_zip, &zip_len);
  make_tar(tar, &tar_len);
  make_tar_legacy(legacy_tar, &legacy_tar_len);
  make_lha_lh0(lha, &lha_len);
  make_lha_lh5_level1(lha_lh5, &lha_lh5_len);
  memset(empty_tar, 0, sizeof(empty_tar));

  if (zz9k_archive_detect_format(gzip_data, sizeof(gzip_data)) !=
      ZZ9K_ARCHIVE_FORMAT_GZIP) return 1;
  if (zz9k_archive_detect_format(seven_z, sizeof(seven_z)) !=
      ZZ9K_ARCHIVE_FORMAT_7Z) return 2;
  if (zz9k_archive_detect_format(lzma, lzma_len) !=
      ZZ9K_ARCHIVE_FORMAT_LZMA_ALONE) return 3;
  if (zz9k_archive_detect_format(zip, zip_len) !=
      ZZ9K_ARCHIVE_FORMAT_ZIP) return 4;
  if (zz9k_archive_detect_format(tar, tar_len) !=
      ZZ9K_ARCHIVE_FORMAT_TAR) return 5;
  if (zz9k_archive_detect_format(empty_zip, sizeof(empty_zip)) !=
      ZZ9K_ARCHIVE_FORMAT_ZIP) return 6;
  if (zz9k_archive_detect_format(legacy_tar, 512U) !=
      ZZ9K_ARCHIVE_FORMAT_TAR) return 7;
  if (zz9k_archive_detect_format(legacy_tar, legacy_tar_len) !=
      ZZ9K_ARCHIVE_FORMAT_TAR) return 8;
  if (zz9k_archive_detect_format(empty_tar, sizeof(empty_tar)) !=
      ZZ9K_ARCHIVE_FORMAT_TAR) return 9;
  if (zz9k_archive_detect_format(lha, lha_len) !=
      ZZ9K_ARCHIVE_FORMAT_LHA) return 10;
  if (zz9k_archive_detect_format(lha_lh5, lha_lh5_len) !=
      ZZ9K_ARCHIVE_FORMAT_LHA) return 11;
  return 0;
}

static int test_7z_empty_list(void)
{
  uint8_t archive[192];
  ZZ9KArchive7zHeader start;
  ZZ9KArchiveEntry entries[4];
  uint32_t archive_len;
  uint32_t count;

  make_7z_empty_files(archive, &archive_len);
  memset(&start, 0, sizeof(start));
  if (!zz9k_archive_7z_start_header(archive, archive_len, &start)) {
    return 1;
  }
  if (start.next_header_offset != 32U) return 2;
  if (start.next_header_size == 0U) return 3;

  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 4U, &count)) {
    return 4;
  }
  if (count != 2U) return 5;
  if (strcmp(entries[0].name, "dir/") != 0) return 6;
  if (!entries[0].is_dir) return 7;
  if (entries[0].uncompressed_size != 0U) return 8;
  if (strcmp(entries[1].name, "empty.txt") != 0) return 9;
  if (entries[1].is_dir) return 10;
  if (entries[1].uncompressed_size != 0U) return 11;
  return 0;
}

static int test_7z_zero_file_archive_uses_file_path(void)
{
  const char *path = "archive_tool_7z_zero_files.tmp";
  uint8_t archive[64];
  ZZ9KContext *ctx = 0;
  ZZ9KServiceInfo service;
  ZZ9KArchiveEntry entries[1];
  uint32_t archive_len;
  uint32_t count;
  int needs_deflate;
  int needs_lzma;
  int needs_lzma2;
  int codec_ready = 0;
  int attempted = 0;
  int rc = 0;

  make_7z_zero_files(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 1U, &count)) {
    return 1;
  }
  if (count != 0U) return 2;
  if (!zz9k_archive_7z_file_can_handle_entries(
          entries, count, archive_len, &needs_deflate,
          &needs_lzma, &needs_lzma2)) {
    return 3;
  }
  if (needs_deflate || needs_lzma || needs_lzma2) return 4;

  remove(path);
  if (!write_test_file(path, archive, archive_len)) return 5;
  memset(&service, 0, sizeof(service));
  if (!zz9k_archive_handle_7z_file(
          &ctx, &service, &codec_ready, "t", path, archive_len,
          0, &attempted)) {
    rc = 6;
    goto out;
  }
  if (!attempted) rc = 7;

out:
  remove(path);
  return rc;
}

static int test_7z_copy_list(void)
{
  uint8_t archive[192];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;

  make_7z_copy_file(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 4;
  if (entries[0].is_dir) return 5;
  if (entries[0].data_offset != 32U) return 6;
  if (entries[0].compressed_size != 5U) return 7;
  if (entries[0].uncompressed_size != 5U) return 8;
  if (memcmp(archive + entries[0].data_offset, "hello", 5U) != 0) return 9;
  if (entries[0].crc32 != 0x3610a686UL) return 10;
  if ((entries[0].flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) == 0U) return 11;
  return 0;
}

static int test_7z_current_dir_prefix_names_are_normalized(void)
{
  uint8_t archive[224];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;

  make_7z_copy_file_named(archive, &archive_len, "./dir/hello.txt");
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 5;
  if (entries[0].data_offset != 32U) return 6;
  if (entries[0].compressed_size != 5U) return 7;
  if (entries[0].uncompressed_size != 5U) return 8;
  return 0;
}

static int test_7z_current_dir_components_are_normalized(void)
{
  uint8_t archive[224];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;

  make_7z_copy_file_named(archive, &archive_len, "dir/./hello.txt");
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 5;
  if (entries[0].compressed_size != 5U) return 6;
  if (entries[0].uncompressed_size != 5U) return 7;
  return 0;
}

static int test_7z_duplicate_slashes_are_normalized(void)
{
  uint8_t archive[224];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;

  make_7z_copy_file_named(archive, &archive_len, "dir//hello.txt");
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 5;
  if (entries[0].compressed_size != 5U) return 6;
  if (entries[0].uncompressed_size != 5U) return 7;
  return 0;
}

static int test_7z_root_current_dir_metadata_is_skipped(void)
{
  uint8_t archive[96];
  ZZ9KArchiveEntry entries[1];
  uint32_t archive_len;
  uint32_t count = 123U;

  make_7z_root_current_dir_metadata(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 0U, &count)) {
    return 1;
  }
  if (count != 0U) return 2;
  count = 123U;
  if (!zz9k_archive_7z_list(archive, archive_len, 0, 0U, &count)) {
    return 3;
  }
  if (count != 0U) return 4;
  return 0;
}

static int test_7z_root_current_dir_metadata_does_not_use_output_slot(void)
{
  uint8_t archive[288];
  ZZ9KArchiveEntry entries[1];
  uint32_t archive_len;
  uint32_t count = 123U;

  make_7z_root_current_dir_then_copy_file(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 1U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 5;
  if (entries[0].data_offset != 32U) return 6;
  if (entries[0].compressed_size != 5U) return 7;
  if (entries[0].uncompressed_size != 5U) return 8;

  count = 123U;
  if (!zz9k_archive_7z_list(archive, archive_len, 0, 0U, &count)) {
    return 9;
  }
  if (count != 1U) return 10;
  return 0;
}

static int test_7z_copy_skips_common_file_metadata(void)
{
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;

  make_7z_copy_file_with_metadata(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 4;
  if (entries[0].data_offset != 32U) return 5;
  if (entries[0].compressed_size != 5U) return 6;
  if (entries[0].uncompressed_size != 5U) return 7;
  if (!zz9k_archive_7z_copy_entry_crc_matches(
          &entries[0], archive + entries[0].data_offset)) {
    return 8;
  }
  return 0;
}

static int test_7z_copy_rejects_unknown_file_metadata(void)
{
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;

  make_7z_copy_file_with_unknown_metadata(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  return 0;
}

static int test_7z_copy_encoded_header_list(void)
{
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[2];
  uint8_t *header;
  uint32_t archive_len;
  uint32_t header_len;
  uint32_t count;

  make_7z_copy_encoded_header_file(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_header_is_encoded(archive, archive_len)) {
    return 1;
  }
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 2;
  }
  if (count != 1U) return 3;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 4;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 5;
  if (entries[0].compressed_size != 5U) return 6;
  if (entries[0].uncompressed_size != 5U) return 7;
  if (entries[0].data_offset <= 32U ||
      entries[0].data_offset + entries[0].compressed_size > archive_len) {
    return 8;
  }
  if (!zz9k_archive_7z_copy_entry_crc_matches(
          &entries[0], archive + entries[0].data_offset)) {
    return 9;
  }
  if (!write_test_file("archive_tool_7z_encoded_header.tmp",
                       archive, archive_len)) {
    return 10;
  }
  header = 0;
  header_len = 0U;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_read_header_from_file(
          "archive_tool_7z_encoded_header.tmp", archive_len,
          &header, &header_len)) {
    remove("archive_tool_7z_encoded_header.tmp");
    return 11;
  }
  if (header_len == 0U ||
      header[0] == ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER) {
    free(header);
    remove("archive_tool_7z_encoded_header.tmp");
    return 12;
  }
  if (!zz9k_archive_7z_list_from_header(
          header, header_len, archive_len, entries, 2U, &count)) {
    free(header);
    remove("archive_tool_7z_encoded_header.tmp");
    return 13;
  }
  free(header);
  remove("archive_tool_7z_encoded_header.tmp");
  if (count != 1U) return 14;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 15;
  if (entries[0].data_offset <= 32U ||
      entries[0].data_offset + entries[0].compressed_size > archive_len) {
    return 16;
  }
  return 0;
}

static int test_7z_copy_encoded_header_rejects_bad_crc(void)
{
  const char *path = "archive_tool_7z_bad_encoded_header_crc.tmp";
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[2];
  uint8_t *header = 0;
  uint32_t archive_len;
  uint32_t count;
  uint32_t decoded_len;
  uint32_t i;
  int corrupted = 0;
  int rc = 0;

  make_7z_copy_encoded_header_file(archive, &archive_len);
  decoded_len = (uint32_t)zz9k_archive_get_le64(archive + 12U) - 5U;
  for (i = 0U; i + 3U < decoded_len; i++) {
    if (archive[32U + i] == (uint8_t)'h' &&
        archive[32U + i + 1U] == 0U &&
        archive[32U + i + 2U] == (uint8_t)'e' &&
        archive[32U + i + 3U] == 0U) {
      archive[32U + i] = (uint8_t)'j';
      corrupted = 1;
      break;
    }
  }
  if (!corrupted) {
    return 1;
  }
  if (zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 2;
  }

  remove(path);
  if (!write_test_file(path, archive, archive_len)) return 3;
  if (zz9k_archive_7z_read_header_from_file(
          path, archive_len, &header, &count)) {
    rc = 4;
    goto out;
  }

out:
  free(header);
  remove(path);
  return rc;
}

static int test_7z_encoded_header_entry_exposes_codec_method(void)
{
  uint8_t header[80];
  ZZ9KArchiveEntry entry;
  uint32_t header_len;

  make_7z_lzma_encoded_header_descriptor(header, &header_len);
  memset(&entry, 0, sizeof(entry));
  if (!zz9k_archive_7z_encoded_header_entry(
          header, header_len, 128U, &entry)) {
    return 1;
  }
  if (entry.method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA) return 2;
  if (entry.data_offset != ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE) return 3;
  if (entry.compressed_size != 6U) return 4;
  if (entry.uncompressed_size != 42U) return 5;
  if (entry.method_props_size != 5U) return 6;
  if (entry.method_props[0] != 0x5dU) return 7;
  if (!zz9k_archive_entry_has_crc32(&entry)) return 8;
  if (entry.crc32 != 0x12345678UL) return 9;
  return 0;
}

static int test_7z_multi_coder_reports_parse_diagnostic(void)
{
  uint8_t archive[256];
  uint8_t folder[8];
  uint32_t folder_len = 0U;
  uint32_t archive_len;
  uint32_t count = 0U;
  const char *diagnostic;

  folder_len += put_7z_number(folder + folder_len, 2U); /* NumCoders */
  make_7z_with_folder_bytes(archive, &archive_len, folder, folder_len);
  if (zz9k_archive_7z_list(archive, archive_len, 0, 0U, &count)) {
    return 1;
  }
  diagnostic = zz9k_archive_7z_parse_diagnostic();
  if (!diagnostic) return 2;
  if (strcmp(diagnostic,
             "7z multi-coder/filter-chain folders unsupported") != 0) {
    return 3;
  }
  return 0;
}

static int test_7z_multiple_stream_coder_reports_parse_diagnostic(void)
{
  uint8_t archive[256];
  uint8_t folder[16];
  uint32_t folder_len = 0U;
  uint32_t archive_len;
  uint32_t count = 0U;
  const char *diagnostic;

  folder_len += put_7z_number(folder + folder_len, 1U); /* NumCoders */
  folder[folder_len++] = 0x13U; /* three-byte method id + stream counts */
  folder[folder_len++] = 0x04U;
  folder[folder_len++] = 0x01U;
  folder[folder_len++] = 0x08U;
  folder_len += put_7z_number(folder + folder_len, 2U); /* in streams */
  folder_len += put_7z_number(folder + folder_len, 1U); /* out streams */
  make_7z_with_folder_bytes(archive, &archive_len, folder, folder_len);
  if (zz9k_archive_7z_list(archive, archive_len, 0, 0U, &count)) {
    return 1;
  }
  diagnostic = zz9k_archive_7z_parse_diagnostic();
  if (!diagnostic) return 2;
  if (strcmp(diagnostic,
             "7z multiple input/output streams unsupported") != 0) {
    return 3;
  }
  return 0;
}

static int test_7z_unsupported_method_reports_parse_diagnostic(void)
{
  uint8_t archive[256];
  uint8_t folder[16];
  uint32_t folder_len = 0U;
  uint32_t archive_len;
  uint32_t count = 0U;
  const char *diagnostic;

  folder_len += put_7z_number(folder + folder_len, 1U); /* NumCoders */
  folder[folder_len++] = 0x03U; /* three-byte unsupported method id */
  folder[folder_len++] = 0x04U;
  folder[folder_len++] = 0x02U;
  folder[folder_len++] = 0x02U;
  make_7z_with_folder_bytes(archive, &archive_len, folder, folder_len);
  if (zz9k_archive_7z_list(archive, archive_len, 0, 0U, &count)) {
    return 1;
  }
  diagnostic = zz9k_archive_7z_parse_diagnostic();
  if (!diagnostic) return 2;
  if (strcmp(diagnostic, "7z folder method unsupported") != 0) {
    return 3;
  }
  return 0;
}

static int test_7z_copy_multi_substream_list(void)
{
  const uint8_t hello[5] = { 'h', 'e', 'l', 'l', 'o' };
  const uint8_t bye[3] = { 'b', 'y', 'e' };
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[3];
  uint32_t archive_len;
  uint32_t count;

  make_7z_copy_two_file_substream(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 3U, &count)) {
    return 1;
  }
  if (count != 2U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 4;
  if (entries[0].data_offset != 32U) return 5;
  if (entries[0].compressed_size != sizeof(hello)) return 6;
  if (entries[0].uncompressed_size != sizeof(hello)) return 7;
  if (entries[0].crc32 !=
      zz9k_archive_crc32(0U, hello, sizeof(hello))) return 8;
  if ((entries[0].flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) == 0U) return 9;
  if (strcmp(entries[1].name, "bye.txt") != 0) return 10;
  if (entries[1].method != ZZ9K_ARCHIVE_7Z_METHOD_COPY) return 11;
  if (entries[1].data_offset != 37U) return 12;
  if (entries[1].compressed_size != sizeof(bye)) return 13;
  if (entries[1].uncompressed_size != sizeof(bye)) return 14;
  if (entries[1].crc32 !=
      zz9k_archive_crc32(0U, bye, sizeof(bye))) return 15;
  if ((entries[1].flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) == 0U) return 16;
  return 0;
}

static int test_7z_deflate_multi_substream_is_file_backed_split(void)
{
  const uint8_t hello[5] = { 'h', 'e', 'l', 'l', 'o' };
  const uint8_t bye[3] = { 'b', 'y', 'e' };
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[3];
  uint32_t archive_len;
  uint32_t count;
  int needs_deflate;
  int needs_lzma;
  int needs_lzma2;

  make_7z_deflate_two_file_substream(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 3U, &count)) {
    return 1;
  }
  if (count != 2U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) return 4;
  if (entries[0].data_offset != 32U) return 5;
  if (entries[0].decoded_offset != 0U) return 20;
  if (entries[0].compressed_size != 10U) return 6;
  if (entries[0].uncompressed_size != sizeof(hello)) return 7;
  if (entries[0].crc32 !=
      zz9k_archive_crc32(0U, hello, sizeof(hello))) return 8;
  if ((entries[0].flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) == 0U) return 9;
  if ((entries[0].flags &
       ZZ9K_ARCHIVE_TEST_7Z_SPLIT_SUBSTREAM_FLAG) == 0U) {
    return 10;
  }
  if (strcmp(entries[1].name, "bye.txt") != 0) return 11;
  if (entries[1].method != ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) return 12;
  if (entries[1].data_offset != 32U) return 13;
  if (entries[1].decoded_offset != sizeof(hello)) return 21;
  if (entries[1].compressed_size != 10U) return 14;
  if (entries[1].uncompressed_size != sizeof(bye)) return 15;
  if (entries[1].crc32 !=
      zz9k_archive_crc32(0U, bye, sizeof(bye))) return 16;
  if ((entries[1].flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) == 0U) return 17;
  if ((entries[1].flags &
       ZZ9K_ARCHIVE_TEST_7Z_SPLIT_SUBSTREAM_FLAG) == 0U) {
    return 18;
  }
  if (!zz9k_archive_7z_file_can_handle_entries(
          entries, count, archive_len, &needs_deflate,
          &needs_lzma, &needs_lzma2)) {
    return 19;
  }
  if (!needs_deflate || needs_lzma || needs_lzma2) return 22;
  return 0;
}

static int assert_7z_real_split_fixture(const uint8_t *archive,
                                        uint32_t archive_len,
                                        uint32_t method,
                                        int expect_deflate,
                                        int expect_lzma,
                                        int expect_lzma2)
{
  const char *output_dir = "archive_tool_real_7z_split_out";
  const char *bye_path = "archive_tool_real_7z_split_out/bye.txt";
  const char *hello_path = "archive_tool_real_7z_split_out/hello.txt";
  const uint8_t decoded[8] = {
    'b', 'y', 'e', 'h', 'e', 'l', 'l', 'o'
  };
  ZZ9KArchiveEntry entries[4];
  ZZ9KArchive7zSplitWriter writer;
  uint8_t actual[5];
  FILE *file = 0;
  uint32_t count;
  int needs_deflate;
  int needs_lzma;
  int needs_lzma2;
  int rc = 0;

  remove(bye_path);
  remove(hello_path);
  remove(output_dir);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 4U, &count)) {
    return 1;
  }
  if (count != 2U) return 2;
  if (strcmp(entries[0].name, "bye.txt") != 0) return 3;
  if (strcmp(entries[1].name, "hello.txt") != 0) return 4;
  if (entries[0].method != method || entries[1].method != method) return 5;
  if (entries[0].data_offset != 32U || entries[1].data_offset != 32U) {
    return 6;
  }
  if (entries[0].compressed_size == 0U ||
      entries[0].compressed_size != entries[1].compressed_size) {
    return 7;
  }
  if (entries[0].uncompressed_size != 3U ||
      entries[1].uncompressed_size != 5U) {
    return 8;
  }
  if (entries[0].decoded_offset != 0U ||
      entries[1].decoded_offset != 3U) {
    return 9;
  }
  if (entries[0].crc32 != 0x77379134UL ||
      entries[1].crc32 != 0x3610a686UL) {
    return 10;
  }
  if ((entries[0].flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) == 0U ||
      (entries[1].flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) == 0U) {
    return 11;
  }
  if ((entries[0].flags & ZZ9K_ARCHIVE_TEST_7Z_SPLIT_SUBSTREAM_FLAG) == 0U ||
      (entries[1].flags & ZZ9K_ARCHIVE_TEST_7Z_SPLIT_SUBSTREAM_FLAG) == 0U) {
    return 12;
  }
  if (zz9k_archive_7z_split_group_count(entries, count, 0U) != 2U) {
    return 13;
  }
  if (!zz9k_archive_7z_file_can_handle_entries(
          entries, count, archive_len, &needs_deflate,
          &needs_lzma, &needs_lzma2)) {
    return 14;
  }
  if (needs_deflate != expect_deflate ||
      needs_lzma != expect_lzma ||
      needs_lzma2 != expect_lzma2) {
    return 15;
  }

  zz9k_archive_7z_split_writer_init(&writer, output_dir, entries, count, 1);
  if (!zz9k_archive_7z_split_writer_chunk(&writer, decoded, 4U) ||
      !zz9k_archive_7z_split_writer_chunk(&writer, decoded + 4U, 4U) ||
      !zz9k_archive_7z_split_writer_finish(&writer)) {
    rc = 16;
    goto out;
  }

  file = fopen(bye_path, "rb");
  if (!file) {
    rc = 17;
    goto out;
  }
  if (fread(actual, 1U, 3U, file) != 3U ||
      memcmp(actual, "bye", 3U) != 0) {
    rc = 18;
    goto out;
  }
  fclose(file);
  file = 0;

  file = fopen(hello_path, "rb");
  if (!file) {
    rc = 19;
    goto out;
  }
  if (fread(actual, 1U, 5U, file) != 5U ||
      memcmp(actual, "hello", 5U) != 0) {
    rc = 20;
    goto out;
  }

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_7z_split_writer_cleanup(&writer);
  remove(bye_path);
  remove(hello_path);
  remove(output_dir);
  return rc;
}

static int test_7z_real_split_fixtures_parse_and_split(void)
{
  int rc;

  rc = assert_7z_real_split_fixture(
      real_7z_split_deflate_fixture,
      (uint32_t)sizeof(real_7z_split_deflate_fixture),
      ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE, 1, 0, 0);
  if (rc) return 100 + rc;

  rc = assert_7z_real_split_fixture(
      real_7z_split_lzma_fixture,
      (uint32_t)sizeof(real_7z_split_lzma_fixture),
      ZZ9K_ARCHIVE_7Z_METHOD_LZMA, 0, 1, 0);
  if (rc) return 200 + rc;

  rc = assert_7z_real_split_fixture(
      real_7z_split_lzma2_fixture,
      (uint32_t)sizeof(real_7z_split_lzma2_fixture),
      ZZ9K_ARCHIVE_7Z_METHOD_LZMA2, 0, 0, 1);
  if (rc) return 300 + rc;

  return 0;
}

static int test_7z_split_writer_extracts_decoded_substreams(void)
{
  const char *output_dir = "archive_tool_7z_split_out";
  const char *hello_path = "archive_tool_7z_split_out/hello.txt";
  const char *bye_path = "archive_tool_7z_split_out/bye.txt";
  const uint8_t decoded[8] = {
    'h', 'e', 'l', 'l', 'o', 'b', 'y', 'e'
  };
  uint8_t archive[256];
  uint8_t actual[5];
  ZZ9KArchiveEntry entries[3];
  ZZ9KArchive7zSplitWriter writer;
  FILE *file = 0;
  uint32_t archive_len;
  uint32_t count;
  int rc = 0;

  remove(hello_path);
  remove(bye_path);
  remove(output_dir);
  make_7z_deflate_two_file_substream(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 3U, &count)) {
    return 1;
  }
  if (count != 2U) return 2;
  if (zz9k_archive_7z_split_group_count(entries, count, 0U) != 2U) {
    return 3;
  }

  zz9k_archive_7z_split_writer_init(&writer, output_dir, entries, count, 1);
  if (!zz9k_archive_7z_split_writer_chunk(&writer, decoded, 2U) ||
      !zz9k_archive_7z_split_writer_chunk(&writer, decoded + 2U, 4U) ||
      !zz9k_archive_7z_split_writer_chunk(&writer, decoded + 6U, 2U) ||
      !zz9k_archive_7z_split_writer_finish(&writer)) {
    rc = 4;
    goto out;
  }

  file = fopen(hello_path, "rb");
  if (!file) {
    rc = 5;
    goto out;
  }
  if (fread(actual, 1U, 5U, file) != 5U ||
      memcmp(actual, "hello", 5U) != 0) {
    rc = 6;
    goto out;
  }
  fclose(file);
  file = fopen(bye_path, "rb");
  if (!file) {
    rc = 7;
    goto out;
  }
  if (fread(actual, 1U, 3U, file) != 3U ||
      memcmp(actual, "bye", 3U) != 0) {
    rc = 8;
    goto out;
  }

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_7z_split_writer_cleanup(&writer);
  remove(hello_path);
  remove(bye_path);
  remove(output_dir);
  return rc;
}

static int test_7z_split_writer_rejects_substream_crc_mismatch(void)
{
  const uint8_t decoded[8] = {
    'h', 'e', 'x', 'l', 'o', 'b', 'y', 'e'
  };
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[3];
  ZZ9KArchive7zSplitWriter writer;
  uint32_t archive_len;
  uint32_t count;
  int ok;

  make_7z_deflate_two_file_substream(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 3U, &count)) {
    return 1;
  }
  zz9k_archive_7z_split_writer_init(&writer, "", entries, count, 0);
  ok = zz9k_archive_7z_split_writer_chunk(
      &writer, decoded, sizeof(decoded));
  zz9k_archive_7z_split_writer_cleanup(&writer);
  return ok ? 2 : 0;
}

static int test_7z_split_writer_filter_ignores_unmatched_crc(void)
{
  const char *output_dir = "archive_tool_7z_split_match_out";
  const char *hello_path = "archive_tool_7z_split_match_out/hello.txt";
  const char *bye_path = "archive_tool_7z_split_match_out/bye.txt";
  const uint8_t decoded[8] = {
    'h', 'e', 'x', 'l', 'o', 'b', 'y', 'e'
  };
  uint8_t archive[256];
  uint8_t actual[3];
  ZZ9KArchiveEntry entries[3];
  ZZ9KArchive7zSplitWriter writer;
  FILE *file = 0;
  uint32_t archive_len;
  uint32_t count;
  int rc = 0;

  remove(hello_path);
  remove(bye_path);
  remove(output_dir);
  make_7z_deflate_two_file_substream(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 3U, &count)) {
    return 1;
  }
  zz9k_archive_match_filter = "bye";
  zz9k_archive_7z_split_writer_init(&writer, output_dir, entries, count, 1);
  if (!zz9k_archive_7z_split_writer_chunk(
          &writer, decoded, sizeof(decoded)) ||
      !zz9k_archive_7z_split_writer_finish(&writer)) {
    rc = 2;
    goto out;
  }

  file = fopen(hello_path, "rb");
  if (file) {
    rc = 3;
    goto out;
  }
  file = fopen(bye_path, "rb");
  if (!file) {
    rc = 4;
    goto out;
  }
  if (fread(actual, 1U, 3U, file) != 3U ||
      memcmp(actual, "bye", 3U) != 0) {
    rc = 5;
    goto out;
  }

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_7z_split_writer_cleanup(&writer);
  zz9k_archive_match_filter = 0;
  remove(hello_path);
  remove(bye_path);
  remove(output_dir);
  return rc;
}

static int test_7z_split_group_rejects_discontinuous_offsets(void)
{
  ZZ9KArchiveEntry entries[2];
  uint32_t unpacked = 0U;

  memset(entries, 0, sizeof(entries));
  strcpy(entries[0].name, "hello.txt");
  entries[0].uncompressed_size = 2U;
  entries[0].decoded_offset = 0U;
  strcpy(entries[1].name, "bye.txt");
  entries[1].uncompressed_size = 3U;
  entries[1].decoded_offset = 5U; /* gap: previous substream ends at 2 */
  if (zz9k_archive_7z_split_group_unpacked_size(entries, 2U, &unpacked)) {
    return 1;
  }
  return 0;
}

static int test_7z_split_writer_handles_zero_length_substreams(void)
{
  const char *output_dir = "archive_tool_7z_split_zero_out";
  const char *a_path = "archive_tool_7z_split_zero_out/a.txt";
  const char *empty_path = "archive_tool_7z_split_zero_out/empty.txt";
  const char *b_path = "archive_tool_7z_split_zero_out/b.txt";
  const uint8_t decoded[8] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
  };
  uint8_t actual[8];
  ZZ9KArchiveEntry entries[3];
  ZZ9KArchive7zSplitWriter writer;
  FILE *file = 0;
  int rc = 0;

  remove(a_path);
  remove(empty_path);
  remove(b_path);
  remove(output_dir);
  memset(entries, 0, sizeof(entries));
  strcpy(entries[0].name, "a.txt");
  entries[0].uncompressed_size = 3U;
  entries[0].decoded_offset = 0U;
  strcpy(entries[1].name, "empty.txt");
  entries[1].uncompressed_size = 0U;
  entries[1].decoded_offset = 3U;
  strcpy(entries[2].name, "b.txt");
  entries[2].uncompressed_size = 5U;
  entries[2].decoded_offset = 3U;

  zz9k_archive_7z_split_writer_init(&writer, output_dir, entries, 3U, 1);
  if (!zz9k_archive_7z_split_writer_chunk(&writer, decoded, 2U) ||
      !zz9k_archive_7z_split_writer_chunk(&writer, decoded + 2U, 3U) ||
      !zz9k_archive_7z_split_writer_chunk(&writer, decoded + 5U, 3U) ||
      !zz9k_archive_7z_split_writer_finish(&writer)) {
    rc = 1;
    goto out;
  }

  file = fopen(a_path, "rb");
  if (!file || fread(actual, 1U, 3U, file) != 3U ||
      memcmp(actual, "abc", 3U) != 0) {
    rc = 2;
    goto out;
  }
  fclose(file);
  file = fopen(empty_path, "rb");
  if (!file) {
    rc = 3;
    goto out;
  }
  if (fseek(file, 0L, SEEK_END) != 0 || ftell(file) != 0L) {
    rc = 4;
    goto out;
  }
  fclose(file);
  file = fopen(b_path, "rb");
  if (!file || fread(actual, 1U, 5U, file) != 5U ||
      memcmp(actual, "defgh", 5U) != 0) {
    rc = 5;
    goto out;
  }

out:
  if (file) {
    fclose(file);
    file = 0;
  }
  zz9k_archive_7z_split_writer_cleanup(&writer);
  remove(a_path);
  remove(empty_path);
  remove(b_path);
  remove(output_dir);
  return rc;
}

static int test_7z_rejects_bad_start_header_crc(void)
{
  uint8_t archive[192];
  ZZ9KArchive7zHeader start;
  uint32_t archive_len;

  make_7z_copy_file(archive, &archive_len);
  memset(&start, 0, sizeof(start));
  if (!zz9k_archive_7z_start_header(archive, archive_len, &start)) {
    return 1;
  }
  archive[8U] ^= 0x01U;
  if (zz9k_archive_7z_start_header(archive, archive_len, &start)) {
    return 2;
  }

  make_7z_copy_file(archive, &archive_len);
  archive[28U] ^= 0x01U;
  if (zz9k_archive_7z_start_header(archive, archive_len, &start)) {
    return 3;
  }
  return 0;
}

static int test_7z_copy_test_rejects_bad_data_crc(void)
{
  uint8_t archive[192];
  uint32_t archive_len;

  make_7z_copy_file(archive, &archive_len);
  archive[32U] ^= 0x01U;
  if (zz9k_archive_handle_7z(
          0, 0, 0, archive, archive_len, "t", 0)) {
    return 1;
  }
  return 0;
}

static int test_7z_copy_file_test_rejects_bad_data_crc(void)
{
  const char *path = "archive_tool_7z_bad_copy_crc.tmp";
  uint8_t archive[192];
  ZZ9KContext *ctx = 0;
  ZZ9KServiceInfo service;
  uint32_t archive_len;
  int codec_ready = 0;
  int attempted = 0;
  int rc = 0;

  make_7z_copy_file(archive, &archive_len);
  archive[32U] ^= 0x01U;
  remove(path);
  if (!write_test_file(path, archive, archive_len)) return 1;
  memset(&service, 0, sizeof(service));
  if (zz9k_archive_handle_7z_file(
          &ctx, &service, &codec_ready, "t", path, archive_len,
          0, &attempted)) {
    rc = 2;
  } else if (!attempted) {
    rc = 3;
  }
  remove(path);
  return rc;
}

static int test_7z_lzma_list_and_wrap(void)
{
  uint8_t archive[224];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchive7zHeader start;
  uint8_t header[13];
  uint8_t *wrapped;
  uint32_t wrapped_length;
  uint32_t archive_len;
  uint32_t count;

  make_7z_lzma_file(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "packed.bin") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA) return 4;
  if (entries[0].method_props_size != 5U) return 5;
  if (memcmp(entries[0].method_props,
             "\x5d\x00\x00\x80\x00", 5U) != 0) return 6;
  if (entries[0].data_offset != 32U) return 7;
  if (entries[0].compressed_size != 6U) return 8;
  if (entries[0].uncompressed_size != 11U) return 9;

  memset(&start, 0, sizeof(start));
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_start_header_from_prefix(
          archive, 32U, archive_len, &start)) {
    return 17;
  }
  if (start.next_header_offset != 38U) return 18;
  if (!zz9k_archive_7z_list_from_header(
          archive + start.next_header_offset, start.next_header_size,
          archive_len, entries, 2U, &count)) {
    return 19;
  }
  if (count != 1U) return 20;
  if (strcmp(entries[0].name, "packed.bin") != 0) return 21;
  if (entries[0].data_offset != 32U) return 22;
  if (entries[0].compressed_size != 6U) return 23;
  if (entries[0].uncompressed_size != 11U) return 24;

  memset(header, 0xa5, sizeof(header));
  if (!zz9k_archive_7z_lzma_alone_header(&entries[0], header)) return 10;
  if (memcmp(header, entries[0].method_props, 5U) != 0) return 11;
  if (zz9k_archive_get_le64(header + 5U) != 11U) return 12;

  wrapped = 0;
  wrapped_length = 0U;
  if (!zz9k_archive_7z_build_lzma_alone_payload(
          &entries[0], archive + entries[0].data_offset,
          entries[0].compressed_size, &wrapped, &wrapped_length)) {
    return 13;
  }
  if (wrapped_length != 19U) return 14;
  if (memcmp(wrapped, header, sizeof(header)) != 0) return 15;
  if (memcmp(wrapped + 13U, archive + entries[0].data_offset, 6U) != 0) {
    return 16;
  }
  free(wrapped);
  return 0;
}

static int test_7z_deflate_list_and_can_handle(void)
{
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;
  int needs_deflate;
  int needs_lzma;
  int needs_lzma2;

  make_7z_deflate_file(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "deflate.bin") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) return 4;
  if (entries[0].method_props_size != 0U) return 5;
  if (entries[0].data_offset != 32U) return 6;
  if (entries[0].compressed_size != 5U) return 7;
  if (entries[0].uncompressed_size != 3U) return 8;
  if (!zz9k_archive_entry_has_crc32(&entries[0])) return 9;
  if (!zz9k_archive_7z_file_can_handle_entries(
          entries, count, archive_len, &needs_deflate,
          &needs_lzma, &needs_lzma2)) {
    return 10;
  }
  if (!needs_deflate || needs_lzma || needs_lzma2) return 11;
  return 0;
}

static int test_7z_lzma2_list_and_wrap(void)
{
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[2];
  uint8_t *wrapped;
  uint32_t wrapped_length;
  uint32_t archive_len;
  uint32_t count;

  make_7z_lzma2_file(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "lzma2.bin") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) return 4;
  if (entries[0].method_props_size != 1U) return 5;
  if (entries[0].method_props[0] != 0x10U) return 6;
  if (entries[0].data_offset != 32U) return 7;
  if (entries[0].compressed_size != 84U) return 8;
  if (entries[0].uncompressed_size != 80U) return 9;

  wrapped = 0;
  wrapped_length = 0U;
  if (!zz9k_archive_7z_build_lzma2_payload(
          &entries[0], archive + entries[0].data_offset,
          entries[0].compressed_size, &wrapped, &wrapped_length)) {
    return 10;
  }
  if (wrapped_length != 85U) return 11;
  if (wrapped[0] != 0x10U) return 12;
  if (memcmp(wrapped + 1U, archive + entries[0].data_offset, 84U) != 0) {
    return 13;
  }
  free(wrapped);
  return 0;
}

static int test_7z_lzma2_feed_output_limit_has_end_marker_headroom(void)
{
  uint8_t archive[256];
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t count;
  uint32_t limit;

  make_7z_lzma2_file(archive, &archive_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list(archive, archive_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (!zz9k_archive_7z_lzma2_feed_output_limit(&entries[0], &limit)) {
    return 3;
  }
  if (limit != entries[0].uncompressed_size + 1U) return 4;
  entries[0].method = ZZ9K_ARCHIVE_7Z_METHOD_LZMA;
  if (zz9k_archive_7z_lzma2_feed_output_limit(&entries[0], &limit)) {
    return 5;
  }
  return 0;
}

static int write_test_file(const char *path,
                           const uint8_t *data,
                           uint32_t length)
{
  FILE *file = fopen(path, "wb");
  int ok = 0;

  if (!file) {
    return 0;
  }
  ok = fwrite(data, 1U, length, file) == length;
  if (fclose(file) != 0) {
    ok = 0;
  }
  return ok;
}

static int test_extract_refuses_existing_file(void)
{
  const char *path = "archive_tool_existing_output.tmp";
  const uint8_t old_data[3] = { 'o', 'l', 'd' };
  const uint8_t new_data[3] = { 'n', 'e', 'w' };
  uint8_t actual[3];
  ZZ9KArchiveEntry entry;
  FILE *file;
  int rc = 0;

  remove(path);
  if (!write_test_file(path, old_data, sizeof(old_data))) return 1;
  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, path);
  entry.uncompressed_size = sizeof(new_data);
  if (zz9k_archive_write_entry("", &entry, new_data)) {
    rc = 2;
    goto out;
  }
  file = fopen(path, "rb");
  if (!file) {
    rc = 3;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 4;
  } else if (memcmp(actual, old_data, sizeof(old_data)) != 0) {
    rc = 5;
  }
  fclose(file);
out:
  remove(path);
  return rc;
}

static int test_extract_skip_existing_keeps_existing_file(void)
{
  const char *path = "archive_tool_skip_existing.tmp";
  const uint8_t old_data[3] = { 'o', 'l', 'd' };
  const uint8_t new_data[3] = { 'n', 'e', 'w' };
  uint8_t actual[3];
  ZZ9KArchiveEntry entry;
  FILE *file;
  int rc = 0;

  remove(path);
  if (!write_test_file(path, old_data, sizeof(old_data))) return 1;
  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, path);
  entry.uncompressed_size = sizeof(new_data);
  zz9k_archive_skip_existing_outputs = 1;
  if (!zz9k_archive_write_entry("", &entry, new_data)) {
    rc = 2;
    goto out;
  }
  file = fopen(path, "rb");
  if (!file) {
    rc = 3;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 4;
  } else if (memcmp(actual, old_data, sizeof(old_data)) != 0) {
    rc = 5;
  }
  fclose(file);
out:
  zz9k_archive_skip_existing_outputs = 0;
  remove(path);
  return rc;
}

static int test_extract_strip_components_rewrites_output_path(void)
{
  const char *output_dir = "archive_tool_strip_out";
  const char *output_path = "archive_tool_strip_out/dir/file.txt";
  const char *unstripped_path = "archive_tool_strip_out/top/dir/file.txt";
  const uint8_t data[5] = { 'h', 'e', 'l', 'l', 'o' };
  uint8_t actual[5];
  ZZ9KArchiveEntry entry;
  FILE *file = 0;
  int rc = 0;

  remove(output_path);
  remove(unstripped_path);
  remove("archive_tool_strip_out/top/dir");
  remove("archive_tool_strip_out/top");
  remove("archive_tool_strip_out/dir");
  remove(output_dir);
  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, "top/dir/file.txt");
  entry.uncompressed_size = sizeof(data);
  zz9k_archive_strip_components = 1U;
  if (!zz9k_archive_write_entry(output_dir, &entry, data)) {
    rc = 1;
    goto out;
  }
  file = fopen(output_path, "rb");
  if (!file) {
    rc = 2;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 3;
    goto out;
  }
  if (memcmp(actual, data, sizeof(data)) != 0) rc = 4;
  if (fopen(unstripped_path, "rb") != 0) rc = 5;
out:
  zz9k_archive_strip_components = 0U;
  if (file) fclose(file);
  remove(output_path);
  remove(unstripped_path);
  remove("archive_tool_strip_out/top/dir");
  remove("archive_tool_strip_out/top");
  remove("archive_tool_strip_out/dir");
  remove(output_dir);
  return rc;
}

static int test_extract_dry_run_does_not_write_file(void)
{
  const char *output_dir = "archive_tool_dry_run_out";
  const char *output_path = "archive_tool_dry_run_out/dir/file.txt";
  const uint8_t data[5] = { 'h', 'e', 'l', 'l', 'o' };
  ZZ9KArchiveEntry entry;
  FILE *file;
  int rc = 0;

  remove(output_path);
  remove("archive_tool_dry_run_out/dir");
  remove(output_dir);
  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, "dir/file.txt");
  entry.uncompressed_size = sizeof(data);
  zz9k_archive_dry_run_outputs = 1;
  if (!zz9k_archive_write_entry(output_dir, &entry, data)) {
    rc = 1;
    goto out;
  }
  file = fopen(output_path, "rb");
  if (file) {
    fclose(file);
    rc = 2;
  }
out:
  zz9k_archive_dry_run_outputs = 0;
  remove(output_path);
  remove("archive_tool_dry_run_out/dir");
  remove(output_dir);
  return rc;
}

static int test_extract_dry_run_rejects_file_where_directory_is_needed(void)
{
  const char *path = "archive_tool_dry_dir_conflict";
  const uint8_t existing[4] = { 'f', 'i', 'l', 'e' };
  ZZ9KArchiveEntry entry;
  int rc = 0;

  remove(path);
  if (!write_test_file(path, existing, sizeof(existing))) return 1;
  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, "archive_tool_dry_dir_conflict/");
  zz9k_archive_dry_run_outputs = 1;
  if (zz9k_archive_write_entry("", &entry, 0)) {
    rc = 2;
  }
  zz9k_archive_dry_run_outputs = 0;
  remove(path);
  return rc;
}

static int test_extract_refuses_file_where_directory_is_needed(void)
{
  const char *prefix = "archive_tool_parent_conflict";
  const char *output_path = "archive_tool_parent_conflict/file.txt";
  const uint8_t existing[4] = { 'f', 'i', 'l', 'e' };
  const uint8_t data[4] = { 'd', 'a', 't', 'a' };
  ZZ9KArchiveEntry entry;
  int rc = 0;

  remove(output_path);
  remove(prefix);
  if (!write_test_file(prefix, existing, sizeof(existing))) return 1;
  if (zz9k_archive_mkdir_one(prefix)) {
    rc = 2;
    goto out;
  }
  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, output_path);
  entry.uncompressed_size = sizeof(data);
  if (zz9k_archive_write_entry("", &entry, data)) {
    rc = 3;
  }
out:
  remove(output_path);
  remove(prefix);
  return rc;
}

static int test_extract_refuses_directory_where_file_is_needed(void)
{
  const char *path = "archive_tool_file_dir_conflict";
  const uint8_t data[4] = { 'd', 'a', 't', 'a' };
  ZZ9KArchiveEntry entry;
  int rc = 0;

  remove(path);
  if (!zz9k_archive_mkdir_one(path)) return 1;
  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, path);
  entry.uncompressed_size = sizeof(data);
  zz9k_archive_overwrite_outputs = 1;
  if (zz9k_archive_write_entry("", &entry, data)) {
    rc = 2;
  }
  zz9k_archive_overwrite_outputs = 0;
  remove(path);
  return rc;
}

static int test_7z_read_header_from_file(void)
{
  const char *path = "archive_tool_7z_header.tmp";
  uint8_t archive[224];
  uint8_t *header = 0;
  ZZ9KArchiveEntry entries[2];
  uint32_t archive_len;
  uint32_t header_len;
  uint32_t count;
  int rc = 0;

  make_7z_lzma_file(archive, &archive_len);
  remove(path);
  if (!write_test_file(path, archive, archive_len)) return 1;
  if (!zz9k_archive_7z_read_header_from_file(
          path, archive_len, &header, &header_len)) {
    rc = 2;
    goto out;
  }
  if (header_len == 0U || header[0] != ZZ9K_ARCHIVE_7Z_ID_HEADER) {
    rc = 3;
    goto out;
  }
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_7z_list_from_header(
          header, header_len, archive_len, entries, 2U, &count)) {
    rc = 4;
    goto out;
  }
  if (count != 1U) rc = 5;
  else if (strcmp(entries[0].name, "packed.bin") != 0) rc = 6;
  else if (entries[0].data_offset != 32U) rc = 7;
  else if (entries[0].compressed_size != 6U) rc = 8;

out:
  free(header);
  remove(path);
  return rc;
}

static int test_feed_file_input_chunk_uses_prefix_and_range(void)
{
  const char *path = "archive_tool_feed_range.tmp";
  const uint8_t source[8] = {
    0x10U, 0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U
  };
  const uint8_t prefix[3] = { 0xa0U, 0xa1U, 0xa2U };
  uint8_t shared[8];
  ZZ9KSharedBuffer input;
  FILE *file = 0;
  uint32_t copied = 0U;
  int rc = 0;

  remove(path);
  if (!write_test_file(path, source, sizeof(source))) return 1;
  file = fopen(path, "rb");
  if (!file) {
    rc = 2;
    goto out;
  }
  if (fseek(file, 2L, SEEK_SET) != 0) {
    rc = 3;
    goto out;
  }
  memset(shared, 0, sizeof(shared));
  memset(&input, 0, sizeof(input));
  input.handle = 1U;
  input.data = shared;
  input.length = sizeof(shared);

  if (!zz9k_archive_copy_feed_file_input_chunk(
          &input, file, prefix, sizeof(prefix), 4U, 0U, 5U, &copied)) {
    rc = 4;
    goto out;
  }
  if (copied != 5U) rc = 5;
  else if (memcmp(shared, "\xa0\xa1\xa2\x12\x13", 5U) != 0) rc = 6;

  memset(shared, 0, sizeof(shared));
  if (!zz9k_archive_copy_feed_file_input_chunk(
          &input, file, prefix, sizeof(prefix), 4U, 5U, 3U, &copied)) {
    rc = 7;
    goto out;
  }
  if (copied != 2U) rc = 8;
  else if (memcmp(shared, "\x14\x15", 2U) != 0) rc = 9;

out:
  if (file) {
    fclose(file);
  }
  remove(path);
  return rc;
}

static int test_write_file_range_entry(void)
{
  const char *input_path = "archive_tool_range_in.tmp";
  const char *output_name = "archive_tool_range_out.tmp";
  const uint8_t source[8] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
  };
  uint8_t actual[4];
  ZZ9KArchiveEntry entry;
  FILE *file = 0;
  int rc = 0;

  remove(input_path);
  remove(output_name);
  if (!write_test_file(input_path, source, sizeof(source))) return 1;

  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, output_name);
  entry.data_offset = 2U;
  entry.compressed_size = 4U;
  entry.uncompressed_size = 4U;
  if (!zz9k_archive_write_file_range_entry(".", &entry, input_path)) {
    rc = 2;
    goto out;
  }

  file = fopen(output_name, "rb");
  if (!file) {
    rc = 3;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 4;
    goto out;
  }
  if (memcmp(actual, "cdef", sizeof(actual)) != 0) rc = 5;

out:
  if (file) {
    fclose(file);
  }
  remove(input_path);
  remove(output_name);
  return rc;
}

static int test_read_file_range(void)
{
  const char *path = "archive_tool_range_read.tmp";
  const uint8_t source[8] = {
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'
  };
  uint8_t *actual = 0;
  int rc = 0;

  remove(path);
  if (!write_test_file(path, source, sizeof(source))) return 1;
  if (!zz9k_archive_read_file_range(path, 3U, 3U, &actual)) {
    rc = 2;
    goto out;
  }
  if (memcmp(actual, "def", 3U) != 0) rc = 3;

out:
  free(actual);
  remove(path);
  return rc;
}

static int test_lzma_props_dict_size(void)
{
  const uint8_t props_8m[5] = { 0x5dU, 0x00U, 0x00U, 0x80U, 0x00U };
  const uint8_t props_64k[5] = { 0x5dU, 0x00U, 0x00U, 0x01U, 0x00U };
  uint32_t dict_size = 0U;

  if (!zz9k_archive_lzma_props_dict_size(
          props_8m, sizeof(props_8m), &dict_size)) {
    return 1;
  }
  if (dict_size != 0x00800000UL) return 2;
  if (!zz9k_archive_lzma_props_dict_size(
          props_64k, sizeof(props_64k), &dict_size)) {
    return 3;
  }
  if (dict_size != 65536UL) return 4;
  if (zz9k_archive_lzma_props_dict_size(props_8m, 4U, &dict_size)) {
    return 5;
  }
  if (zz9k_archive_lzma_props_dict_size(0, sizeof(props_8m), &dict_size)) {
    return 6;
  }
  if (zz9k_archive_lzma_props_dict_size(
          props_8m, sizeof(props_8m), 0)) {
    return 7;
  }
  return 0;
}

static int test_lzma2_prop_dict_size(void)
{
  uint32_t dict_size;

  dict_size = 0U;
  if (!zz9k_archive_lzma2_prop_dict_size(0x00U, &dict_size)) return 1;
  if (dict_size != 4096U) return 2;
  if (!zz9k_archive_lzma2_prop_dict_size(0x10U, &dict_size)) return 3;
  if (dict_size != 1048576U) return 4;
  if (!zz9k_archive_lzma2_prop_dict_size(0x18U, &dict_size)) return 5;
  if (dict_size != 16777216U) return 6;
  if (!zz9k_archive_lzma2_prop_dict_size(0x28U, &dict_size)) return 7;
  if (dict_size != 0xffffffffUL) return 8;
  if (zz9k_archive_lzma2_prop_dict_size(0x29U, &dict_size)) return 9;
  if (zz9k_archive_lzma2_prop_dict_size(0x10U, 0)) return 10;
  return 0;
}

static int test_safe_paths(void)
{
  if (!zz9k_archive_path_is_safe("dir/file.txt")) return 1;
  if (!zz9k_archive_path_is_safe("plain")) return 2;
  if (zz9k_archive_path_is_safe("../escape")) return 3;
  if (zz9k_archive_path_is_safe("dir/../../escape")) return 4;
  if (zz9k_archive_path_is_safe("/absolute")) return 5;
  if (zz9k_archive_path_is_safe("SYS:Prefs")) return 6;
  if (zz9k_archive_path_is_safe("dir\\file")) return 7;
  return 0;
}

static int test_gzip_info(void)
{
  const char *path = "archive_tool_gzip_info.tmp";
  static const uint8_t gzip_data[] = {
    0x1f, 0x8b, 0x08, 0x08, 0, 0, 0, 0, 0, 0xff,
    'h', 'e', 'l', 'l', 'o', '.', 't', 'x', 't', 0,
    0x86, 0xa6, 0x10, 0x36, 5, 0, 0, 0
  };
  ZZ9KArchiveGzipInfo info;
  ZZ9KDecompressResult result;
  uint8_t gzip_fhcrc[30];
  uint16_t fhcrc;
  int rc = 0;

  if (!zz9k_archive_gzip_info(gzip_data, sizeof(gzip_data), &info)) {
    return 1;
  }
  if (strcmp(info.name, "hello.txt") != 0) return 2;
  if (info.uncompressed_size != 5U) return 3;
  if (info.crc32 != 0x3610a686UL) return 10;
  if (zz9k_archive_crc32(0U, (const uint8_t *)"hello", 5U) !=
      0x3610a686UL) {
    return 11;
  }
  memset(&result, 0, sizeof(result));
  result.bytes_written = 5U;
  result.checksum = 0x3610a686UL;
  if (!zz9k_archive_gzip_result_matches_footer(&info, &result)) return 12;
  result.checksum = 0U;
  if (zz9k_archive_gzip_result_matches_footer(&info, &result)) return 13;

  memcpy(gzip_fhcrc, gzip_data, sizeof(gzip_data));
  gzip_fhcrc[3] = 0x0aU;
  fhcrc = (uint16_t)(zz9k_archive_crc32(0U, gzip_fhcrc, 20U) & 0xffffU);
  put_le16(gzip_fhcrc + 20U, fhcrc);
  memcpy(gzip_fhcrc + 22U, gzip_data + 20U, 8U);
  if (!zz9k_archive_gzip_info(gzip_fhcrc, sizeof(gzip_fhcrc), &info)) {
    return 14;
  }
  gzip_fhcrc[21U] ^= 0x01U;
  if (zz9k_archive_gzip_info(gzip_fhcrc, sizeof(gzip_fhcrc), &info)) {
    return 15;
  }

  remove(path);
  if (!write_test_file(path, gzip_data, sizeof(gzip_data))) return 4;
  memset(&info, 0, sizeof(info));
  if (!zz9k_archive_gzip_info_from_file(
          path, gzip_data, 20U, sizeof(gzip_data), &info)) {
    rc = 5;
    goto out;
  }
  if (strcmp(info.name, "hello.txt") != 0) rc = 6;
  else if (info.compressed_size != sizeof(gzip_data)) rc = 7;
  else if (info.uncompressed_size != 5U) rc = 8;
  else if (info.crc32 != 0x3610a686UL) rc = 10;
  else if (zz9k_archive_gzip_info_from_file(
               path, gzip_data, 12U, sizeof(gzip_data), &info)) {
    rc = 9;
  }

out:
  remove(path);
  return rc;
}

static int test_gzip_filename_is_normalized(void)
{
  static const uint8_t gzip_data[] = {
    0x1f, 0x8b, 0x08, 0x08, 0, 0, 0, 0, 0, 0xff,
    '.', '/', 'd', 'i', 'r', '\\', '/', '/', '.', '/', 'h', 'e', 'l', 'l',
    'o', '.', 't', 'x', 't', 0,
    0x86, 0xa6, 0x10, 0x36, 5, 0, 0, 0
  };
  ZZ9KArchiveGzipInfo info;

  if (!zz9k_archive_gzip_info(gzip_data, sizeof(gzip_data), &info)) {
    return 1;
  }
  if (strcmp(info.name, "dir/hello.txt") != 0) return 2;
  if (!zz9k_archive_path_is_safe(info.name)) return 3;
  return 0;
}

static int test_lzma_info(void)
{
  uint8_t lzma[32];
  ZZ9KArchiveLzmaInfo info;
  uint32_t capacity;
  uint32_t lzma_len;

  make_lzma_alone(lzma, &lzma_len, 79U);
  if (!zz9k_archive_lzma_info(lzma, lzma_len, &info)) {
    return 1;
  }
  if (strcmp(info.name, "output") != 0) return 2;
  if (!info.size_known) return 3;
  if (info.uncompressed_size != 79U) return 4;
  if (info.compressed_offset != 0U) return 5;
  if (info.compressed_size != lzma_len) return 6;
  memset(&info, 0, sizeof(info));
  if (!zz9k_archive_lzma_info_from_header(lzma, 14U, 4096U, &info)) {
    return 15;
  }
  if (!info.size_known) return 16;
  if (info.uncompressed_size != 79U) return 17;
  if (info.compressed_offset != 0U) return 18;
  if (info.compressed_size != 4096U) return 19;
  if (!zz9k_archive_lzma_output_capacity(&info, 0U, &capacity)) {
    return 7;
  }
  if (capacity != 79U) return 8;

  make_lzma_alone(lzma, &lzma_len, UINT64_MAX);
  memset(&info, 0, sizeof(info));
  if (!zz9k_archive_lzma_info(lzma, lzma_len, &info)) {
    return 9;
  }
  if (info.size_known) return 10;
  if (info.uncompressed_size != 0U) return 11;
  if (zz9k_archive_lzma_output_capacity(&info, 0U, &capacity)) {
    return 12;
  }
  if (!zz9k_archive_lzma_output_capacity(&info, 256U, &capacity)) {
    return 13;
  }
  if (capacity != 256U) return 14;
  return 0;
}

static int test_zip_list(void)
{
  uint8_t zip[160];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;

  make_zip_store(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].method != 0U) return 4;
  if (entries[0].compressed_size != 5U) return 5;
  if (entries[0].uncompressed_size != 5U) return 6;
  if (entries[0].data_offset != 39U) return 7;
  if (entries[0].crc32 != 0x3610a686UL) return 8;
  return 0;
}

static int test_lha_lh0_list(void)
{
  uint8_t lha[128];
  ZZ9KArchiveEntry entries[2];
  uint32_t lha_len;
  uint32_t count;

  make_lha_lh0(lha, &lha_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_lha_list(lha, lha_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_LHA_METHOD_LH0) return 4;
  if (entries[0].compressed_size != 5U) return 5;
  if (entries[0].uncompressed_size != 5U) return 6;
  if (entries[0].data_offset != lha_len - 6U) return 7;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 8;
  return 0;
}

static int test_lha_level1_lh5_list_reports_unsupported_method(void)
{
  uint8_t lha[128];
  ZZ9KArchiveEntry entries[2];
  uint32_t lha_len;
  uint32_t count;

  make_lha_lh5_level1(lha, &lha_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_lha_list(lha, lha_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].method == ZZ9K_ARCHIVE_LHA_METHOD_LH0) return 4;
  if (entries[0].compressed_size != 10U) return 5;
  if (entries[0].uncompressed_size != 20U) return 6;
  if (entries[0].data_offset != lha_len - 11U) return 7;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 8;
  return 0;
}

static int test_lha_level1_extension_name_and_dir_are_used(void)
{
  uint8_t lha[192];
  ZZ9KArchiveEntry entries[2];
  uint32_t lha_len;
  uint32_t count;

  make_lha_lh5_level1_ext_name(lha, &lha_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_lha_list(lha, lha_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/renamed.txt") != 0) return 3;
  if (entries[0].method == ZZ9K_ARCHIVE_LHA_METHOD_LH0) return 4;
  if (entries[0].compressed_size != 10U) return 5;
  if (entries[0].uncompressed_size != 20U) return 6;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 7;
  return 0;
}

static int test_lha_level1_lhd_and_lh0_extract(void)
{
  const char *output_dir = "archive_tool_lha_l1_out";
  const char *output_path = "archive_tool_lha_l1_out/dir/stored.txt";
  uint8_t lha[256];
  ZZ9KArchiveEntry entries[4];
  uint8_t actual[5];
  FILE *file = 0;
  uint32_t lha_len;
  uint32_t count;
  int rc = 0;

  remove(output_path);
  remove("archive_tool_lha_l1_out/dir");
  remove(output_dir);
  make_lha_level1_lhd_and_lh0(lha, &lha_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_lha_list(lha, lha_len, entries, 4U, &count)) return 1;
  if (count != 2U) return 2;
  if (strcmp(entries[0].name, "dir/") != 0) return 3;
  if (!entries[0].is_dir) return 4;
  if (strcmp(entries[1].name, "dir/stored.txt") != 0) return 5;
  if (entries[1].method != ZZ9K_ARCHIVE_LHA_METHOD_LH0) return 6;
  if (entries[1].is_dir) return 7;
  if (!zz9k_archive_handle_lha(0, 0, lha, lha_len, "x", output_dir)) {
    rc = 8;
    goto out;
  }
  file = fopen(output_path, "rb");
  if (!file) {
    rc = 9;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 10;
    goto out;
  }
  if (memcmp(actual, "hello", 5U) != 0) rc = 11;

out:
  if (file) fclose(file);
  remove(output_path);
  remove("archive_tool_lha_l1_out/dir");
  remove(output_dir);
  return rc;
}

static int test_lha_extract_match_filter_skips_unmatched(void)
{
  const char *output_dir = "archive_tool_lha_match_out";
  const char *output_path = "archive_tool_lha_match_out/dir/stored.txt";
  uint8_t lha[256];
  uint8_t actual[5];
  FILE *file = 0;
  uint32_t lha_len;
  int rc = 0;

  remove(output_path);
  remove("archive_tool_lha_match_out/dir");
  remove(output_dir);
  make_lha_level1_lhd_and_lh0(lha, &lha_len);
  zz9k_archive_match_filter = "stored";
  if (!zz9k_archive_handle_lha(0, 0, lha, lha_len, "x", output_dir)) {
    rc = 1;
    goto out;
  }
  file = fopen(output_path, "rb");
  if (!file) {
    rc = 2;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 3;
    goto out;
  }
  if (memcmp(actual, "hello", 5U) != 0) rc = 4;
out:
  zz9k_archive_match_filter = 0;
  if (file) fclose(file);
  remove(output_path);
  remove("archive_tool_lha_match_out/dir");
  remove(output_dir);
  return rc;
}

static int test_lha_level2_extension_name_and_dir_are_listed(void)
{
  uint8_t lha[192];
  ZZ9KArchiveEntry entries[2];
  uint32_t lha_len;
  uint32_t count;

  make_lha_lh5_level2_ext_name(lha, &lha_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_lha_list(lha, lha_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/level2.txt") != 0) return 3;
  if (entries[0].method == ZZ9K_ARCHIVE_LHA_METHOD_LH0) return 4;
  if (entries[0].compressed_size != 10U) return 5;
  if (entries[0].uncompressed_size != 20U) return 6;
  if (entries[0].is_dir) return 7;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 8;
  return 0;
}

static int test_lha_lh5_docker_fixture_extracts(void)
{
  const char *output_dir = "archive_tool_lha_lh5_out";
  const char *output_path =
      "archive_tool_lha_lh5_out/build/lha-fixture/src/dir/hello.txt";
  const char *line = "hello from lh5 repeated line\n";
  ZZ9KArchiveEntry entries[2];
  uint32_t count = 0U;
  FILE *file;
  uint8_t buffer[29];
  long size;
  int i;

  remove(output_path);
  remove("archive_tool_lha_lh5_out/build/lha-fixture/src/dir");
  remove("archive_tool_lha_lh5_out/build/lha-fixture/src");
  remove("archive_tool_lha_lh5_out/build/lha-fixture");
  remove("archive_tool_lha_lh5_out/build");
  remove(output_dir);

  if (!zz9k_archive_lha_list(lha_lh5_docker_fixture,
                             (uint32_t)sizeof(lha_lh5_docker_fixture),
                             entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "build/lha-fixture/src/dir/hello.txt") != 0) {
    return 3;
  }
  if (entries[0].method != ZZ9K_ARCHIVE_LHA_METHOD_LH5) return 4;
  if (entries[0].compressed_size != 185U) return 5;
  if (entries[0].uncompressed_size != 58000U) return 6;
  if (entries[0].crc32 != 0x14e1U) return 7;
  if (!zz9k_archive_handle_lha(
          0, 0, lha_lh5_docker_fixture,
          (uint32_t)sizeof(lha_lh5_docker_fixture), "x", output_dir)) {
    return 8;
  }
  file = fopen(output_path, "rb");
  if (!file) return 9;
  if (fseek(file, 0L, SEEK_END) != 0) {
    fclose(file);
    return 10;
  }
  size = ftell(file);
  if (size != 58000L || fseek(file, 0L, SEEK_SET) != 0) {
    fclose(file);
    return 11;
  }
  for (i = 0; i < 3; i++) {
    if (fread(buffer, 1U, sizeof(buffer), file) != sizeof(buffer) ||
        memcmp(buffer, line, sizeof(buffer)) != 0) {
      fclose(file);
      return 12;
    }
  }
  fclose(file);
  remove(output_path);
  remove("archive_tool_lha_lh5_out/build/lha-fixture/src/dir");
  remove("archive_tool_lha_lh5_out/build/lha-fixture/src");
  remove("archive_tool_lha_lh5_out/build/lha-fixture");
  remove("archive_tool_lha_lh5_out/build");
  remove(output_dir);
  return 0;
}

static int test_lha_lh6_lh7_docker_fixtures_extract(void)
{
  const char *output_dir = "archive_tool_lha_lh67_out";
  const char *output_path =
      "archive_tool_lha_lh67_out/build/lha-method-fixtures/src/payload.txt";
  const char *line = "method fixture repeated line\n";
  uint8_t fixture[sizeof(lha_lh6_docker_fixture)];
  ZZ9KArchiveEntry entries[2];
  uint32_t count;
  FILE *file;
  uint8_t buffer[29];
  long size;
  int pass;

  for (pass = 0; pass < 2; pass++) {
    remove(output_path);
    remove("archive_tool_lha_lh67_out/build/lha-method-fixtures/src");
    remove("archive_tool_lha_lh67_out/build/lha-method-fixtures");
    remove("archive_tool_lha_lh67_out/build");
    remove(output_dir);
    memcpy(fixture, lha_lh6_docker_fixture, sizeof(fixture));
    if (pass == 1) {
      fixture[5] = '7';
      fixture[27] = 0x3fU;
      fixture[28] = 0x5bU;
    }
    memset(entries, 0, sizeof(entries));
    count = 0U;
    if (!zz9k_archive_lha_list(fixture, (uint32_t)sizeof(fixture),
                               entries, 2U, &count)) {
      return 1 + pass * 20;
    }
    if (count != 1U) return 2 + pass * 20;
    if (entries[0].method !=
        (pass == 0 ? ZZ9K_ARCHIVE_LHA_METHOD_LH6 :
                     ZZ9K_ARCHIVE_LHA_METHOD_LH7)) {
      return 3 + pass * 20;
    }
    if (entries[0].compressed_size != 184U ||
        entries[0].uncompressed_size != 58000U ||
        entries[0].crc32 != 0x692dU) {
      return 4 + pass * 20;
    }
    if (!zz9k_archive_handle_lha(
            0, 0, fixture, (uint32_t)sizeof(fixture), "x", output_dir)) {
      return 5 + pass * 20;
    }
    file = fopen(output_path, "rb");
    if (!file) return 6 + pass * 20;
    if (fseek(file, 0L, SEEK_END) != 0) {
      fclose(file);
      return 7 + pass * 20;
    }
    size = ftell(file);
    if (size != 58000L || fseek(file, 0L, SEEK_SET) != 0) {
      fclose(file);
      return 8 + pass * 20;
    }
    if (fread(buffer, 1U, sizeof(buffer), file) != sizeof(buffer) ||
        memcmp(buffer, line, sizeof(buffer)) != 0) {
      fclose(file);
      return 9 + pass * 20;
    }
    fclose(file);
  }
  remove(output_path);
  remove("archive_tool_lha_lh67_out/build/lha-method-fixtures/src");
  remove("archive_tool_lha_lh67_out/build/lha-method-fixtures");
  remove("archive_tool_lha_lh67_out/build");
  remove(output_dir);
  return 0;
}

static int test_lha_lh1_docker_fixture_extracts(void)
{
  const char *output_dir = "archive_tool_lha_lh1_out";
  const char *output_path =
      "archive_tool_lha_lh1_out/build/lha-oldmethod-fixtures/src/payload.txt";
  const char *line = "lh1 fixture line\n";
  ZZ9KArchiveEntry entries[2];
  FILE *file;
  uint8_t buffer[17];
  uint32_t count;

  remove(output_path);
  remove("archive_tool_lha_lh1_out/build/lha-oldmethod-fixtures/src");
  remove("archive_tool_lha_lh1_out/build/lha-oldmethod-fixtures");
  remove("archive_tool_lha_lh1_out/build");
  remove(output_dir);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_lha_list(lha_lh1_docker_fixture,
                             (uint32_t)sizeof(lha_lh1_docker_fixture),
                             entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (entries[0].method != ZZ9K_ARCHIVE_LHA_METHOD_LH1) return 3;
  if (entries[0].compressed_size != 61U) return 4;
  if (entries[0].uncompressed_size != 1360U) return 5;
  if (entries[0].crc32 != 0x6355U) return 6;
  if (!zz9k_archive_handle_lha(
          0, 0, lha_lh1_docker_fixture,
          (uint32_t)sizeof(lha_lh1_docker_fixture), "x", output_dir)) {
    return 7;
  }
  file = fopen(output_path, "rb");
  if (!file) return 8;
  if (fread(buffer, 1U, sizeof(buffer), file) != sizeof(buffer) ||
      memcmp(buffer, line, sizeof(buffer)) != 0) {
    fclose(file);
    return 9;
  }
  fclose(file);
  remove(output_path);
  remove("archive_tool_lha_lh1_out/build/lha-oldmethod-fixtures/src");
  remove("archive_tool_lha_lh1_out/build/lha-oldmethod-fixtures");
  remove("archive_tool_lha_lh1_out/build");
  remove(output_dir);
  return 0;
}

static int test_lha_lh0_extract(void)
{
  const char *output_dir = "archive_tool_lha_out";
  const char *output_path = "archive_tool_lha_out/dir/hello.txt";
  uint8_t lha[128];
  uint8_t actual[5];
  FILE *file = 0;
  uint32_t lha_len;
  int rc = 0;

  remove(output_path);
  remove("archive_tool_lha_out/dir");
  remove(output_dir);
  make_lha_lh0(lha, &lha_len);
  if (!zz9k_archive_handle_lha(0, 0, lha, lha_len, "x", output_dir)) {
    rc = 1;
    goto out;
  }
  file = fopen(output_path, "rb");
  if (!file) {
    rc = 2;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 3;
    goto out;
  }
  if (memcmp(actual, "hello", 5U) != 0) rc = 4;

out:
  if (file) fclose(file);
  remove(output_path);
  remove("archive_tool_lha_out/dir");
  remove(output_dir);
  return rc;
}

static int test_zip_backslash_names_are_normalized(void)
{
  const char *path = "archive_tool_zip_backslash.tmp";
  uint8_t zip[192];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_backslash_name(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 5;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 2U, &count)) {
    rc = 6;
    goto out;
  }
  if (count != 1U) {
    rc = 7;
    goto out;
  }
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) rc = 8;

out:
  remove(path);
  return rc;
}

static int test_zip_current_dir_prefix_names_are_normalized(void)
{
  const char *path = "archive_tool_zip_dot_prefix.tmp";
  uint8_t zip[224];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_named(zip, &zip_len, "./dir/hello.txt", "hello",
                       0x3610a686UL, 0U);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 5;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 2U, &count)) {
    rc = 6;
    goto out;
  }
  if (count != 1U) {
    rc = 7;
    goto out;
  }
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) rc = 8;

out:
  remove(path);
  return rc;
}

static int test_zip_current_dir_components_are_normalized(void)
{
  const char *path = "archive_tool_zip_dot_component.tmp";
  uint8_t zip[224];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_named(zip, &zip_len, "dir/./hello.txt", "hello",
                       0x3610a686UL, 0U);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 5;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 2U, &count)) {
    rc = 6;
    goto out;
  }
  if (count != 1U) {
    rc = 7;
    goto out;
  }
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) rc = 8;

out:
  remove(path);
  return rc;
}

static int test_zip_duplicate_slashes_are_normalized(void)
{
  const char *path = "archive_tool_zip_double_slash.tmp";
  uint8_t zip[224];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_named(zip, &zip_len, "dir//hello.txt", "hello",
                       0x3610a686UL, 0U);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 4;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 5;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 2U, &count)) {
    rc = 6;
    goto out;
  }
  if (count != 1U) {
    rc = 7;
    goto out;
  }
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) rc = 8;

out:
  remove(path);
  return rc;
}

static int test_zip_current_dir_prefixed_directory_is_directory(void)
{
  const char *path = "archive_tool_zip_dot_dir.tmp";
  uint8_t zip[160];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_named(zip, &zip_len, "./dir/", 0, 0U, 0U);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/") != 0) return 3;
  if (!entries[0].is_dir) return 4;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 5;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 6;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 2U, &count)) {
    rc = 7;
    goto out;
  }
  if (count != 1U) rc = 8;
  else if (strcmp(entries[0].name, "dir/") != 0) rc = 9;
  else if (!entries[0].is_dir) rc = 10;

out:
  remove(path);
  return rc;
}

static int test_zip_root_current_dir_metadata_is_skipped(void)
{
  const char *path = "archive_tool_zip_root_dot.tmp";
  uint8_t zip[128];
  ZZ9KArchiveEntry entries[1];
  uint32_t zip_len;
  uint32_t count = 123U;
  int rc = 0;

  make_zip_store_named(zip, &zip_len, "./", 0, 0U, 0x10U);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 0U, &count)) {
    return 1;
  }
  if (count != 0U) return 2;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 3;
  count = 123U;
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 0U, &count)) {
    rc = 4;
    goto out;
  }
  if (count != 0U) rc = 5;

out:
  remove(path);
  return rc;
}

static int test_zip_root_current_dir_metadata_does_not_use_output_slot(void)
{
  const char *path = "archive_tool_zip_root_dot_file.tmp";
  uint8_t zip[320];
  ZZ9KArchiveEntry entries[1];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_root_current_dir_then_file(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 1U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/hello.txt") != 0) return 3;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 4;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 1U, &count)) {
    rc = 5;
    goto out;
  }
  if (count != 1U) rc = 6;
  else if (strcmp(entries[0].name, "dir/hello.txt") != 0) rc = 7;

out:
  remove(path);
  return rc;
}

static int test_zip_list_file(void)
{
  const char *path = "archive_tool_zip_file.tmp";
  uint8_t zip[160];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store(zip, &zip_len);
  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 1;

  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(
          path, zip_len, entries, 2U, &count)) {
    rc = 2;
    goto out;
  }
  if (count != 1U) rc = 3;
  else if (strcmp(entries[0].name, "hello.txt") != 0) rc = 4;
  else if (entries[0].method != ZZ9K_ARCHIVE_ZIP_METHOD_STORE) rc = 5;
  else if (entries[0].compressed_size != 5U) rc = 6;
  else if (entries[0].uncompressed_size != 5U) rc = 7;
  else if (entries[0].data_offset != 39U) rc = 8;
  else if (entries[0].crc32 != 0x3610a686UL) rc = 9;

out:
  remove(path);
  return rc;
}

static int test_zip_list_file_allows_trailing_central_data(void)
{
  const char *path = "archive_tool_zip_file_trailing.tmp";
  uint8_t zip[160];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t eocd;
  uint32_t cd_size;
  uint32_t count;
  int rc = 0;

  make_zip_store(zip, &zip_len);
  eocd = zip_len - 22U;
  cd_size = zz9k_archive_get_le32(zip + eocd + 12U);
  memmove(zip + eocd + 6U, zip + eocd, 22U);
  zip[eocd + 0U] = 0x50U;
  zip[eocd + 1U] = 0x4bU;
  zip[eocd + 2U] = 0x05U;
  zip[eocd + 3U] = 0x05U;
  zip[eocd + 4U] = 0U;
  zip[eocd + 5U] = 0U;
  put_le32(zip + eocd + 6U + 12U, cd_size + 6U);
  zip_len += 6U;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 1;

  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(
          path, zip_len, entries, 2U, &count)) {
    rc = 2;
    goto out;
  }
  if (count != 1U) rc = 3;
  else if (strcmp(entries[0].name, "hello.txt") != 0) rc = 4;
  else if (entries[0].data_offset != 39U) rc = 5;

out:
  remove(path);
  return rc;
}

static int test_zip64_extra_resolves_small_entry(void)
{
  const char *path = "archive_tool_zip64_extra.tmp";
  uint8_t zip[180];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_zip64_extra(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_ZIP_METHOD_STORE) return 4;
  if (entries[0].compressed_size != 5U) return 5;
  if (entries[0].uncompressed_size != 5U) return 6;
  if (entries[0].data_offset != 39U) return 7;
  if (entries[0].crc32 != 0x3610a686UL) return 8;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 9;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(
          path, zip_len, entries, 2U, &count)) {
    rc = 10;
    goto out;
  }
  if (count != 1U) rc = 11;
  else if (strcmp(entries[0].name, "hello.txt") != 0) rc = 12;
  else if (entries[0].compressed_size != 5U) rc = 13;
  else if (entries[0].uncompressed_size != 5U) rc = 14;
  else if (entries[0].data_offset != 39U) rc = 15;

out:
  remove(path);
  return rc;
}

static int test_zip64_local_extra_resolves_small_entry(void)
{
  const char *path = "archive_tool_zip64_local_extra.tmp";
  uint8_t zip[220];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_zip64_local_extra(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].compressed_size != 5U) return 4;
  if (entries[0].uncompressed_size != 5U) return 5;
  if (entries[0].data_offset != 59U) return 6;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 7;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(
          path, zip_len, entries, 2U, &count)) {
    rc = 8;
    goto out;
  }
  if (count != 1U) rc = 9;
  else if (entries[0].compressed_size != 5U) rc = 10;
  else if (entries[0].uncompressed_size != 5U) rc = 11;
  else if (entries[0].data_offset != 59U) rc = 12;

out:
  remove(path);
  return rc;
}

static int test_zip64_offset_extra_resolves_small_entry(void)
{
  const char *path = "archive_tool_zip64_offset_extra.tmp";
  uint8_t zip[180];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store_zip64_offset_extra(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].compressed_size != 5U) return 4;
  if (entries[0].uncompressed_size != 5U) return 5;
  if (entries[0].data_offset != 39U) return 6;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 7;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(
          path, zip_len, entries, 2U, &count)) {
    rc = 8;
    goto out;
  }
  if (count != 1U) rc = 9;
  else if (entries[0].compressed_size != 5U) rc = 10;
  else if (entries[0].uncompressed_size != 5U) rc = 11;
  else if (entries[0].data_offset != 39U) rc = 12;

out:
  remove(path);
  return rc;
}

static int test_zip64_eocd_resolves_small_archive(void)
{
  const char *path = "archive_tool_zip64_eocd.tmp";
  uint8_t zip[240];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count = 123U;
  int rc = 0;

  make_zip_store_zip64_eocd(zip, &zip_len);
  if (!zz9k_archive_count_zip_entries(zip, zip_len, &count)) {
    return 14;
  }
  if (count != 1U) return 15;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "hello.txt") != 0) return 3;
  if (entries[0].compressed_size != 5U) return 4;
  if (entries[0].uncompressed_size != 5U) return 5;
  if (entries[0].data_offset != 39U) return 6;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 7;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(
          path, zip_len, entries, 2U, &count)) {
    rc = 8;
    goto out;
  }
  if (count != 1U) rc = 9;
  else if (strcmp(entries[0].name, "hello.txt") != 0) rc = 10;
  else if (entries[0].compressed_size != 5U) rc = 11;
  else if (entries[0].uncompressed_size != 5U) rc = 12;
  else if (entries[0].data_offset != 39U) rc = 13;

out:
  remove(path);
  return rc;
}

static int test_zip_directory_external_attributes(void)
{
  const char *path = "archive_tool_zip_dir_attr.tmp";
  uint8_t zip[128];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_directory_external_attr(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir") != 0) return 3;
  if (!entries[0].is_dir) return 4;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 5;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list_file(
          path, zip_len, entries, 2U, &count)) {
    rc = 6;
    goto out;
  }
  if (count != 1U) rc = 7;
  else if (strcmp(entries[0].name, "dir") != 0) rc = 8;
  else if (!entries[0].is_dir) rc = 9;

out:
  remove(path);
  return rc;
}

static int test_zip_crc_validation_helpers(void)
{
  uint8_t zip[160];
  ZZ9KArchiveEntry entries[2];
  ZZ9KDecompressResult result;
  uint32_t zip_len;
  uint32_t count;

  make_zip_store(zip, &zip_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }
  if (!zz9k_archive_zip_stored_entry_crc_matches(
          &entries[0], zip + entries[0].data_offset)) {
    return 2;
  }
  entries[0].crc32 = 0U;
  if (zz9k_archive_zip_stored_entry_crc_matches(
          &entries[0], zip + entries[0].data_offset)) {
    return 3;
  }
  entries[0].crc32 = 0x3610a686UL;

  memset(&result, 0, sizeof(result));
  result.bytes_written = 5U;
  result.checksum = 0x3610a686UL;
  if (!zz9k_archive_zip_result_matches_entry(&entries[0], &result)) {
    return 4;
  }
  result.checksum = 0U;
  if (zz9k_archive_zip_result_matches_entry(&entries[0], &result)) {
    return 5;
  }
  return 0;
}

static int test_7z_result_crc_validation_helpers(void)
{
  ZZ9KArchiveEntry entry;
  ZZ9KDecompressResult result;

  memset(&entry, 0, sizeof(entry));
  entry.uncompressed_size = 5U;
  entry.crc32 = 0x3610a686UL;
  entry.flags = ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32;

  memset(&result, 0, sizeof(result));
  result.bytes_written = 5U;
  result.checksum = 0x3610a686UL;
  if (!zz9k_archive_7z_result_matches_entry(&entry, &result)) {
    return 1;
  }
  result.checksum = 0U;
  if (zz9k_archive_7z_result_matches_entry(&entry, &result)) {
    return 2;
  }
  entry.flags = 0U;
  if (!zz9k_archive_7z_result_matches_entry(&entry, &result)) {
    return 3;
  }
  result.bytes_written = 4U;
  if (zz9k_archive_7z_result_matches_entry(&entry, &result)) {
    return 4;
  }
  return 0;
}

static int test_empty_zip_archive_is_valid(void)
{
  const char *path = "archive_tool_empty_zip.tmp";
  uint8_t zip[22];
  ZZ9KArchiveEntry entries[1];
  uint32_t zip_len;
  uint32_t count = 123U;
  int rc = 0;

  make_zip_empty(zip, &zip_len);
  if (!zz9k_archive_count_zip_entries(zip, zip_len, &count)) {
    return 1;
  }
  if (count != 0U) return 2;
  memset(entries, 0, sizeof(entries));
  count = 123U;
  if (!zz9k_archive_zip_list(zip, zip_len, entries, 1U, &count)) {
    return 3;
  }
  if (count != 0U) return 4;

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 5;
  count = 123U;
  if (!zz9k_archive_zip_list_file(path, zip_len, entries, 1U, &count)) {
    rc = 6;
    goto out;
  }
  if (count != 0U) rc = 7;

out:
  remove(path);
  return rc;
}

static int test_zip_rejects_mismatched_local_header_name(void)
{
  const char *path = "archive_tool_zip_bad_local.tmp";
  uint8_t zip[160];
  ZZ9KArchiveEntry entries[2];
  uint32_t zip_len;
  uint32_t count;
  int rc = 0;

  make_zip_store(zip, &zip_len);
  zip[30U] = 'j';
  memset(entries, 0, sizeof(entries));
  if (zz9k_archive_zip_list(zip, zip_len, entries, 2U, &count)) {
    return 1;
  }

  remove(path);
  if (!write_test_file(path, zip, zip_len)) return 2;
  if (zz9k_archive_zip_list_file(path, zip_len, entries, 2U, &count)) {
    rc = 3;
    goto out;
  }

out:
  remove(path);
  return rc;
}

static int test_tar_list(void)
{
  uint8_t tar[1536];
  ZZ9KArchiveEntry entries[2];
  uint32_t tar_len;
  uint32_t count;

  make_tar(tar, &tar_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, "dir/file.txt") != 0) return 3;
  if (entries[0].method != ZZ9K_ARCHIVE_TAR_METHOD_STORE) return 4;
  if (entries[0].uncompressed_size != 5U) return 5;
  if (entries[0].data_offset != 512U) return 6;
  return 0;
}

static int test_tar_stream_extracts_split_chunks(void)
{
  const char *output_name = "archive_tool_tar_stream_out.tmp";
  uint8_t tar[1536];
  uint8_t actual[5];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  uint32_t tar_len;
  uint32_t pos = 0U;
  int rc = 0;

  remove(output_name);
  make_tar_single_file(tar, &tar_len, output_name);
  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 37U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 1;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 2;
    goto out;
  }
  if (stream.count != 1U) {
    rc = 3;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 4;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 5;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 6;

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(output_name);
  return rc;
}

static int test_tar_stream_normalizes_current_dir_prefix(void)
{
  const char *output_name = "archive_tool_tar_prefix_out.tmp";
  uint8_t tar[2560];
  uint8_t actual[5];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  uint32_t tar_len;
  uint32_t pos = 0U;
  int rc = 0;

  remove(output_name);
  make_tar_current_dir_prefixed_file(tar, &tar_len, output_name);
  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 29U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 1;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 2;
    goto out;
  }
  if (stream.count != 1U) {
    rc = 3;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 4;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 5;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 6;

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(output_name);
  return rc;
}

static int test_tar_root_current_dir_metadata_is_skipped(void)
{
  uint8_t tar[1536];
  ZZ9KArchiveEntry entries[1];
  uint32_t tar_len;
  uint32_t count = 123U;

  make_tar_root_current_dir_metadata(tar, &tar_len);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 0U) return 2;
  memset(entries, 0, sizeof(entries));
  count = 123U;
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 0U, &count)) {
    return 3;
  }
  if (count != 0U) return 4;
  return 0;
}

static int test_tar_current_dir_components_are_normalized(void)
{
  uint8_t tar[1536];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  uint32_t tar_len;
  uint32_t count = 123U;
  uint32_t pos = 0U;
  int rc = 0;

  make_tar_current_dir_component_file(tar, &tar_len);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 3;
  }
  if (count != 1U) return 4;
  if (strcmp(entries[0].name, "dir/file.txt") != 0) return 5;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 6;

  zz9k_archive_tar_stream_init(&stream, "t", ".");
  while (pos < tar_len) {
    uint32_t part = 37U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 7;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 8;
    goto out;
  }
  if (stream.count != 1U) rc = 9;

out:
  zz9k_archive_tar_stream_cleanup(&stream);
  return rc;
}

static int test_tar_duplicate_slashes_are_normalized(void)
{
  uint8_t tar[1536];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  uint32_t tar_len;
  uint32_t count = 123U;
  uint32_t pos = 0U;
  int rc = 0;

  make_tar_duplicate_slash_file(tar, &tar_len);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 3;
  }
  if (count != 1U) return 4;
  if (strcmp(entries[0].name, "dir/file.txt") != 0) return 5;
  if (!zz9k_archive_path_is_safe(entries[0].name)) return 6;

  zz9k_archive_tar_stream_init(&stream, "t", ".");
  while (pos < tar_len) {
    uint32_t part = 41U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 7;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 8;
    goto out;
  }
  if (stream.count != 1U) rc = 9;

out:
  zz9k_archive_tar_stream_cleanup(&stream);
  return rc;
}

static int test_tar_gnu_long_root_current_dir_metadata_is_skipped(void)
{
  uint8_t tar[2048];
  ZZ9KArchiveEntry entries[1];
  ZZ9KArchiveTarStream stream;
  uint32_t tar_len;
  uint32_t count = 123U;
  uint32_t pos = 0U;
  int rc = 0;

  make_tar_gnu_long_root_current_dir_metadata(tar, &tar_len);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 0U) return 2;
  memset(entries, 0, sizeof(entries));
  count = 123U;
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 0U, &count)) {
    return 3;
  }
  if (count != 0U) return 4;

  zz9k_archive_tar_stream_init(&stream, "t", ".");
  while (pos < tar_len) {
    uint32_t part = 31U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 5;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 6;
    goto out;
  }
  if (stream.count != 0U) rc = 7;

out:
  zz9k_archive_tar_stream_cleanup(&stream);
  return rc;
}

static int test_tar_trailing_slash_entry_is_directory(void)
{
  uint8_t tar[2560];
  ZZ9KArchiveEntry entries[3];
  uint32_t tar_len;
  uint32_t count;

  make_tar_trailing_slash_directory(tar, &tar_len);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 3U, &count)) {
    return 1;
  }
  if (count != 2U) return 2;
  if (strcmp(entries[0].name, "archive_tool_tar_trailing_dir/") != 0) {
    return 3;
  }
  if (!entries[0].is_dir) return 4;
  if (entries[0].uncompressed_size != 0U) return 5;
  if (strcmp(entries[1].name,
             "archive_tool_tar_trailing_dir/file.txt") != 0) {
    return 6;
  }
  if (entries[1].is_dir) return 7;
  return 0;
}

static int test_tar_gnu_long_name_applies_to_next_entry(void)
{
  const char *output_name = "archive_tool_tar_gnu_longname_out.tmp";
  uint8_t tar[3072];
  uint8_t actual[5];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  uint32_t tar_len;
  uint32_t count;
  uint32_t pos = 0U;
  int rc = 0;

  remove(output_name);
  make_tar_gnu_long_name_file(tar, &tar_len, output_name);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, output_name) != 0) return 3;

  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 31U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 4;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 5;
    goto out;
  }
  if (stream.count != 1U) {
    rc = 6;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 7;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 8;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 9;

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(output_name);
  return rc;
}

static int test_tar_pax_path_applies_to_next_entry(void)
{
  const char *output_name = "archive_tool_tar_pax_path_out.tmp";
  uint8_t tar[3072];
  uint8_t actual[5];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  uint32_t tar_len;
  uint32_t count;
  uint32_t pos = 0U;
  int rc = 0;

  remove(output_name);
  make_tar_pax_path_file(tar, &tar_len, output_name);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, output_name) != 0) return 3;

  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 23U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 4;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 5;
    goto out;
  }
  if (stream.count != 1U) {
    rc = 6;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 7;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 8;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 9;

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(output_name);
  return rc;
}

static int test_tar_pax_root_current_dir_metadata_is_skipped(void)
{
  uint8_t tar[2048];
  ZZ9KArchiveEntry entries[1];
  ZZ9KArchiveTarStream stream;
  uint32_t tar_len;
  uint32_t count = 123U;
  uint32_t pos = 0U;
  int rc = 0;

  make_tar_pax_root_current_dir_metadata(tar, &tar_len);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 0U) return 2;
  memset(entries, 0, sizeof(entries));
  count = 123U;
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 0U, &count)) {
    return 3;
  }
  if (count != 0U) return 4;

  zz9k_archive_tar_stream_init(&stream, "t", ".");
  while (pos < tar_len) {
    uint32_t part = 19U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 5;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 6;
    goto out;
  }
  if (stream.count != 0U) rc = 7;

out:
  zz9k_archive_tar_stream_cleanup(&stream);
  return rc;
}

static int test_tar_pax_size_applies_to_next_entry(void)
{
  const char *output_name = "archive_tool_tar_pax_size_out.tmp";
  uint8_t tar[3072];
  uint8_t actual[5];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  uint32_t tar_len;
  uint32_t count = 123U;
  uint32_t pos = 0U;
  int rc = 0;

  remove(output_name);
  make_tar_pax_size_file(tar, &tar_len, output_name);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 3;
  }
  if (count != 1U) return 4;
  if (strcmp(entries[0].name, output_name) != 0) return 5;
  if (entries[0].uncompressed_size != 5U) return 6;
  if (entries[0].data_offset != 1536U) return 7;

  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 17U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 8;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 9;
    goto out;
  }
  if (stream.count != 1U) {
    rc = 10;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 11;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 12;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 13;

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(output_name);
  return rc;
}

static int test_tar_stream_accepts_large_pax_header(void)
{
  const char *output_name = "archive_tool_tar_large_pax_out.tmp";
  uint8_t tar[4096];
  uint8_t actual[5];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  uint32_t tar_len;
  uint32_t count;
  uint32_t pos = 0U;
  int rc = 0;

  remove(output_name);
  make_tar_large_pax_size_file(tar, &tar_len, output_name);
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, output_name) != 0) return 3;
  if (entries[0].uncompressed_size != 5U) return 4;

  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 53U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 5;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 6;
    goto out;
  }
  if (stream.count != 1U) {
    rc = 7;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 8;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 9;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 10;

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(output_name);
  return rc;
}

static int test_tar_base256_size_is_accepted(void)
{
  const char *output_name = "archive_tool_tar_base256_out.tmp";
  uint8_t tar[1536];
  uint8_t actual[5];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  uint32_t tar_len;
  uint32_t count = 123U;
  uint32_t pos = 0U;
  int rc = 0;

  remove(output_name);
  make_tar_base256_size_file(tar, &tar_len, output_name);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 3;
  }
  if (count != 1U) return 4;
  if (strcmp(entries[0].name, output_name) != 0) return 5;
  if (entries[0].uncompressed_size != 5U) return 6;

  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 41U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 7;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 8;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 9;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 10;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 11;

out:
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(output_name);
  return rc;
}

static int test_tar_signed_checksum_is_accepted(void)
{
  const char *output_name = "archive_tool_tar_signed_sum_out.tmp";
  uint8_t tar[1536];
  ZZ9KArchiveEntry entries[2];
  uint32_t tar_len;
  uint32_t count = 123U;

  make_tar_signed_checksum_file(tar, &tar_len, output_name);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 3;
  }
  if (count != 1U) return 4;
  if (strcmp(entries[0].name, output_name) != 0) return 5;
  if (entries[0].uncompressed_size != 5U) return 6;
  return 0;
}

static int test_tar_skips_unsupported_special_entries(void)
{
  const char *special_name = "archive_tool_tar_special_link.tmp";
  const char *output_name = "archive_tool_tar_special_file.tmp";
  uint8_t tar[3072];
  uint8_t actual[5];
  ZZ9KArchiveEntry entries[2];
  ZZ9KArchiveTarStream stream;
  FILE *file = 0;
  FILE *special_file = 0;
  uint32_t tar_len;
  uint32_t count = 123U;
  uint32_t pos = 0U;
  int rc = 0;

  remove(special_name);
  remove(output_name);
  make_tar_special_then_file(tar, &tar_len, special_name, output_name);
  if (!zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 11;
  }
  if (count != 1U) return 12;
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_tar_list(tar, tar_len, entries, 2U, &count)) {
    return 1;
  }
  if (count != 1U) return 2;
  if (strcmp(entries[0].name, output_name) != 0) return 3;

  zz9k_archive_tar_stream_init(&stream, "x", ".");
  while (pos < tar_len) {
    uint32_t part = 19U;
    if (part > tar_len - pos) {
      part = tar_len - pos;
    }
    if (!zz9k_archive_tar_stream_consume(&stream, tar + pos, part)) {
      rc = 4;
      goto out;
    }
    pos += part;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    rc = 5;
    goto out;
  }
  if (stream.count != 1U) {
    rc = 6;
    goto out;
  }
  special_file = fopen(special_name, "rb");
  if (special_file) {
    rc = 7;
    goto out;
  }
  file = fopen(output_name, "rb");
  if (!file) {
    rc = 8;
    goto out;
  }
  if (fread(actual, 1U, sizeof(actual), file) != sizeof(actual)) {
    rc = 9;
    goto out;
  }
  if (memcmp(actual, "hello", sizeof(actual)) != 0) rc = 10;

out:
  if (special_file) {
    fclose(special_file);
  }
  if (file) {
    fclose(file);
  }
  zz9k_archive_tar_stream_cleanup(&stream);
  remove(special_name);
  remove(output_name);
  return rc;
}

static int test_empty_tar_archive_is_valid(void)
{
  uint8_t tar[1024];
  ZZ9KArchiveEntry entries[1];
  ZZ9KArchiveEntry *allocated = 0;
  ZZ9KArchiveTarStream stream;
  uint32_t count = 123U;

  memset(tar, 0, sizeof(tar));
  memset(entries, 0, sizeof(entries));
  if (!zz9k_archive_count_tar_entries(tar, sizeof(tar), &count)) {
    return 1;
  }
  if (count != 0U) return 2;
  count = 123U;
  if (!zz9k_archive_tar_list(tar, sizeof(tar), entries, 1U, &count)) {
    return 3;
  }
  if (count != 0U) return 4;
  if (!zz9k_archive_alloc_entries(0U, &allocated)) return 5;
  free(allocated);

  zz9k_archive_tar_stream_init(&stream, "t", ".");
  if (!zz9k_archive_tar_stream_consume(&stream, tar, sizeof(tar))) {
    return 6;
  }
  if (!zz9k_archive_tar_stream_finish(&stream)) {
    return 7;
  }
  if (stream.count != 0U) return 8;
  return 0;
}

static int test_tar_rejects_bad_header_checksum(void)
{
  uint8_t tar[1536];
  ZZ9KArchiveEntry entries[1];
  uint32_t tar_len;
  uint32_t count;

  make_tar_single_file(tar, &tar_len, "archive_tool_tar_bad_checksum.tmp");
  tar[148U] = '0';
  tar[149U] = '\0';
  if (zz9k_archive_count_tar_entries(tar, tar_len, &count)) {
    return 1;
  }
  if (zz9k_archive_tar_list(tar, tar_len, entries, 1U, &count)) {
    return 2;
  }
  return 0;
}

static int test_lha_method_to_compression_mapper(void)
{
  if (zz9k_archive_lha_method_to_compression(ZZ9K_ARCHIVE_LHA_METHOD_LH1) !=
      ZZ9K_COMPRESSION_LH1) {
    return 1;
  }
  if (zz9k_archive_lha_method_to_compression(ZZ9K_ARCHIVE_LHA_METHOD_LH5) !=
      ZZ9K_COMPRESSION_LH5) {
    return 2;
  }
  if (zz9k_archive_lha_method_to_compression(ZZ9K_ARCHIVE_LHA_METHOD_LH6) !=
      ZZ9K_COMPRESSION_LH6) {
    return 3;
  }
  if (zz9k_archive_lha_method_to_compression(ZZ9K_ARCHIVE_LHA_METHOD_LH7) !=
      ZZ9K_COMPRESSION_LH7) {
    return 4;
  }
  /* Unsupported / lh0-stored methods must not map to a codec algorithm. */
  if (zz9k_archive_lha_method_to_compression(ZZ9K_ARCHIVE_LHA_METHOD_LH0) !=
      0U) {
    return 5;
  }
  return 0;
}

int main(void)
{
  int rc;

  rc = test_detect_formats();
  if (rc) {
    printf("test_detect_formats failed: %d\n", rc);
    return 10 + rc;
  }
  rc = test_extract_refuses_existing_file();
  if (rc) {
    printf("test_extract_refuses_existing_file failed: %d\n", rc);
    return 11 + rc;
  }
  rc = test_extract_skip_existing_keeps_existing_file();
  if (rc) {
    printf("test_extract_skip_existing_keeps_existing_file failed: %d\n", rc);
    return 14 + rc;
  }
  rc = test_extract_strip_components_rewrites_output_path();
  if (rc) {
    printf("test_extract_strip_components_rewrites_output_path failed: %d\n",
           rc);
    return 15 + rc;
  }
  rc = test_extract_dry_run_does_not_write_file();
  if (rc) {
    printf("test_extract_dry_run_does_not_write_file failed: %d\n", rc);
    return 16 + rc;
  }
  rc = test_extract_dry_run_rejects_file_where_directory_is_needed();
  if (rc) {
    printf("test_extract_dry_run_rejects_file_where_directory_is_needed "
           "failed: %d\n", rc);
    return 17 + rc;
  }
  rc = test_extract_refuses_file_where_directory_is_needed();
  if (rc) {
    printf("test_extract_refuses_file_where_directory_is_needed failed: %d\n",
           rc);
    return 12 + rc;
  }
  rc = test_extract_refuses_directory_where_file_is_needed();
  if (rc) {
    printf("test_extract_refuses_directory_where_file_is_needed failed: %d\n",
           rc);
    return 13 + rc;
  }
  rc = test_7z_empty_list();
  if (rc) {
    printf("test_7z_empty_list failed: %d\n", rc);
    return 70 + rc;
  }
  rc = test_7z_zero_file_archive_uses_file_path();
  if (rc) {
    printf("test_7z_zero_file_archive_uses_file_path failed: %d\n", rc);
    return 75 + rc;
  }
  rc = test_7z_copy_list();
  if (rc) {
    printf("test_7z_copy_list failed: %d\n", rc);
    return 80 + rc;
  }
  rc = test_7z_current_dir_prefix_names_are_normalized();
  if (rc) {
    printf("test_7z_current_dir_prefix_names_are_normalized failed: %d\n",
           rc);
    return 81 + rc;
  }
  rc = test_7z_current_dir_components_are_normalized();
  if (rc) {
    printf("test_7z_current_dir_components_are_normalized failed: %d\n", rc);
    return 82 + rc;
  }
  rc = test_7z_duplicate_slashes_are_normalized();
  if (rc) {
    printf("test_7z_duplicate_slashes_are_normalized failed: %d\n", rc);
    return 86 + rc;
  }
  rc = test_7z_root_current_dir_metadata_is_skipped();
  if (rc) {
    printf("test_7z_root_current_dir_metadata_is_skipped failed: %d\n", rc);
    return 83 + rc;
  }
  rc = test_7z_root_current_dir_metadata_does_not_use_output_slot();
  if (rc) {
    printf("test_7z_root_current_dir_metadata_does_not_use_output_slot "
           "failed: %d\n", rc);
    return 85 + rc;
  }
  rc = test_7z_copy_skips_common_file_metadata();
  if (rc) {
    printf("test_7z_copy_skips_common_file_metadata failed: %d\n", rc);
    return 82 + rc;
  }
  rc = test_7z_copy_rejects_unknown_file_metadata();
  if (rc) {
    printf("test_7z_copy_rejects_unknown_file_metadata failed: %d\n", rc);
    return 84 + rc;
  }
  rc = test_7z_copy_encoded_header_list();
  if (rc) {
    printf("test_7z_copy_encoded_header_list failed: %d\n", rc);
    return 255 + rc;
  }
  rc = test_7z_copy_encoded_header_rejects_bad_crc();
  if (rc) {
    printf("test_7z_copy_encoded_header_rejects_bad_crc failed: %d\n", rc);
    return 265 + rc;
  }
  rc = test_7z_encoded_header_entry_exposes_codec_method();
  if (rc) {
    printf("test_7z_encoded_header_entry_exposes_codec_method failed: %d\n",
           rc);
    return 275 + rc;
  }
  rc = test_7z_multi_coder_reports_parse_diagnostic();
  if (rc) {
    printf("test_7z_multi_coder_reports_parse_diagnostic failed: %d\n", rc);
    return 276 + rc;
  }
  rc = test_7z_multiple_stream_coder_reports_parse_diagnostic();
  if (rc) {
    printf("test_7z_multiple_stream_coder_reports_parse_diagnostic "
           "failed: %d\n", rc);
    return 277 + rc;
  }
  rc = test_7z_unsupported_method_reports_parse_diagnostic();
  if (rc) {
    printf("test_7z_unsupported_method_reports_parse_diagnostic failed: %d\n",
           rc);
    return 278 + rc;
  }
  rc = test_7z_copy_multi_substream_list();
  if (rc) {
    printf("test_7z_copy_multi_substream_list failed: %d\n", rc);
    return 310 + rc;
  }
  rc = test_7z_deflate_multi_substream_is_file_backed_split();
  if (rc) {
    printf("test_7z_deflate_multi_substream_is_file_backed_split "
           "failed: %d\n", rc);
    return 315 + rc;
  }
  rc = test_7z_real_split_fixtures_parse_and_split();
  if (rc) {
    printf("test_7z_real_split_fixtures_parse_and_split failed: %d\n",
           rc);
    return 317 + rc;
  }
  rc = test_7z_split_writer_extracts_decoded_substreams();
  if (rc) {
    printf("test_7z_split_writer_extracts_decoded_substreams failed: %d\n",
           rc);
    return 320 + rc;
  }
  rc = test_7z_split_writer_rejects_substream_crc_mismatch();
  if (rc) {
    printf("test_7z_split_writer_rejects_substream_crc_mismatch failed: %d\n",
           rc);
    return 325 + rc;
  }
  rc = test_7z_split_writer_filter_ignores_unmatched_crc();
  if (rc) {
    printf("test_7z_split_writer_filter_ignores_unmatched_crc failed: %d\n",
           rc);
    return 330 + rc;
  }
  rc = test_7z_rejects_bad_start_header_crc();
  if (rc) {
    printf("test_7z_rejects_bad_start_header_crc failed: %d\n", rc);
    return 270 + rc;
  }
  rc = test_7z_copy_test_rejects_bad_data_crc();
  if (rc) {
    printf("test_7z_copy_test_rejects_bad_data_crc failed: %d\n", rc);
    return 280 + rc;
  }
  rc = test_7z_copy_file_test_rejects_bad_data_crc();
  if (rc) {
    printf("test_7z_copy_file_test_rejects_bad_data_crc failed: %d\n", rc);
    return 290 + rc;
  }
  rc = test_7z_lzma_list_and_wrap();
  if (rc) {
    printf("test_7z_lzma_list_and_wrap failed: %d\n", rc);
    return 90 + rc;
  }
  rc = test_7z_deflate_list_and_can_handle();
  if (rc) {
    printf("test_7z_deflate_list_and_can_handle failed: %d\n", rc);
    return 335 + rc;
  }
  rc = test_7z_lzma2_list_and_wrap();
  if (rc) {
    printf("test_7z_lzma2_list_and_wrap failed: %d\n", rc);
    return 320 + rc;
  }
  rc = test_7z_lzma2_feed_output_limit_has_end_marker_headroom();
  if (rc) {
    printf("test_7z_lzma2_feed_output_limit_has_end_marker_headroom failed: %d\n",
           rc);
    return 340 + rc;
  }
  rc = test_7z_read_header_from_file();
  if (rc) {
    printf("test_7z_read_header_from_file failed: %d\n", rc);
    return 110 + rc;
  }
  rc = test_feed_file_input_chunk_uses_prefix_and_range();
  if (rc) {
    printf("test_feed_file_input_chunk_uses_prefix_and_range failed: %d\n",
           rc);
    return 120 + rc;
  }
  rc = test_write_file_range_entry();
  if (rc) {
    printf("test_write_file_range_entry failed: %d\n", rc);
    return 130 + rc;
  }
  rc = test_read_file_range();
  if (rc) {
    printf("test_read_file_range failed: %d\n", rc);
    return 150 + rc;
  }
  rc = test_lzma_props_dict_size();
  if (rc) {
    printf("test_lzma_props_dict_size failed: %d\n", rc);
    return 100 + rc;
  }
  rc = test_lzma2_prop_dict_size();
  if (rc) {
    printf("test_lzma2_prop_dict_size failed: %d\n", rc);
    return 330 + rc;
  }
  rc = test_safe_paths();
  if (rc) {
    printf("test_safe_paths failed: %d\n", rc);
    return 20 + rc;
  }
  rc = test_gzip_info();
  if (rc) {
    printf("test_gzip_info failed: %d\n", rc);
    return 30 + rc;
  }
  rc = test_gzip_filename_is_normalized();
  if (rc) {
    printf("test_gzip_filename_is_normalized failed: %d\n", rc);
    return 35 + rc;
  }
  rc = test_lzma_info();
  if (rc) {
    printf("test_lzma_info failed: %d\n", rc);
    return 60 + rc;
  }
  rc = test_zip_list();
  if (rc) {
    printf("test_zip_list failed: %d\n", rc);
    return 40 + rc;
  }
  rc = test_lha_lh0_list();
  if (rc) {
    printf("test_lha_lh0_list failed: %d\n", rc);
    return 42 + rc;
  }
  rc = test_lha_level1_lh5_list_reports_unsupported_method();
  if (rc) {
    printf("test_lha_level1_lh5_list_reports_unsupported_method "
           "failed: %d\n", rc);
    return 43 + rc;
  }
  rc = test_lha_level1_extension_name_and_dir_are_used();
  if (rc) {
    printf("test_lha_level1_extension_name_and_dir_are_used failed: %d\n",
           rc);
    return 44 + rc;
  }
  rc = test_lha_level1_lhd_and_lh0_extract();
  if (rc) {
    printf("test_lha_level1_lhd_and_lh0_extract failed: %d\n", rc);
    return 45 + rc;
  }
  rc = test_lha_extract_match_filter_skips_unmatched();
  if (rc) {
    printf("test_lha_extract_match_filter_skips_unmatched failed: %d\n", rc);
    return 49 + rc;
  }
  rc = test_lha_level2_extension_name_and_dir_are_listed();
  if (rc) {
    printf("test_lha_level2_extension_name_and_dir_are_listed failed: %d\n",
           rc);
    return 46 + rc;
  }
  rc = test_lha_lh5_docker_fixture_extracts();
  if (rc) {
    printf("test_lha_lh5_docker_fixture_extracts failed: %d\n", rc);
    return 48 + rc;
  }
  rc = test_lha_lh6_lh7_docker_fixtures_extract();
  if (rc) {
    printf("test_lha_lh6_lh7_docker_fixtures_extract failed: %d\n", rc);
    return 49 + rc;
  }
  rc = test_lha_lh1_docker_fixture_extracts();
  if (rc) {
    printf("test_lha_lh1_docker_fixture_extracts failed: %d\n", rc);
    return 50 + rc;
  }
  rc = test_lha_lh0_extract();
  if (rc) {
    printf("test_lha_lh0_extract failed: %d\n", rc);
    return 47 + rc;
  }
  rc = test_zip_backslash_names_are_normalized();
  if (rc) {
    printf("test_zip_backslash_names_are_normalized failed: %d\n", rc);
    return 45 + rc;
  }
  rc = test_zip_current_dir_prefix_names_are_normalized();
  if (rc) {
    printf("test_zip_current_dir_prefix_names_are_normalized failed: %d\n",
           rc);
    return 46 + rc;
  }
  rc = test_zip_current_dir_components_are_normalized();
  if (rc) {
    printf("test_zip_current_dir_components_are_normalized failed: %d\n",
           rc);
    return 47 + rc;
  }
  rc = test_zip_duplicate_slashes_are_normalized();
  if (rc) {
    printf("test_zip_duplicate_slashes_are_normalized failed: %d\n", rc);
    return 48 + rc;
  }
  rc = test_zip_current_dir_prefixed_directory_is_directory();
  if (rc) {
    printf("test_zip_current_dir_prefixed_directory_is_directory failed: %d\n",
           rc);
    return 47 + rc;
  }
  rc = test_zip_root_current_dir_metadata_is_skipped();
  if (rc) {
    printf("test_zip_root_current_dir_metadata_is_skipped failed: %d\n", rc);
    return 48 + rc;
  }
  rc = test_zip_root_current_dir_metadata_does_not_use_output_slot();
  if (rc) {
    printf("test_zip_root_current_dir_metadata_does_not_use_output_slot "
           "failed: %d\n", rc);
    return 49 + rc;
  }
  rc = test_zip_list_file();
  if (rc) {
    printf("test_zip_list_file failed: %d\n", rc);
    return 140 + rc;
  }
  rc = test_zip_list_file_allows_trailing_central_data();
  if (rc) {
    printf("test_zip_list_file_allows_trailing_central_data failed: %d\n",
           rc);
    return 160 + rc;
  }
  rc = test_zip64_extra_resolves_small_entry();
  if (rc) {
    printf("test_zip64_extra_resolves_small_entry failed: %d\n", rc);
    return 340 + rc;
  }
  rc = test_zip64_local_extra_resolves_small_entry();
  if (rc) {
    printf("test_zip64_local_extra_resolves_small_entry failed: %d\n", rc);
    return 350 + rc;
  }
  rc = test_zip64_offset_extra_resolves_small_entry();
  if (rc) {
    printf("test_zip64_offset_extra_resolves_small_entry failed: %d\n", rc);
    return 360 + rc;
  }
  rc = test_zip64_eocd_resolves_small_archive();
  if (rc) {
    printf("test_zip64_eocd_resolves_small_archive failed: %d\n", rc);
    return 370 + rc;
  }
  rc = test_zip_directory_external_attributes();
  if (rc) {
    printf("test_zip_directory_external_attributes failed: %d\n", rc);
    return 390 + rc;
  }
  rc = test_zip_crc_validation_helpers();
  if (rc) {
    printf("test_zip_crc_validation_helpers failed: %d\n", rc);
    return 240 + rc;
  }
  rc = test_7z_result_crc_validation_helpers();
  if (rc) {
    printf("test_7z_result_crc_validation_helpers failed: %d\n", rc);
    return 300 + rc;
  }
  rc = test_empty_zip_archive_is_valid();
  if (rc) {
    printf("test_empty_zip_archive_is_valid failed: %d\n", rc);
    return 250 + rc;
  }
  rc = test_zip_rejects_mismatched_local_header_name();
  if (rc) {
    printf("test_zip_rejects_mismatched_local_header_name failed: %d\n", rc);
    return 260 + rc;
  }
  rc = test_tar_list();
  if (rc) {
    printf("test_tar_list failed: %d\n", rc);
    return 50 + rc;
  }
  rc = test_tar_stream_extracts_split_chunks();
  if (rc) {
    printf("test_tar_stream_extracts_split_chunks failed: %d\n", rc);
    return 170 + rc;
  }
  rc = test_tar_stream_normalizes_current_dir_prefix();
  if (rc) {
    printf("test_tar_stream_normalizes_current_dir_prefix failed: %d\n", rc);
    return 180 + rc;
  }
  rc = test_tar_root_current_dir_metadata_is_skipped();
  if (rc) {
    printf("test_tar_root_current_dir_metadata_is_skipped failed: %d\n", rc);
    return 182 + rc;
  }
  rc = test_tar_current_dir_components_are_normalized();
  if (rc) {
    printf("test_tar_current_dir_components_are_normalized failed: %d\n", rc);
    return 183 + rc;
  }
  rc = test_tar_duplicate_slashes_are_normalized();
  if (rc) {
    printf("test_tar_duplicate_slashes_are_normalized failed: %d\n", rc);
    return 184 + rc;
  }
  rc = test_tar_gnu_long_root_current_dir_metadata_is_skipped();
  if (rc) {
    printf("test_tar_gnu_long_root_current_dir_metadata_is_skipped "
           "failed: %d\n", rc);
    return 185 + rc;
  }
  rc = test_tar_trailing_slash_entry_is_directory();
  if (rc) {
    printf("test_tar_trailing_slash_entry_is_directory failed: %d\n", rc);
    return 185 + rc;
  }
  rc = test_tar_gnu_long_name_applies_to_next_entry();
  if (rc) {
    printf("test_tar_gnu_long_name_applies_to_next_entry failed: %d\n", rc);
    return 190 + rc;
  }
  rc = test_tar_pax_path_applies_to_next_entry();
  if (rc) {
    printf("test_tar_pax_path_applies_to_next_entry failed: %d\n", rc);
    return 200 + rc;
  }
  rc = test_tar_pax_root_current_dir_metadata_is_skipped();
  if (rc) {
    printf("test_tar_pax_root_current_dir_metadata_is_skipped failed: %d\n",
           rc);
    return 205 + rc;
  }
  rc = test_tar_pax_size_applies_to_next_entry();
  if (rc) {
    printf("test_tar_pax_size_applies_to_next_entry failed: %d\n", rc);
    return 380 + rc;
  }
  rc = test_tar_stream_accepts_large_pax_header();
  if (rc) {
    printf("test_tar_stream_accepts_large_pax_header failed: %d\n", rc);
    return 385 + rc;
  }
  rc = test_tar_base256_size_is_accepted();
  if (rc) {
    printf("test_tar_base256_size_is_accepted failed: %d\n", rc);
    return 400 + rc;
  }
  rc = test_tar_signed_checksum_is_accepted();
  if (rc) {
    printf("test_tar_signed_checksum_is_accepted failed: %d\n", rc);
    return 410 + rc;
  }
  rc = test_tar_skips_unsupported_special_entries();
  if (rc) {
    printf("test_tar_skips_unsupported_special_entries failed: %d\n", rc);
    return 210 + rc;
  }
  rc = test_empty_tar_archive_is_valid();
  if (rc) {
    printf("test_empty_tar_archive_is_valid failed: %d\n", rc);
    return 220 + rc;
  }
  rc = test_tar_rejects_bad_header_checksum();
  if (rc) {
    printf("test_tar_rejects_bad_header_checksum failed: %d\n", rc);
    return 230 + rc;
  }
  rc = test_7z_split_group_rejects_discontinuous_offsets();
  if (rc) {
    printf("test_7z_split_group_rejects_discontinuous_offsets failed: %d\n",
           rc);
    return 415 + rc;
  }
  rc = test_7z_split_writer_handles_zero_length_substreams();
  if (rc) {
    printf("test_7z_split_writer_handles_zero_length_substreams failed: %d\n",
           rc);
    return 420 + rc;
  }
  rc = test_lha_method_to_compression_mapper();
  if (rc) {
    printf("test_lha_method_to_compression_mapper failed: %d\n", rc);
    return 430 + rc;
  }
  return 0;
}
