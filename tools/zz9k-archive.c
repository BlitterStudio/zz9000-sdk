/*
 * ZZ9000 SDK archive frontend.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k/caps.h"
#include "zz9k/compression.h"
#include "zz9k/host.h"
#include "zz9k/shared.h"
#include "zz9k/text.h"
#include "lha-unix/zz9k_lha_unix.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <sys/stat.h>
#elif defined(__amigaos__)
#include <proto/dos.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#define ZZ9K_ARCHIVE_MAX_NAME 256U
#define ZZ9K_ARCHIVE_MAX_PAX_DATA 65536U
#define ZZ9K_ARCHIVE_TAR_METHOD_STORE 0U
#define ZZ9K_ARCHIVE_TAR_FLAG_SKIP 1U
#define ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME 2U
#define ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER 4U
#define ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32 0x80000000UL
#define ZZ9K_ARCHIVE_ENTRY_FLAG_7Z_UNSUPPORTED_SPLIT 0x40000000UL
#define ZZ9K_ARCHIVE_ZIP_METHOD_STORE 0U
#define ZZ9K_ARCHIVE_ZIP_METHOD_DEFLATE 8U
#define ZZ9K_ARCHIVE_ZIP_EXTRA_ZIP64 0x0001U
#define ZZ9K_ARCHIVE_ZIP_U32_SENTINEL 0xffffffffUL
#define ZZ9K_ARCHIVE_ZIP_SUPPORTED_U32_MAX 0x7fffffffUL
#define ZZ9K_ARCHIVE_LHA_METHOD_LH0 0U
#define ZZ9K_ARCHIVE_LHA_METHOD_LH1 1U
#define ZZ9K_ARCHIVE_LHA_METHOD_LH5 5U
#define ZZ9K_ARCHIVE_LHA_METHOD_LH6 6U
#define ZZ9K_ARCHIVE_LHA_METHOD_LH7 7U
#define ZZ9K_ARCHIVE_LHA_METHOD_LHD 11U
#define ZZ9K_ARCHIVE_7Z_METHOD_COPY 0U
#define ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE 0x040108UL
#define ZZ9K_ARCHIVE_7Z_METHOD_LZMA 0x030101UL
#define ZZ9K_ARCHIVE_7Z_METHOD_LZMA2 0x21U
#define ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS 16U
#define ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE 32U
#define ZZ9K_ARCHIVE_7Z_ID_END 0x00U
#define ZZ9K_ARCHIVE_7Z_ID_HEADER 0x01U
#define ZZ9K_ARCHIVE_7Z_ID_ARCHIVE_PROPERTIES 0x02U
#define ZZ9K_ARCHIVE_7Z_ID_ADDITIONAL_STREAMS_INFO 0x03U
#define ZZ9K_ARCHIVE_7Z_ID_MAIN_STREAMS_INFO 0x04U
#define ZZ9K_ARCHIVE_7Z_ID_FILES_INFO 0x05U
#define ZZ9K_ARCHIVE_7Z_ID_PACK_INFO 0x06U
#define ZZ9K_ARCHIVE_7Z_ID_UNPACK_INFO 0x07U
#define ZZ9K_ARCHIVE_7Z_ID_SUBSTREAMS_INFO 0x08U
#define ZZ9K_ARCHIVE_7Z_ID_SIZE 0x09U
#define ZZ9K_ARCHIVE_7Z_ID_CRC 0x0aU
#define ZZ9K_ARCHIVE_7Z_ID_FOLDER 0x0bU
#define ZZ9K_ARCHIVE_7Z_ID_CODERS_UNPACK_SIZE 0x0cU
#define ZZ9K_ARCHIVE_7Z_ID_NUM_UNPACK_STREAM 0x0dU
#define ZZ9K_ARCHIVE_7Z_ID_EMPTY_STREAM 0x0eU
#define ZZ9K_ARCHIVE_7Z_ID_EMPTY_FILE 0x0fU
#define ZZ9K_ARCHIVE_7Z_ID_ANTI 0x10U
#define ZZ9K_ARCHIVE_7Z_ID_NAME 0x11U
#define ZZ9K_ARCHIVE_7Z_ID_CREATION_TIME 0x12U
#define ZZ9K_ARCHIVE_7Z_ID_LAST_ACCESS_TIME 0x13U
#define ZZ9K_ARCHIVE_7Z_ID_LAST_WRITE_TIME 0x14U
#define ZZ9K_ARCHIVE_7Z_ID_WIN_ATTRIBUTES 0x15U
#define ZZ9K_ARCHIVE_7Z_ID_COMMENT 0x16U
#define ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER 0x17U
#define ZZ9K_ARCHIVE_7Z_ID_START_POS 0x18U
#define ZZ9K_ARCHIVE_7Z_ID_DUMMY 0x19U
#define ZZ9K_ARCHIVE_PROBE_BYTES 512U
#define ZZ9K_ARCHIVE_STREAM_CHUNK 32768U
#define ZZ9K_ARCHIVE_STREAM_MIN_CHUNK 4096U
#define ZZ9K_ARCHIVE_TAR_BLOCK 512U

static int zz9k_archive_overwrite_outputs = 0;
static int zz9k_archive_skip_existing_outputs = 0;
static int zz9k_archive_dry_run_outputs = 0;
static int zz9k_archive_last_output_skipped = 0;
static int zz9k_archive_last_output_dry_run = 0;
static const char *zz9k_archive_match_filter = 0;
static uint32_t zz9k_archive_strip_components = 0U;

typedef enum ZZ9KArchiveFormat {
  ZZ9K_ARCHIVE_FORMAT_UNKNOWN = 0,
  ZZ9K_ARCHIVE_FORMAT_GZIP,
  ZZ9K_ARCHIVE_FORMAT_ZIP,
  ZZ9K_ARCHIVE_FORMAT_TAR,
  ZZ9K_ARCHIVE_FORMAT_7Z,
  ZZ9K_ARCHIVE_FORMAT_LZMA_ALONE,
  ZZ9K_ARCHIVE_FORMAT_LHA
} ZZ9KArchiveFormat;

typedef struct ZZ9KArchiveEntry {
  char name[ZZ9K_ARCHIVE_MAX_NAME];
  uint32_t method;
  uint32_t flags;
  uint32_t crc32;
  uint32_t data_offset;
  uint32_t decoded_offset;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint32_t is_dir;
  uint32_t method_props_size;
  uint8_t method_props[ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS];
} ZZ9KArchiveEntry;

typedef struct ZZ9KArchiveGzipInfo {
  char name[ZZ9K_ARCHIVE_MAX_NAME];
  uint32_t crc32;
  uint32_t compressed_offset;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
} ZZ9KArchiveGzipInfo;

typedef struct ZZ9KArchiveLzmaInfo {
  char name[ZZ9K_ARCHIVE_MAX_NAME];
  uint32_t compressed_offset;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  int size_known;
} ZZ9KArchiveLzmaInfo;

typedef struct ZZ9KArchive7zHeader {
  uint32_t next_header_offset;
  uint32_t next_header_size;
  uint32_t next_header_crc;
} ZZ9KArchive7zHeader;

typedef struct ZZ9KArchive7zCursor {
  const uint8_t *data;
  uint32_t size;
  uint32_t pos;
} ZZ9KArchive7zCursor;

typedef struct ZZ9KArchive7zStreams {
  uint32_t count;
  uint32_t *data_offsets;
  uint32_t *decoded_offsets;
  uint32_t *pack_sizes;
  uint32_t *unpack_sizes;
  uint32_t *crcs;
  uint32_t *methods;
  uint32_t *flags;
  uint8_t *crc_defined;
  uint8_t *props_sizes;
  uint8_t *props;
} ZZ9KArchive7zStreams;

typedef int (*ZZ9KArchiveDecodedChunkFn)(void *user,
                                         const uint8_t *data,
                                         uint32_t length);

typedef struct ZZ9KArchiveTarStream {
  const char *command;
  const char *output_dir;
  uint8_t header[ZZ9K_ARCHIVE_TAR_BLOCK];
  ZZ9KArchiveEntry entry;
  FILE *file;
  char pending_name[ZZ9K_ARCHIVE_MAX_NAME];
  uint8_t *pax_data;
  uint32_t pax_capacity;
  uint32_t pending_size;
  uint32_t header_used;
  uint32_t entry_remaining;
  uint32_t padding_remaining;
  uint32_t pax_used;
  uint32_t count;
  int pending_size_valid;
  int pending_name_skip;
  int ok;
  int done;
} ZZ9KArchiveTarStream;

typedef struct ZZ9KArchiveTarPaxInfo {
  char path[ZZ9K_ARCHIVE_MAX_NAME];
  uint32_t size;
  int has_path;
  int has_size;
  int path_skip;
} ZZ9KArchiveTarPaxInfo;

static int zz9k_archive_tar_header_checksum_valid(const uint8_t *header);
static int zz9k_archive_tar_header_empty(const uint8_t *header);
static int zz9k_archive_open_output_entry(const char *output_dir,
                                          const ZZ9KArchiveEntry *entry,
                                          FILE **file);

static uint16_t zz9k_archive_get_le16(const uint8_t *p)
{
  return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t zz9k_archive_get_le32(const uint8_t *p)
{
  return (uint32_t)p[0] |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static uint64_t zz9k_archive_get_le64(const uint8_t *p)
{
  uint32_t lo;
  uint32_t hi;

  lo = zz9k_archive_get_le32(p);
  hi = zz9k_archive_get_le32(p + 4U);
  return (uint64_t)lo | ((uint64_t)hi << 32);
}

static uint32_t zz9k_archive_crc32(uint32_t crc,
                                   const uint8_t *data,
                                   uint32_t length)
{
  uint32_t i;

  if (!data && length != 0U) {
    return 0U;
  }
  crc ^= 0xffffffffUL;
  for (i = 0U; i < length; i++) {
    uint32_t bit;

    crc ^= data[i];
    for (bit = 0U; bit < 8U; bit++) {
      crc = (crc & 1U) != 0U ? (crc >> 1) ^ 0xedb88320UL : crc >> 1;
    }
  }
  return crc ^ 0xffffffffUL;
}

static int zz9k_archive_copy_name(char *dst, uint32_t dst_capacity,
                                  const uint8_t *src, uint32_t length)
{
  if (!dst || !src || dst_capacity == 0U ||
      length == 0U || length >= dst_capacity) {
    return 0;
  }
  memcpy(dst, src, length);
  dst[length] = '\0';
  return 1;
}

static void zz9k_archive_strip_current_dir_prefix(char *path)
{
  char *p;

  if (!path) {
    return;
  }
  while (path[0] == '.' && path[1] == '/' && path[2] != '\0') {
    memmove(path, path + 2, strlen(path + 2) + 1U);
  }
  p = path;
  while (*p != '\0') {
    if (p[0] == '/' && p[1] == '/') {
      memmove(p + 1, p + 2, strlen(p + 2) + 1U);
      continue;
    }
    if (p[0] == '/' && p[1] == '.' && p[2] == '/') {
      memmove(p + 1, p + 3, strlen(p + 3) + 1U);
      continue;
    }
    p++;
  }
}

static int zz9k_archive_copy_zip_name(char *dst, uint32_t dst_capacity,
                                      const uint8_t *src, uint32_t length)
{
  uint32_t i;

  if (!zz9k_archive_copy_name(dst, dst_capacity, src, length)) {
    return 0;
  }
  for (i = 0U; dst[i] != '\0'; i++) {
    if (dst[i] == '\\') {
      dst[i] = '/';
    }
  }
  zz9k_archive_strip_current_dir_prefix(dst);
  return 1;
}

static int zz9k_archive_copy_gzip_name(char *dst, uint32_t dst_capacity,
                                       const uint8_t *src, uint32_t length)
{
  uint32_t i;

  if (!zz9k_archive_copy_name(dst, dst_capacity, src, length)) {
    return 0;
  }
  for (i = 0U; dst[i] != '\0'; i++) {
    if (dst[i] == '\\') {
      dst[i] = '/';
    }
  }
  zz9k_archive_strip_current_dir_prefix(dst);
  return 1;
}

static int zz9k_archive_copy_lha_name(char *dst, uint32_t dst_capacity,
                                      const uint8_t *src, uint32_t length)
{
  uint32_t i;

  if (!zz9k_archive_copy_gzip_name(dst, dst_capacity, src, length)) {
    return 0;
  }
  for (i = 0U; dst[i] != '\0'; i++) {
    if ((uint8_t)dst[i] == 0xffU) {
      dst[i] = '/';
    }
  }
  zz9k_archive_strip_current_dir_prefix(dst);
  return 1;
}

static int zz9k_archive_lha_join_dir_name(char *dst,
                                          uint32_t dst_capacity,
                                          const char *dir,
                                          const char *name)
{
  size_t dir_len;
  size_t name_len;
  int need_sep;
  char name_copy[ZZ9K_ARCHIVE_MAX_NAME];

  if (!dst || !name || dst_capacity == 0U) {
    return 0;
  }
  if (!dir) {
    dir = "";
  }
  dir_len = strlen(dir);
  name_len = strlen(name);
  need_sep = dir_len != 0U && dir[dir_len - 1U] != '/';
  if (name_len >= sizeof(name_copy) ||
      dir_len + (need_sep ? 1U : 0U) + name_len >= dst_capacity) {
    return 0;
  }
  strcpy(name_copy, name);
  dst[0] = '\0';
  strcat(dst, dir);
  if (need_sep) {
    strcat(dst, "/");
  }
  strcat(dst, name_copy);
  zz9k_archive_strip_current_dir_prefix(dst);
  return 1;
}

static int zz9k_archive_name_ends_with_slash(const char *name)
{
  uint32_t length;

  if (!name) {
    return 0;
  }
  length = (uint32_t)strlen(name);
  return length > 0U && name[length - 1U] == '/';
}

static void zz9k_archive_trim_trailing_separators(char *path)
{
  uint32_t length;

  if (!path) {
    return;
  }
  length = (uint32_t)strlen(path);
  while (length > 0U &&
         (path[length - 1U] == '/' || path[length - 1U] == '\\')) {
    path[--length] = '\0';
  }
}

static int zz9k_archive_entry_matches_filter(const ZZ9KArchiveEntry *entry)
{
  if (!zz9k_archive_match_filter || zz9k_archive_match_filter[0] == '\0') {
    return 1;
  }
  return entry && strstr(entry->name, zz9k_archive_match_filter) != 0;
}

static const char *zz9k_archive_strip_entry_name(const char *name)
{
  uint32_t stripped = 0U;
  const char *p = name;

  if (!name || zz9k_archive_strip_components == 0U) {
    return name;
  }
  while (*p != '\0' && stripped < zz9k_archive_strip_components) {
    while (*p == '/') {
      p++;
    }
    while (*p != '\0' && *p != '/') {
      p++;
    }
    if (*p == '/') {
      p++;
      stripped++;
    } else {
      stripped++;
      break;
    }
  }
  while (*p == '/') {
    p++;
  }
  return stripped >= zz9k_archive_strip_components ? p : "";
}

static int zz9k_archive_output_entry(const ZZ9KArchiveEntry *entry,
                                     ZZ9KArchiveEntry *output_entry)
{
  const char *name;

  if (!entry || !output_entry) {
    return 0;
  }
  *output_entry = *entry;
  name = zz9k_archive_strip_entry_name(entry->name);
  if (!name || name[0] == '\0') {
    return 0;
  }
  if (strlen(name) >= sizeof(output_entry->name)) {
    return 0;
  }
  strcpy(output_entry->name, name);
  output_entry->is_dir = zz9k_archive_name_ends_with_slash(output_entry->name);
  return 1;
}

static int zz9k_archive_zip_entry_is_root_metadata(
    const ZZ9KArchiveEntry *entry)
{
  if (!entry || !entry->is_dir ||
      entry->method != ZZ9K_ARCHIVE_ZIP_METHOD_STORE ||
      entry->compressed_size != 0U || entry->uncompressed_size != 0U) {
    return 0;
  }
  return strcmp(entry->name, ".") == 0 || strcmp(entry->name, "./") == 0;
}

static int zz9k_archive_7z_entry_is_root_metadata(
    const ZZ9KArchiveEntry *entry)
{
  if (!entry || !entry->is_dir ||
      entry->method != ZZ9K_ARCHIVE_7Z_METHOD_COPY ||
      entry->compressed_size != 0U || entry->uncompressed_size != 0U) {
    return 0;
  }
  return strcmp(entry->name, ".") == 0 || strcmp(entry->name, "./") == 0;
}

static int zz9k_archive_path_is_safe(const char *path)
{
  const char *component;
  const char *p;
  uint32_t component_len;

  if (!path || path[0] == '\0' ||
      path[0] == '/' || path[0] == '\\') {
    return 0;
  }

  component = path;
  p = path;
  while (1) {
    if (*p == ':' || *p == '\\') {
      return 0;
    }
    if (*p == '/' || *p == '\0') {
      component_len = (uint32_t)(p - component);
      if (component_len == 0U) {
        if (*p == '\0' && p != path) {
          return 1;
        }
        return 0;
      }
      if ((component_len == 1U && component[0] == '.') ||
          (component_len == 2U && component[0] == '.' &&
           component[1] == '.')) {
        return 0;
      }
      if (*p == '\0') {
        return 1;
      }
      component = p + 1;
    }
    p++;
  }
}

static int zz9k_archive_detect_tar(const uint8_t *data, uint32_t length)
{
  if (!data || length < ZZ9K_ARCHIVE_TAR_BLOCK) {
    return 0;
  }
  if (length >= ZZ9K_ARCHIVE_TAR_BLOCK * 2U &&
      zz9k_archive_tar_header_empty(data) &&
      zz9k_archive_tar_header_empty(data + ZZ9K_ARCHIVE_TAR_BLOCK)) {
    return 1;
  }
  if (data[0] == 0U) {
    return 0;
  }
  return zz9k_archive_tar_header_checksum_valid(data);
}

static int zz9k_archive_lha_header_checksum_valid(const uint8_t *data,
                                                  uint32_t length)
{
  uint32_t header_size;
  uint32_t i;
  uint8_t checksum = 0U;

  if (!data || length < 24U) {
    return 0;
  }
  header_size = data[0];
  if (header_size == 0U) {
    return 0;
  }
  if (length >= 21U && data[20] == 2U) {
    header_size = zz9k_archive_get_le16(data);
    return header_size >= 26U && header_size <= length;
  }
  if (header_size + 2U > length) {
    return 0;
  }
  for (i = 2U; i < 2U + header_size; i++) {
    checksum = (uint8_t)(checksum + data[i]);
  }
  return checksum == data[1];
}

static int zz9k_archive_detect_lha(const uint8_t *data, uint32_t length)
{
  uint32_t header_size;
  uint8_t level;

  if (!zz9k_archive_lha_header_checksum_valid(data, length)) {
    return 0;
  }
  header_size = data[20] == 2U ? zz9k_archive_get_le16(data) : data[0];
  if (data[20] == 2U) {
    if (header_size < 26U ||
        data[2] != '-' || data[6] != '-' ||
        data[3] != 'l' ||
        (data[4] != 'h' && data[4] != 'z')) {
      return 0;
    }
  } else if (header_size < 22U ||
             data[2] != '-' || data[6] != '-' ||
             data[3] != 'l' ||
             (data[4] != 'h' && data[4] != 'z')) {
    return 0;
  }
  level = data[20];
  if (level > 2U) {
    return 0;
  }
  if (level == 0U && data[21] > header_size - 22U) {
    return 0;
  }
  return 1;
}

static ZZ9KArchiveFormat zz9k_archive_detect_format(const uint8_t *data,
                                                    uint32_t length)
{
  if (!data || length < 4U) {
    return ZZ9K_ARCHIVE_FORMAT_UNKNOWN;
  }
  if (length >= 6U &&
      data[0] == 0x37U && data[1] == 0x7aU &&
      data[2] == 0xbcU && data[3] == 0xafU &&
      data[4] == 0x27U && data[5] == 0x1cU) {
    return ZZ9K_ARCHIVE_FORMAT_7Z;
  }
  if (length >= 10U &&
      data[0] == 0x1fU && data[1] == 0x8bU && data[2] == 0x08U) {
    return ZZ9K_ARCHIVE_FORMAT_GZIP;
  }
  if (length >= 4U &&
      (zz9k_archive_get_le32(data) == 0x04034b50UL ||
       zz9k_archive_get_le32(data) == 0x06054b50UL)) {
    return ZZ9K_ARCHIVE_FORMAT_ZIP;
  }
  if (length >= 512U &&
      memcmp(data + 257U, "ustar", 5U) == 0) {
    return ZZ9K_ARCHIVE_FORMAT_TAR;
  }
  if (zz9k_archive_detect_tar(data, length)) {
    return ZZ9K_ARCHIVE_FORMAT_TAR;
  }
  if (zz9k_archive_detect_lha(data, length)) {
    return ZZ9K_ARCHIVE_FORMAT_LHA;
  }
  if (length >= 14U && data[0] < (9U * 5U * 5U) &&
      zz9k_archive_get_le32(data + 1U) >= 4096U) {
    return ZZ9K_ARCHIVE_FORMAT_LZMA_ALONE;
  }
  return ZZ9K_ARCHIVE_FORMAT_UNKNOWN;
}

static const char *zz9k_archive_format_name(ZZ9KArchiveFormat format)
{
  switch (format) {
    case ZZ9K_ARCHIVE_FORMAT_GZIP:
      return "gzip";
    case ZZ9K_ARCHIVE_FORMAT_ZIP:
      return "zip";
    case ZZ9K_ARCHIVE_FORMAT_TAR:
      return "tar";
    case ZZ9K_ARCHIVE_FORMAT_7Z:
      return "7z";
    case ZZ9K_ARCHIVE_FORMAT_LZMA_ALONE:
      return "lzma-alone";
    case ZZ9K_ARCHIVE_FORMAT_LHA:
      return "lha";
    default:
      return "unknown";
  }
}

static int zz9k_archive_read_file(const char *path, uint8_t **data,
                                  uint32_t *length)
{
  FILE *file;
  long size;
  uint8_t *bytes;

  file = fopen(path, "rb");
  if (!file) {
    printf("open failed: %s\n", path);
    return 0;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    printf("seek failed: %s\n", path);
    return 0;
  }
  size = ftell(file);
  if (size < 0 || size > 0x7fffffffL) {
    fclose(file);
    printf("unsupported file size: %s\n", path);
    return 0;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    printf("seek failed: %s\n", path);
    return 0;
  }
  bytes = 0;
  if (size > 0) {
    bytes = (uint8_t *)malloc((size_t)size);
    if (!bytes) {
      fclose(file);
      printf("input allocation failed\n");
      return 0;
    }
    if (fread(bytes, 1U, (size_t)size, file) != (size_t)size) {
      free(bytes);
      fclose(file);
      printf("read failed: %s\n", path);
      return 0;
    }
  }
  fclose(file);
  *data = bytes;
  *length = (uint32_t)size;
  return 1;
}

static int zz9k_archive_read_file_range(const char *path,
                                        uint32_t offset,
                                        uint32_t length,
                                        uint8_t **data)
{
  FILE *file;
  uint8_t *bytes = 0;

  if (!path || !data || offset > 0x7fffffffUL) {
    return 0;
  }
  *data = 0;
  if (length != 0U) {
    bytes = (uint8_t *)malloc((size_t)length);
    if (!bytes) {
      printf("file range allocation failed: %lu bytes\n",
             (unsigned long)length);
      return 0;
    }
  }
  file = fopen(path, "rb");
  if (!file) {
    free(bytes);
    printf("open failed: %s\n", path);
    return 0;
  }
  if (fseek(file, (long)offset, SEEK_SET) != 0 ||
      (length != 0U &&
       fread(bytes, 1U, (size_t)length, file) != (size_t)length)) {
    free(bytes);
    fclose(file);
    printf("file range read failed: %s\n", path);
    return 0;
  }
  fclose(file);
  *data = bytes;
  return 1;
}

static int zz9k_archive_file_range_crc32(const char *path,
                                         uint32_t offset,
                                         uint32_t length,
                                         uint32_t *crc32)
{
  FILE *file;
  uint8_t *chunk;
  uint32_t remaining = length;
  uint32_t crc = 0U;
  int ok = 0;

  if (!path || !crc32 || offset > 0x7fffffffUL) {
    return 0;
  }
  chunk = (uint8_t *)malloc(ZZ9K_ARCHIVE_STREAM_CHUNK);
  if (!chunk) {
    printf("crc buffer allocation failed\n");
    return 0;
  }
  file = fopen(path, "rb");
  if (!file) {
    printf("open failed: %s\n", path);
    goto out;
  }
  if (fseek(file, (long)offset, SEEK_SET) != 0) {
    printf("file range seek failed: %s\n", path);
    goto out;
  }
  while (remaining != 0U) {
    uint32_t part = remaining > ZZ9K_ARCHIVE_STREAM_CHUNK ?
        ZZ9K_ARCHIVE_STREAM_CHUNK : remaining;

    if (fread(chunk, 1U, part, file) != part) {
      printf("file range read failed: %s\n", path);
      goto out;
    }
    crc = zz9k_archive_crc32(crc, chunk, part);
    remaining -= part;
  }
  *crc32 = crc;
  ok = 1;

out:
  if (file) {
    fclose(file);
  }
  free(chunk);
  return ok;
}

static int zz9k_archive_probe_file(const char *path,
                                   uint8_t *data,
                                   uint32_t capacity,
                                   uint32_t *length,
                                   uint32_t *file_length)
{
  FILE *file;
  long size;
  uint32_t read_length;

  if (!path || !data || capacity == 0U || !length || !file_length) {
    return 0;
  }

  file = fopen(path, "rb");
  if (!file) {
    printf("open failed: %s\n", path);
    return 0;
  }
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    printf("seek failed: %s\n", path);
    return 0;
  }
  size = ftell(file);
  if (size < 0 || size > 0x7fffffffL) {
    fclose(file);
    printf("unsupported file size: %s\n", path);
    return 0;
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    printf("seek failed: %s\n", path);
    return 0;
  }

  read_length = (uint32_t)size;
  if (read_length > capacity) {
    read_length = capacity;
  }
  if (read_length != 0U &&
      fread(data, 1U, (size_t)read_length, file) !=
          (size_t)read_length) {
    fclose(file);
    printf("read failed: %s\n", path);
    return 0;
  }

  fclose(file);
  *length = read_length;
  *file_length = (uint32_t)size;
  return 1;
}

static int zz9k_archive_path_exists(const char *path)
{
  if (!path || path[0] == '\0') {
    return 0;
  }
#if defined(__amigaos__)
  {
    BPTR lock = Lock((CONST_STRPTR)path, ACCESS_READ);
    if (lock) {
      UnLock(lock);
      return 1;
    }
    return 0;
  }
#else
  {
    struct stat st;
    return stat(path, &st) == 0;
  }
#endif
}

static int zz9k_archive_path_is_dir(const char *path)
{
  if (!path || path[0] == '\0') {
    return 0;
  }
#if defined(__amigaos__)
  {
    BPTR lock;
    struct FileInfoBlock *fib;
    int ok = 0;

    lock = Lock((CONST_STRPTR)path, ACCESS_READ);
    if (!lock) {
      return 0;
    }
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, 0);
    if (fib) {
      ok = Examine(lock, fib) && fib->fib_DirEntryType > 0;
      FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return ok;
  }
#else
  {
    struct stat st;
    if (stat(path, &st) != 0) {
      return 0;
    }
#if defined(_WIN32)
    return (st.st_mode & _S_IFDIR) != 0;
#else
    return S_ISDIR(st.st_mode);
#endif
  }
#endif
}

static int zz9k_archive_write_file(const char *path, const uint8_t *data,
                                   uint32_t length)
{
  FILE *file;

  zz9k_archive_last_output_skipped = 0;
  zz9k_archive_last_output_dry_run = 0;
  if (zz9k_archive_path_is_dir(path)) {
    printf("output path is a directory: %s\n", path);
    return 0;
  }
  if (zz9k_archive_skip_existing_outputs && zz9k_archive_path_exists(path)) {
    printf("s %s\n", path);
    zz9k_archive_last_output_skipped = 1;
    return 1;
  }
  if (!zz9k_archive_overwrite_outputs && zz9k_archive_path_exists(path)) {
    printf("output exists, use --overwrite: %s\n", path);
    return 0;
  }
  if (zz9k_archive_dry_run_outputs) {
    printf("dry %s\n", path);
    zz9k_archive_last_output_dry_run = 1;
    return 1;
  }
  file = fopen(path, "wb");
  if (!file) {
    printf("open output failed: %s\n", path);
    return 0;
  }
  if (length != 0U && fwrite(data, 1U, length, file) != length) {
    fclose(file);
    printf("write failed: %s\n", path);
    return 0;
  }
  fclose(file);
  return 1;
}

static FILE *zz9k_archive_open_discard_file(void)
{
#if defined(__amigaos__)
  return fopen("NIL:", "wb");
#elif defined(_WIN32)
  return fopen("NUL", "wb");
#else
  return fopen("/dev/null", "wb");
#endif
}

static int zz9k_archive_mkdir_one(const char *path)
{
  if (!path || path[0] == '\0') {
    return 1;
  }
#if defined(__amigaos__)
  {
    BPTR lock;

    lock = CreateDir((CONST_STRPTR)path);
    if (lock) {
      UnLock(lock);
      return 1;
    }
    return zz9k_archive_path_is_dir(path);
  }
#elif defined(_WIN32)
  if (_mkdir(path) == 0) {
    return 1;
  }
  if (errno == EEXIST) {
    return zz9k_archive_path_is_dir(path);
  }
  return 0;
#else
  if (mkdir(path, 0777) == 0) {
    return 1;
  }
  if (errno == EEXIST) {
    return zz9k_archive_path_is_dir(path);
  }
  return 0;
#endif
}

static int zz9k_archive_ascii_lower(int ch)
{
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A' + 'a';
  }
  return ch;
}

static int zz9k_archive_has_suffix_ci(const char *text, const char *suffix)
{
  size_t text_len;
  size_t suffix_len;
  size_t i;

  if (!text || !suffix) {
    return 0;
  }
  text_len = strlen(text);
  suffix_len = strlen(suffix);
  if (suffix_len > text_len) {
    return 0;
  }
  text += text_len - suffix_len;
  for (i = 0U; i < suffix_len; i++) {
    if (zz9k_archive_ascii_lower((unsigned char)text[i]) !=
        zz9k_archive_ascii_lower((unsigned char)suffix[i])) {
      return 0;
    }
  }
  return 1;
}

static char *zz9k_archive_join_path(const char *base, const char *name)
{
  size_t base_len;
  size_t name_len;
  int need_sep;
  char *path;

  if (!name) {
    return 0;
  }
  if (!base || base[0] == '\0') {
    base = "";
  }
  base_len = strlen(base);
  name_len = strlen(name);
  need_sep = (base_len != 0U &&
              base[base_len - 1U] != '/' &&
              base[base_len - 1U] != '\\' &&
              base[base_len - 1U] != ':');
  path = (char *)malloc(base_len + (need_sep ? 1U : 0U) + name_len + 1U);
  if (!path) {
    return 0;
  }
  path[0] = '\0';
  strcat(path, base);
  if (need_sep) {
    strcat(path, "/");
  }
  strcat(path, name);
  return path;
}

static int zz9k_archive_ensure_parent_dirs(const char *base,
                                           const char *name)
{
  char *prefix;
  char *slash;
  char saved;
  int ok = 1;

  prefix = zz9k_archive_join_path(base, name);
  if (!prefix) {
    return 0;
  }

  slash = prefix;
  while (*slash != '\0') {
    if (*slash == '/' || *slash == '\\') {
      saved = *slash;
      *slash = '\0';
      if (prefix[0] != '\0' &&
          prefix[strlen(prefix) - 1U] != ':' &&
          !zz9k_archive_mkdir_one(prefix)) {
        ok = 0;
        *slash = saved;
        break;
      }
      *slash = saved;
    }
    slash++;
  }

  free(prefix);
  return ok;
}

static int zz9k_archive_gzip_header_info(
    const uint8_t *data,
    uint32_t length,
    ZZ9KArchiveGzipInfo *info,
    uint32_t *header_end)
{
  uint32_t pos;
  uint8_t flags;
  uint32_t name_start;

  if (!data || !info || !header_end || length < 10U ||
      data[0] != 0x1fU || data[1] != 0x8bU || data[2] != 0x08U) {
    return 0;
  }

  memset(info, 0, sizeof(*info));
  flags = data[3];
  pos = 10U;

  if ((flags & 0xe0U) != 0U) {
    return 0;
  }
  if ((flags & 0x04U) != 0U) {
    uint32_t extra_len;
    if (pos + 2U > length) {
      return 0;
    }
    extra_len = zz9k_archive_get_le16(data + pos);
    pos += 2U;
    if (extra_len > length || pos + extra_len > length) {
      return 0;
    }
    pos += extra_len;
  }
  if ((flags & 0x08U) != 0U) {
    name_start = pos;
    while (pos < length && data[pos] != 0U) {
      pos++;
    }
    if (pos >= length ||
        !zz9k_archive_copy_gzip_name(info->name, sizeof(info->name),
                                     data + name_start, pos - name_start)) {
      return 0;
    }
    pos++;
  }
  if ((flags & 0x10U) != 0U) {
    while (pos < length && data[pos] != 0U) {
      pos++;
    }
    if (pos >= length) {
      return 0;
    }
    pos++;
  }
  if ((flags & 0x02U) != 0U) {
    uint32_t actual;
    uint32_t expected;

    if (pos + 2U > length) {
      return 0;
    }
    actual = zz9k_archive_crc32(0U, data, pos) & 0xffffU;
    expected = zz9k_archive_get_le16(data + pos);
    if (actual != expected) {
      return 0;
    }
    pos += 2U;
  }
  if (info->name[0] == '\0') {
    strcpy(info->name, "output");
  }
  info->compressed_offset = 0U;
  *header_end = pos;
  return 1;
}

static int zz9k_archive_gzip_info(const uint8_t *data, uint32_t length,
                                  ZZ9KArchiveGzipInfo *info)
{
  uint32_t header_end;

  if (!zz9k_archive_gzip_header_info(data, length, info, &header_end) ||
      header_end + 8U > length) {
    return 0;
  }
  info->compressed_size = length;
  info->crc32 = zz9k_archive_get_le32(data + length - 8U);
  info->uncompressed_size = zz9k_archive_get_le32(data + length - 4U);
  return 1;
}

static int zz9k_archive_gzip_info_from_file(
    const char *path,
    const uint8_t *header,
    uint32_t header_length,
    uint32_t file_length,
    ZZ9KArchiveGzipInfo *info)
{
  uint8_t *tail = 0;
  uint32_t header_end;
  int ok = 0;

  if (!path || !info || file_length < 18U ||
      !zz9k_archive_gzip_header_info(
          header, header_length, info, &header_end) ||
      header_end + 8U > file_length) {
    return 0;
  }
  if (!zz9k_archive_read_file_range(path, file_length - 8U, 8U, &tail)) {
    return 0;
  }
  info->compressed_size = file_length;
  info->crc32 = zz9k_archive_get_le32(tail);
  info->uncompressed_size = zz9k_archive_get_le32(tail + 4U);
  ok = 1;

  free(tail);
  return ok;
}

static int zz9k_archive_gzip_result_matches_footer(
    const ZZ9KArchiveGzipInfo *info,
    const ZZ9KDecompressResult *result)
{
  if (!info || !result) {
    return 0;
  }
  return result->bytes_written == info->uncompressed_size &&
         result->checksum == info->crc32;
}

static int zz9k_archive_gzip_is_tar_candidate(
    const char *archive_path,
    const ZZ9KArchiveGzipInfo *info)
{
  return (info && zz9k_archive_has_suffix_ci(info->name, ".tar")) ||
         zz9k_archive_has_suffix_ci(archive_path, ".tar.gz") ||
         zz9k_archive_has_suffix_ci(archive_path, ".tgz");
}

static int zz9k_archive_lzma_info(const uint8_t *data, uint32_t length,
                                  ZZ9KArchiveLzmaInfo *info)
{
  uint64_t unpacked_size;

  if (!data || !info || length < 14U ||
      data[0] >= (9U * 5U * 5U) ||
      zz9k_archive_get_le32(data + 1U) < 4096U) {
    return 0;
  }

  memset(info, 0, sizeof(*info));
  strcpy(info->name, "output");
  info->compressed_offset = 0U;
  info->compressed_size = length;
  unpacked_size = zz9k_archive_get_le64(data + 5U);
  if (unpacked_size == ~(uint64_t)0) {
    info->size_known = 0;
    info->uncompressed_size = 0U;
    return 1;
  }
  if (unpacked_size > 0x7fffffffULL) {
    return 0;
  }
  info->size_known = 1;
  info->uncompressed_size = (uint32_t)unpacked_size;
  return 1;
}

static int zz9k_archive_lzma_info_from_header(
    const uint8_t *data,
    uint32_t length,
    uint32_t file_length,
    ZZ9KArchiveLzmaInfo *info)
{
  if (!zz9k_archive_lzma_info(data, length, info)) {
    return 0;
  }
  info->compressed_size = file_length;
  return file_length >= 14U;
}

static int zz9k_archive_lha_list(const uint8_t *data,
                                 uint32_t length,
                                 ZZ9KArchiveEntry *entries,
                                 uint32_t max_entries,
                                 uint32_t *count)
{
  uint32_t pos = 0U;
  uint32_t entries_used = 0U;

  if (!data || !count) {
    return 0;
  }
  *count = 0U;
  while (pos < length) {
    ZZ9KArchiveEntry entry;
    uint32_t header_size;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t name_len;
    uint32_t data_offset;
    uint32_t level;
    uint32_t method_offset;
    uint32_t base_header_size;
    uint32_t crc_offset;
    uint32_t os_offset;
    uint32_t ext_size_offset;
    uint32_t name_offset;
    uint32_t ext_size;
    uint32_t ext_pos;
    uint32_t ext_total;
    char ext_dir[ZZ9K_ARCHIVE_MAX_NAME];
    char ext_name[ZZ9K_ARCHIVE_MAX_NAME];

    header_size = data[pos];
    if (header_size == 0U) {
      *count = entries_used;
      return 1;
    }
    if (pos + 21U > length ||
        !zz9k_archive_lha_header_checksum_valid(data + pos,
                                                length - pos) ||
        data[pos + 20U] > 2U) {
      return 0;
    }
    level = data[pos + 20U];
    method_offset = pos + 2U;
    if (level == 2U) {
      header_size = zz9k_archive_get_le16(data + pos);
      base_header_size = 26U;
      crc_offset = pos + 21U;
      os_offset = pos + 23U;
      ext_size_offset = pos + 24U;
      name_len = 0U;
      name_offset = 0U;
      data_offset = pos + header_size;
    } else {
      base_header_size = level == 0U ? 22U : 25U;
      crc_offset = pos + 22U + data[pos + 21U];
      os_offset = crc_offset + 2U;
      ext_size_offset = os_offset + 1U;
      name_len = data[pos + 21U];
      name_offset = pos + 22U;
      data_offset = pos + 2U + header_size;
    }
    if (header_size < base_header_size ||
        (level == 2U && pos + header_size > length) ||
        (level != 2U && pos + 2U + header_size > length) ||
        data[method_offset] != '-' || data[method_offset + 4U] != '-' ||
        data[method_offset + 1U] != 'l' ||
        (data[method_offset + 2U] != 'h' &&
         data[method_offset + 2U] != 'z')) {
      return 0;
    }
    compressed_size = zz9k_archive_get_le32(data + pos + 7U);
    uncompressed_size = zz9k_archive_get_le32(data + pos + 11U);
    if (level != 2U && name_len > header_size - base_header_size) {
      return 0;
    }
    ext_size = 0U;
    ext_total = 0U;
    ext_pos = level == 2U ? pos + base_header_size : data_offset;
    memset(ext_dir, 0, sizeof(ext_dir));
    memset(ext_name, 0, sizeof(ext_name));
    if (level == 1U || level == 2U) {
      if (ext_size_offset + 2U > length) {
        return 0;
      }
      ext_size = zz9k_archive_get_le16(data + ext_size_offset);
      while (ext_size != 0U) {
        uint32_t ext_data_len;
        uint32_t next_ext_size;

        if (ext_size < 3U || ext_pos > length ||
            ext_size > length - ext_pos) {
          return 0;
        }
        ext_total += ext_size;
        ext_data_len = ext_size - 3U;
        next_ext_size = zz9k_archive_get_le16(
            data + ext_pos + ext_size - 2U);
        if (data[ext_pos] == 0x01U && ext_data_len != 0U) {
          while (ext_data_len != 0U &&
                 data[ext_pos + 1U + ext_data_len - 1U] == 0U) {
            ext_data_len--;
          }
          if (ext_data_len != 0U &&
              !zz9k_archive_copy_lha_name(
                  ext_name, sizeof(ext_name), data + ext_pos + 1U,
                  ext_data_len)) {
            return 0;
          }
        } else if (data[ext_pos] == 0x02U && ext_data_len != 0U) {
          while (ext_data_len != 0U &&
                 data[ext_pos + 1U + ext_data_len - 1U] == 0U) {
            ext_data_len--;
          }
          if (ext_data_len != 0U &&
              !zz9k_archive_copy_lha_name(
                  ext_dir, sizeof(ext_dir), data + ext_pos + 1U,
                  ext_data_len)) {
            return 0;
          }
        }
        ext_pos += ext_size;
        ext_size = next_ext_size;
      }
      data_offset = ext_pos;
    }
    if (level == 1U && ext_total != 0U &&
        data_offset <= length &&
        compressed_size > length - data_offset) {
      if (compressed_size < ext_total) {
        return 0;
      }
      compressed_size -= ext_total;
    }
    if (compressed_size > length || data_offset > length ||
        compressed_size > length - data_offset) {
      return 0;
    }

    memset(&entry, 0, sizeof(entry));
    if (name_len != 0U) {
      if (!zz9k_archive_copy_lha_name(
              entry.name, sizeof(entry.name), data + name_offset, name_len)) {
        return 0;
      }
    } else {
      strcpy(entry.name, "unnamed");
    }
    if (ext_name[0] != '\0') {
      if (!zz9k_archive_lha_join_dir_name(
              entry.name, sizeof(entry.name), ext_dir, ext_name)) {
        return 0;
      }
    } else if (ext_dir[0] != '\0') {
      if (!zz9k_archive_lha_join_dir_name(
              entry.name, sizeof(entry.name), ext_dir, entry.name)) {
        return 0;
      }
    }
    if (memcmp(data + method_offset, "-lh0-", 5U) == 0 ||
        memcmp(data + method_offset, "-lz4-", 5U) == 0) {
      entry.method = ZZ9K_ARCHIVE_LHA_METHOD_LH0;
    } else if (memcmp(data + method_offset, "-lhd-", 5U) == 0) {
      entry.method = ZZ9K_ARCHIVE_LHA_METHOD_LHD;
      entry.is_dir = 1U;
      if (ext_dir[0] != '\0' && ext_name[0] == '\0') {
        strcpy(entry.name, ext_dir);
      }
      if (!zz9k_archive_name_ends_with_slash(entry.name)) {
        size_t entry_name_len = strlen(entry.name);
        if (entry_name_len + 1U >= sizeof(entry.name)) {
          return 0;
        }
        entry.name[entry_name_len] = '/';
        entry.name[entry_name_len + 1U] = '\0';
      }
    } else if (memcmp(data + method_offset, "-lh1-", 5U) == 0) {
      entry.method = ZZ9K_ARCHIVE_LHA_METHOD_LH1;
    } else if (memcmp(data + method_offset, "-lh5-", 5U) == 0) {
      entry.method = ZZ9K_ARCHIVE_LHA_METHOD_LH5;
    } else if (memcmp(data + method_offset, "-lh6-", 5U) == 0) {
      entry.method = ZZ9K_ARCHIVE_LHA_METHOD_LH6;
    } else if (memcmp(data + method_offset, "-lh7-", 5U) == 0) {
      entry.method = ZZ9K_ARCHIVE_LHA_METHOD_LH7;
    } else {
      entry.method = 0xffffffffUL;
    }
    if (crc_offset + 2U <= length) {
      entry.crc32 = zz9k_archive_get_le16(data + crc_offset);
      entry.flags |= ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32;
    }
    entry.compressed_size = compressed_size;
    entry.uncompressed_size = uncompressed_size;
    entry.data_offset = data_offset;
    if (entry.method != ZZ9K_ARCHIVE_LHA_METHOD_LHD) {
      entry.is_dir = zz9k_archive_name_ends_with_slash(entry.name);
    }
    if (entries && entries_used < max_entries) {
      entries[entries_used] = entry;
    }
    entries_used++;
    pos = data_offset + compressed_size;
  }
  return 0;
}

static int zz9k_archive_lzma_output_capacity(
    const ZZ9KArchiveLzmaInfo *info,
    uint32_t requested_capacity,
    uint32_t *output_capacity)
{
  if (!info || !output_capacity) {
    return 0;
  }
  if (info->size_known) {
    if (requested_capacity != 0U &&
        requested_capacity < info->uncompressed_size) {
      return 0;
    }
    *output_capacity = info->uncompressed_size;
    return info->uncompressed_size != 0U;
  }
  if (requested_capacity == 0U) {
    return 0;
  }
  *output_capacity = requested_capacity;
  return 1;
}

static int zz9k_archive_lzma_props_dict_size(const uint8_t *props,
                                             uint32_t props_size,
                                             uint32_t *dict_size)
{
  if (!props || props_size != 5U || !dict_size) {
    return 0;
  }
  *dict_size = zz9k_archive_get_le32(props + 1U);
  return 1;
}

static int zz9k_archive_lzma2_prop_dict_size(uint8_t prop,
                                             uint32_t *dict_size)
{
  if (!dict_size || prop > 40U) {
    return 0;
  }
  if (prop == 40U) {
    *dict_size = 0xffffffffUL;
  } else {
    *dict_size =
        ((uint32_t)(2U | (prop & 1U))) << ((uint32_t)(prop / 2U) + 11U);
  }
  return 1;
}

static int zz9k_archive_7z_read_byte(ZZ9KArchive7zCursor *cursor,
                                     uint8_t *value)
{
  if (!cursor || !value || cursor->pos >= cursor->size) {
    return 0;
  }
  *value = cursor->data[cursor->pos++];
  return 1;
}

static int zz9k_archive_7z_skip(ZZ9KArchive7zCursor *cursor,
                                uint64_t length)
{
  if (!cursor || length > 0x7fffffffULL ||
      cursor->pos > cursor->size ||
      (uint32_t)length > cursor->size - cursor->pos) {
    return 0;
  }
  cursor->pos += (uint32_t)length;
  return 1;
}

static int zz9k_archive_7z_read_number(ZZ9KArchive7zCursor *cursor,
                                       uint64_t *value)
{
  uint8_t first;
  uint8_t mask;
  uint32_t i;
  uint64_t out;

  if (!value || !zz9k_archive_7z_read_byte(cursor, &first)) {
    return 0;
  }
  if ((first & 0x80U) == 0U) {
    *value = first;
    return 1;
  }

  out = 0U;
  mask = 0x80U;
  for (i = 0U; i < 8U; i++) {
    uint8_t next_mask = (uint8_t)(mask >> 1U);

    if ((first & mask) == 0U) {
      out |= (uint64_t)(first & (mask - 1U)) << (8U * i);
      *value = out;
      return 1;
    }
    {
      uint8_t b;

      if (!zz9k_archive_7z_read_byte(cursor, &b)) {
        return 0;
      }
      out |= (uint64_t)b << (8U * i);
    }
    mask = next_mask;
  }

  *value = out;
  return 1;
}

static int zz9k_archive_7z_skip_property(ZZ9KArchive7zCursor *cursor)
{
  uint64_t size;

  if (!zz9k_archive_7z_read_number(cursor, &size)) {
    return 0;
  }
  return zz9k_archive_7z_skip(cursor, size);
}

static int zz9k_archive_7z_bit_is_set(const uint8_t *bits, uint32_t bit)
{
  return bits && (bits[bit >> 3U] & (0x80U >> (bit & 7U))) != 0U;
}

static uint32_t zz9k_archive_7z_count_bits(const uint8_t *bits,
                                           uint32_t bit_count)
{
  uint32_t i;
  uint32_t count = 0U;

  if (!bits) {
    return 0U;
  }
  for (i = 0U; i < bit_count; i++) {
    if (zz9k_archive_7z_bit_is_set(bits, i)) {
      count++;
    }
  }
  return count;
}

static int zz9k_archive_7z_read_digests(ZZ9KArchive7zCursor *cursor,
                                        uint32_t item_count,
                                        uint32_t *crcs,
                                        uint8_t *defined)
{
  uint8_t all_defined;
  uint32_t defined_count = item_count;
  const uint8_t *defined_bits = 0;
  uint32_t i;

  if (!zz9k_archive_7z_read_byte(cursor, &all_defined)) {
    return 0;
  }
  if (all_defined == 0U) {
    uint32_t bit_bytes = (item_count + 7U) >> 3U;

    if (cursor->pos > cursor->size ||
        bit_bytes > cursor->size - cursor->pos) {
      return 0;
    }
    defined_count = zz9k_archive_7z_count_bits(
        cursor->data + cursor->pos, item_count);
    defined_bits = cursor->data + cursor->pos;
    cursor->pos += bit_bytes;
  }
  if (defined_count > 0x1fffffffUL ||
      cursor->pos > cursor->size ||
      defined_count * 4U > cursor->size - cursor->pos) {
    return 0;
  }
  for (i = 0U; i < item_count; i++) {
    int is_defined = all_defined != 0U ||
        zz9k_archive_7z_bit_is_set(defined_bits, i);

    if (defined) {
      defined[i] = (uint8_t)(is_defined ? 1U : 0U);
    }
    if (is_defined) {
      uint32_t crc = zz9k_archive_get_le32(cursor->data + cursor->pos);

      cursor->pos += 4U;
      if (crcs) {
        crcs[i] = crc;
      }
    } else if (crcs) {
      crcs[i] = 0U;
    }
  }
  return 1;
}

static int zz9k_archive_7z_skip_digests(ZZ9KArchive7zCursor *cursor,
                                        uint32_t item_count)
{
  return zz9k_archive_7z_read_digests(cursor, item_count, 0, 0);
}

static int zz9k_archive_7z_copy_utf16_name(const uint8_t *names,
                                           uint32_t names_size,
                                           uint32_t *names_pos,
                                           char *dst,
                                           uint32_t dst_capacity)
{
  uint32_t out = 0U;

  if (!names || !names_pos || !dst || dst_capacity == 0U) {
    return 0;
  }
  while (*names_pos + 1U < names_size) {
    uint8_t low = names[*names_pos];
    uint8_t high = names[*names_pos + 1U];
    char ch;

    *names_pos += 2U;
    if (low == 0U && high == 0U) {
      if (out == 0U || out >= dst_capacity) {
        return 0;
      }
      dst[out] = '\0';
      zz9k_archive_strip_current_dir_prefix(dst);
      return 1;
    }
    if (out + 1U >= dst_capacity) {
      return 0;
    }
    ch = (high == 0U && low >= 0x20U) ? (char)low : '?';
    if (ch == '\\') {
      ch = '/';
    }
    dst[out++] = ch;
  }
  return 0;
}

static int zz9k_archive_7z_start_header_from_prefix(
    const uint8_t *data,
    uint32_t prefix_length,
    uint32_t archive_length,
    ZZ9KArchive7zHeader *header)
{
  uint64_t relative_offset;
  uint64_t header_size;
  uint32_t start_header_crc;
  uint32_t next_header_crc;
  uint32_t next_header_offset;
  uint32_t next_header_size;

  if (!data || !header ||
      prefix_length < ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE ||
      archive_length < ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE ||
      data[0] != 0x37U || data[1] != 0x7aU ||
      data[2] != 0xbcU || data[3] != 0xafU ||
      data[4] != 0x27U || data[5] != 0x1cU ||
      data[6] != 0U) {
    return 0;
  }
  start_header_crc = zz9k_archive_get_le32(data + 8U);
  if (zz9k_archive_crc32(0U, data + 12U, 20U) != start_header_crc) {
    return 0;
  }

  relative_offset = zz9k_archive_get_le64(data + 12U);
  header_size = zz9k_archive_get_le64(data + 20U);
  if (relative_offset > 0x7fffffffULL ||
      header_size > 0x7fffffffULL ||
      relative_offset + header_size < relative_offset ||
      ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE + relative_offset >
        0x7fffffffULL ||
      ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE + relative_offset + header_size >
        archive_length) {
    return 0;
  }

  next_header_offset =
      (uint32_t)(ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE + relative_offset);
  next_header_size = (uint32_t)header_size;
  next_header_crc = zz9k_archive_get_le32(data + 28U);
  if (next_header_size == 0U) {
    return 0;
  }
  if (next_header_offset <= prefix_length &&
      next_header_size <= prefix_length - next_header_offset &&
      zz9k_archive_crc32(0U, data + next_header_offset, next_header_size) !=
          next_header_crc) {
    return 0;
  }

  header->next_header_offset = next_header_offset;
  header->next_header_size = next_header_size;
  header->next_header_crc = next_header_crc;
  return header->next_header_size != 0U;
}

static int zz9k_archive_7z_start_header(const uint8_t *data,
                                        uint32_t length,
                                        ZZ9KArchive7zHeader *header)
{
  return zz9k_archive_7z_start_header_from_prefix(
      data, length, length, header);
}

static int zz9k_archive_7z_header_is_encoded(const uint8_t *data,
                                             uint32_t length)
{
  ZZ9KArchive7zHeader header;

  if (!zz9k_archive_7z_start_header(data, length, &header) ||
      header.next_header_size == 0U ||
      header.next_header_offset >= length) {
    return 0;
  }
  return data[header.next_header_offset] ==
         ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER;
}

static void zz9k_archive_7z_streams_init(ZZ9KArchive7zStreams *streams)
{
  if (streams) {
    memset(streams, 0, sizeof(*streams));
  }
}

static void zz9k_archive_7z_streams_free(ZZ9KArchive7zStreams *streams)
{
  if (!streams) {
    return;
  }
  free(streams->data_offsets);
  free(streams->decoded_offsets);
  free(streams->pack_sizes);
  free(streams->unpack_sizes);
  free(streams->crcs);
  free(streams->methods);
  free(streams->flags);
  free(streams->crc_defined);
  free(streams->props_sizes);
  free(streams->props);
  zz9k_archive_7z_streams_init(streams);
}

static int zz9k_archive_7z_streams_alloc(ZZ9KArchive7zStreams *streams,
                                         uint32_t count)
{
  if (!streams || count == 0U || count > 65535U) {
    return 0;
  }
  zz9k_archive_7z_streams_free(streams);
  streams->data_offsets = (uint32_t *)calloc((size_t)count,
                                             sizeof(uint32_t));
  streams->decoded_offsets = (uint32_t *)calloc((size_t)count,
                                                sizeof(uint32_t));
  streams->pack_sizes = (uint32_t *)calloc((size_t)count,
                                           sizeof(uint32_t));
  streams->unpack_sizes = (uint32_t *)calloc((size_t)count,
                                             sizeof(uint32_t));
  streams->crcs = (uint32_t *)calloc((size_t)count,
                                     sizeof(uint32_t));
  streams->methods = (uint32_t *)calloc((size_t)count,
                                        sizeof(uint32_t));
  streams->flags = (uint32_t *)calloc((size_t)count,
                                      sizeof(uint32_t));
  streams->crc_defined = (uint8_t *)calloc((size_t)count,
                                           sizeof(uint8_t));
  streams->props_sizes = (uint8_t *)calloc((size_t)count,
                                           sizeof(uint8_t));
  streams->props = (uint8_t *)calloc((size_t)count,
                                     ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS);
  if (!streams->data_offsets || !streams->decoded_offsets ||
      !streams->pack_sizes ||
      !streams->unpack_sizes || !streams->crcs || !streams->methods ||
      !streams->flags || !streams->crc_defined ||
      !streams->props_sizes || !streams->props) {
    zz9k_archive_7z_streams_free(streams);
    return 0;
  }
  streams->count = count;
  return 1;
}

static int zz9k_archive_7z_parse_pack_info(ZZ9KArchive7zCursor *cursor,
                                           uint32_t archive_length,
                                           ZZ9KArchive7zStreams *streams)
{
  uint64_t pack_pos64;
  uint64_t stream_count64;
  uint64_t type;
  uint32_t stream_count;
  uint32_t data_pos;
  uint32_t i;

  if (!cursor ||
      !zz9k_archive_7z_read_number(cursor, &pack_pos64) ||
      !zz9k_archive_7z_read_number(cursor, &stream_count64) ||
      pack_pos64 > 0x7fffffffULL ||
      stream_count64 > 65535ULL) {
    return 0;
  }
  stream_count = (uint32_t)stream_count64;
  if (!zz9k_archive_7z_streams_alloc(streams, stream_count)) {
    return 0;
  }
  if (!zz9k_archive_7z_read_number(cursor, &type) ||
      type != ZZ9K_ARCHIVE_7Z_ID_SIZE) {
    return 0;
  }

  data_pos = ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE + (uint32_t)pack_pos64;
  for (i = 0U; i < stream_count; i++) {
    uint64_t size64;

    if (!zz9k_archive_7z_read_number(cursor, &size64) ||
        size64 > 0x7fffffffULL ||
        data_pos > archive_length ||
        (uint32_t)size64 > archive_length - data_pos) {
      return 0;
    }
    streams->data_offsets[i] = data_pos;
    streams->pack_sizes[i] = (uint32_t)size64;
    data_pos += (uint32_t)size64;
  }

  while (1) {
    if (!zz9k_archive_7z_read_number(cursor, &type)) {
      return 0;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_END) {
      return 1;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_CRC) {
      if (!zz9k_archive_7z_skip_digests(cursor, stream_count)) {
        return 0;
      }
    } else {
      return 0;
    }
  }
}

static int zz9k_archive_7z_read_simple_folder(ZZ9KArchive7zCursor *cursor,
                                              uint32_t *method,
                                              uint8_t *props,
                                              uint8_t *props_size)
{
  uint64_t num_coders;
  uint8_t main_byte;
  uint32_t id_size;
  uint32_t parsed_method = 0U;
  uint32_t i;

  if (!method || !props || !props_size) {
    return 0;
  }
  *method = 0U;
  *props_size = 0U;
  memset(props, 0, ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS);

  if (!zz9k_archive_7z_read_number(cursor, &num_coders) ||
      num_coders != 1U ||
      !zz9k_archive_7z_read_byte(cursor, &main_byte) ||
      (main_byte & 0xc0U) != 0U) {
    return 0;
  }

  id_size = main_byte & 0x0fU;
  if (id_size == 0U || id_size > 4U ||
      cursor->pos > cursor->size ||
      id_size > cursor->size - cursor->pos) {
    return 0;
  }
  for (i = 0U; i < id_size; i++) {
    parsed_method = (parsed_method << 8U) | cursor->data[cursor->pos++];
  }
  if ((main_byte & 0x10U) != 0U) {
    uint64_t in_streams;
    uint64_t out_streams;

    if (!zz9k_archive_7z_read_number(cursor, &in_streams) ||
        !zz9k_archive_7z_read_number(cursor, &out_streams) ||
        in_streams != 1U || out_streams != 1U) {
      return 0;
    }
  }
  if ((main_byte & 0x20U) != 0U) {
    uint64_t parsed_props_size;

    if (!zz9k_archive_7z_read_number(cursor, &parsed_props_size) ||
        parsed_props_size > ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS ||
        cursor->pos > cursor->size ||
        (uint32_t)parsed_props_size > cursor->size - cursor->pos) {
      return 0;
    }
    memcpy(props, cursor->data + cursor->pos, (uint32_t)parsed_props_size);
    cursor->pos += (uint32_t)parsed_props_size;
    *props_size = (uint8_t)parsed_props_size;
  }
  if (parsed_method == ZZ9K_ARCHIVE_7Z_METHOD_COPY) {
    if (*props_size != 0U) {
      return 0;
    }
  } else if (parsed_method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
    if (*props_size != 0U) {
      return 0;
    }
  } else if (parsed_method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
    if (*props_size != 5U) {
      return 0;
    }
  } else if (parsed_method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
    if (*props_size != 1U) {
      return 0;
    }
  } else {
    return 0;
  }
  *method = parsed_method;
  return 1;
}

static int zz9k_archive_7z_parse_unpack_info(ZZ9KArchive7zCursor *cursor,
                                             ZZ9KArchive7zStreams *streams)
{
  uint64_t type;
  uint64_t folder_count64;
  uint8_t external;
  uint32_t i;

  if (!cursor || !streams || streams->count == 0U ||
      !zz9k_archive_7z_read_number(cursor, &type) ||
      type != ZZ9K_ARCHIVE_7Z_ID_FOLDER ||
      !zz9k_archive_7z_read_number(cursor, &folder_count64) ||
      folder_count64 != streams->count ||
      !zz9k_archive_7z_read_byte(cursor, &external) ||
      external != 0U) {
    return 0;
  }

  for (i = 0U; i < streams->count; i++) {
    uint8_t *props;
    uint32_t method;
    uint8_t props_size;

    props = streams->props +
            i * ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS;
    if (!zz9k_archive_7z_read_simple_folder(cursor, &method,
                                            props, &props_size)) {
      return 0;
    }
    streams->methods[i] = method;
    streams->props_sizes[i] = props_size;
  }

  if (!zz9k_archive_7z_read_number(cursor, &type) ||
      type != ZZ9K_ARCHIVE_7Z_ID_CODERS_UNPACK_SIZE) {
    return 0;
  }
  for (i = 0U; i < streams->count; i++) {
    uint64_t unpack_size;

    if (!zz9k_archive_7z_read_number(cursor, &unpack_size) ||
        unpack_size > 0x7fffffffULL) {
      return 0;
    }
    streams->unpack_sizes[i] = (uint32_t)unpack_size;
  }
  while (1) {
    if (!zz9k_archive_7z_read_number(cursor, &type)) {
      return 0;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_END) {
      return 1;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_CRC) {
      if (!zz9k_archive_7z_read_digests(
              cursor, streams->count, streams->crcs,
              streams->crc_defined)) {
        return 0;
      }
    } else {
      return 0;
    }
  }
}

static int zz9k_archive_7z_streams_flatten_substreams(
    ZZ9KArchive7zStreams *streams,
    const uint32_t *substream_counts,
    const uint32_t *substream_sizes,
    const uint32_t *substream_crcs,
    const uint8_t *substream_crc_defined,
    uint32_t substream_count)
{
  uint32_t folder_count;
  uint32_t *data_offsets = 0;
  uint32_t *decoded_offsets = 0;
  uint32_t *pack_sizes = 0;
  uint32_t *unpack_sizes = 0;
  uint32_t *crcs = 0;
  uint32_t *methods = 0;
  uint32_t *flags = 0;
  uint8_t *crc_defined = 0;
  uint8_t *props_sizes = 0;
  uint8_t *props = 0;
  uint32_t folder;
  uint32_t out = 0U;
  int ok = 0;

  if (!streams || !substream_counts || !substream_sizes ||
      substream_count == 0U || substream_count > 65535U) {
    return 0;
  }
  folder_count = streams->count;
  data_offsets = (uint32_t *)calloc((size_t)substream_count,
                                    sizeof(uint32_t));
  decoded_offsets = (uint32_t *)calloc((size_t)substream_count,
                                       sizeof(uint32_t));
  pack_sizes = (uint32_t *)calloc((size_t)substream_count,
                                  sizeof(uint32_t));
  unpack_sizes = (uint32_t *)calloc((size_t)substream_count,
                                    sizeof(uint32_t));
  crcs = (uint32_t *)calloc((size_t)substream_count, sizeof(uint32_t));
  methods = (uint32_t *)calloc((size_t)substream_count, sizeof(uint32_t));
  flags = (uint32_t *)calloc((size_t)substream_count, sizeof(uint32_t));
  crc_defined = (uint8_t *)calloc((size_t)substream_count,
                                  sizeof(uint8_t));
  props_sizes = (uint8_t *)calloc((size_t)substream_count,
                                  sizeof(uint8_t));
  props = (uint8_t *)calloc((size_t)substream_count,
                            ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS);
  if (!data_offsets || !decoded_offsets || !pack_sizes || !unpack_sizes ||
      !crcs || !methods || !flags || !crc_defined || !props_sizes || !props) {
    goto out;
  }

  for (folder = 0U; folder < folder_count; folder++) {
    uint32_t count = substream_counts[folder];
    uint32_t offset = streams->data_offsets[folder];
    uint32_t total = 0U;
    int unsupported_split = 0;
    uint32_t i;

    if (count == 0U || out + count < out || out + count > substream_count) {
      goto out;
    }
    if (count > 1U &&
        (streams->methods[folder] != ZZ9K_ARCHIVE_7Z_METHOD_COPY ||
         streams->pack_sizes[folder] != streams->unpack_sizes[folder])) {
      unsupported_split = 1;
    }
    for (i = 0U; i < count; i++) {
      uint32_t size = substream_sizes[out + i];

      if (total > streams->unpack_sizes[folder] ||
          size > streams->unpack_sizes[folder] - total) {
        goto out;
      }
      data_offsets[out + i] = unsupported_split ? streams->data_offsets[folder] :
          (count == 1U ? streams->data_offsets[folder] : offset);
      decoded_offsets[out + i] = total;
      pack_sizes[out + i] = unsupported_split ? streams->pack_sizes[folder] :
          (streams->methods[folder] == ZZ9K_ARCHIVE_7Z_METHOD_COPY ?
           size : streams->pack_sizes[folder]);
      unpack_sizes[out + i] = size;
      methods[out + i] = streams->methods[folder];
      flags[out + i] = streams->flags[folder];
      if (unsupported_split) {
        flags[out + i] |= ZZ9K_ARCHIVE_ENTRY_FLAG_7Z_UNSUPPORTED_SPLIT;
      }
      props_sizes[out + i] = streams->props_sizes[folder];
      if (props_sizes[out + i] != 0U) {
        memcpy(props + (out + i) * ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS,
               streams->props + folder * ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS,
               props_sizes[out + i]);
      }
      if (substream_crc_defined && substream_crc_defined[out + i]) {
        crcs[out + i] = substream_crcs[out + i];
        crc_defined[out + i] = 1U;
      } else if (count == 1U && streams->crc_defined[folder]) {
        crcs[out + i] = streams->crcs[folder];
        crc_defined[out + i] = 1U;
      }
      offset += size;
      total += size;
    }
    if (total != streams->unpack_sizes[folder]) {
      goto out;
    }
    out += count;
  }
  if (out != substream_count) {
    goto out;
  }

  free(streams->data_offsets);
  free(streams->decoded_offsets);
  free(streams->pack_sizes);
  free(streams->unpack_sizes);
  free(streams->crcs);
  free(streams->methods);
  free(streams->flags);
  free(streams->crc_defined);
  free(streams->props_sizes);
  free(streams->props);
  streams->data_offsets = data_offsets;
  streams->decoded_offsets = decoded_offsets;
  streams->pack_sizes = pack_sizes;
  streams->unpack_sizes = unpack_sizes;
  streams->crcs = crcs;
  streams->methods = methods;
  streams->flags = flags;
  streams->crc_defined = crc_defined;
  streams->props_sizes = props_sizes;
  streams->props = props;
  streams->count = substream_count;
  data_offsets = 0;
  decoded_offsets = 0;
  pack_sizes = 0;
  unpack_sizes = 0;
  crcs = 0;
  methods = 0;
  flags = 0;
  crc_defined = 0;
  props_sizes = 0;
  props = 0;
  ok = 1;

out:
  free(data_offsets);
  free(decoded_offsets);
  free(pack_sizes);
  free(unpack_sizes);
  free(crcs);
  free(methods);
  free(flags);
  free(crc_defined);
  free(props_sizes);
  free(props);
  return ok;
}

static int zz9k_archive_7z_parse_substreams_info(
    ZZ9KArchive7zCursor *cursor,
    ZZ9KArchive7zStreams *streams)
{
  uint64_t type;
  uint32_t folder_count;
  uint32_t substream_count;
  uint32_t *substream_counts = 0;
  uint32_t *substream_sizes = 0;
  uint32_t *substream_crcs = 0;
  uint8_t *substream_crc_defined = 0;
  int saw_size = 0;
  int ok = 0;
  uint32_t i;

  if (!cursor || !streams) {
    return 0;
  }
  folder_count = streams->count;
  substream_count = folder_count;
  substream_counts = (uint32_t *)calloc((size_t)folder_count,
                                        sizeof(uint32_t));
  if (!substream_counts) {
    return 0;
  }
  for (i = 0U; i < folder_count; i++) {
    substream_counts[i] = 1U;
  }

  while (1) {
    if (!zz9k_archive_7z_read_number(cursor, &type)) {
      goto out;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_END) {
      uint32_t i;

      if (!saw_size) {
        substream_sizes = (uint32_t *)calloc((size_t)substream_count,
                                             sizeof(uint32_t));
        if (!substream_sizes) {
          goto out;
        }
        for (i = 0U; i < folder_count; i++) {
          if (substream_counts[i] != 1U) {
            goto out;
          }
          substream_sizes[i] = streams->unpack_sizes[i];
        }
      }
      ok = zz9k_archive_7z_streams_flatten_substreams(
          streams, substream_counts, substream_sizes, substream_crcs,
          substream_crc_defined, substream_count);
      goto out;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_NUM_UNPACK_STREAM) {
      uint32_t i;

      if (saw_size || substream_crcs || substream_crc_defined) {
        goto out;
      }
      substream_count = 0U;
      for (i = 0U; i < streams->count; i++) {
        uint64_t count64;

        if (!zz9k_archive_7z_read_number(cursor, &count64) ||
            count64 == 0U || count64 > 65535ULL ||
            substream_count + (uint32_t)count64 < substream_count ||
            substream_count + (uint32_t)count64 > 65535U) {
          goto out;
        }
        substream_counts[i] = (uint32_t)count64;
        substream_count += (uint32_t)count64;
      }
    } else if (type == ZZ9K_ARCHIVE_7Z_ID_SIZE) {
      uint32_t out_index = 0U;
      uint32_t folder;

      if (saw_size) {
        goto out;
      }
      substream_sizes = (uint32_t *)calloc((size_t)substream_count,
                                           sizeof(uint32_t));
      if (!substream_sizes) {
        goto out;
      }
      for (folder = 0U; folder < folder_count; folder++) {
        uint32_t count = substream_counts[folder];
        uint32_t total = 0U;
        uint32_t i;

        for (i = 0U; i + 1U < count; i++) {
          uint64_t size64;

          if (!zz9k_archive_7z_read_number(cursor, &size64) ||
              size64 > 0x7fffffffULL ||
              total + (uint32_t)size64 < total ||
              total + (uint32_t)size64 >
                streams->unpack_sizes[folder]) {
            goto out;
          }
          substream_sizes[out_index++] = (uint32_t)size64;
          total += (uint32_t)size64;
        }
        substream_sizes[out_index++] = streams->unpack_sizes[folder] - total;
      }
      if (out_index != substream_count) {
        goto out;
      }
      saw_size = 1;
    } else if (type == ZZ9K_ARCHIVE_7Z_ID_CRC) {
      if (substream_crcs || substream_crc_defined) {
        goto out;
      }
      substream_crcs = (uint32_t *)calloc((size_t)substream_count,
                                          sizeof(uint32_t));
      substream_crc_defined = (uint8_t *)calloc((size_t)substream_count,
                                                sizeof(uint8_t));
      if (!substream_crcs || !substream_crc_defined ||
          !zz9k_archive_7z_read_digests(
              cursor, substream_count, substream_crcs,
              substream_crc_defined)) {
        goto out;
      }
    } else {
      goto out;
    }
  }

out:
  free(substream_counts);
  free(substream_sizes);
  free(substream_crcs);
  free(substream_crc_defined);
  return ok;
}

static int zz9k_archive_7z_parse_streams_info(
    ZZ9KArchive7zCursor *cursor,
    uint32_t archive_length,
    ZZ9KArchive7zStreams *streams)
{
  uint64_t type;
  int saw_pack = 0;
  int saw_unpack = 0;

  if (!cursor || !streams) {
    return 0;
  }
  while (1) {
    if (!zz9k_archive_7z_read_number(cursor, &type)) {
      return 0;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_END) {
      return saw_pack && saw_unpack;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_PACK_INFO) {
      if (saw_pack ||
          !zz9k_archive_7z_parse_pack_info(
              cursor, archive_length, streams)) {
        return 0;
      }
      saw_pack = 1;
    } else if (type == ZZ9K_ARCHIVE_7Z_ID_UNPACK_INFO) {
      if (!saw_pack || saw_unpack ||
          !zz9k_archive_7z_parse_unpack_info(cursor, streams)) {
        return 0;
      }
      saw_unpack = 1;
    } else if (type == ZZ9K_ARCHIVE_7Z_ID_SUBSTREAMS_INFO) {
      if (!saw_unpack ||
          !zz9k_archive_7z_parse_substreams_info(cursor, streams)) {
        return 0;
      }
    } else {
      return 0;
    }
  }
}

static int zz9k_archive_7z_files_info_property_supported(uint64_t type)
{
  switch ((uint32_t)type) {
  case ZZ9K_ARCHIVE_7Z_ID_CREATION_TIME:
  case ZZ9K_ARCHIVE_7Z_ID_LAST_ACCESS_TIME:
  case ZZ9K_ARCHIVE_7Z_ID_LAST_WRITE_TIME:
  case ZZ9K_ARCHIVE_7Z_ID_WIN_ATTRIBUTES:
  case ZZ9K_ARCHIVE_7Z_ID_COMMENT:
  case ZZ9K_ARCHIVE_7Z_ID_START_POS:
  case ZZ9K_ARCHIVE_7Z_ID_DUMMY:
    return 1;
  default:
    return 0;
  }
}

static int zz9k_archive_7z_parse_files_info(
    ZZ9KArchive7zCursor *cursor,
    const ZZ9KArchive7zStreams *streams,
    ZZ9KArchiveEntry *entries,
    uint32_t entry_capacity,
    uint32_t *entry_count)
{
  uint64_t num_files64;
  uint32_t num_files;
  const uint8_t *empty_streams = 0;
  const uint8_t *empty_files = 0;
  const uint8_t *names = 0;
  uint32_t empty_streams_size = 0U;
  uint32_t empty_files_size = 0U;
  uint32_t names_size = 0U;
  uint32_t empty_count = 0U;
  uint32_t names_pos = 0U;
  uint32_t empty_index = 0U;
  uint32_t stream_index = 0U;
  uint32_t count = 0U;
  uint32_t i;

  if (!cursor || !entry_count ||
      !zz9k_archive_7z_read_number(cursor, &num_files64) ||
      num_files64 > 65535ULL) {
    return 0;
  }
  num_files = (uint32_t)num_files64;

  while (1) {
    uint64_t type;
    uint64_t size64;
    const uint8_t *property;
    uint32_t property_size;

    if (!zz9k_archive_7z_read_number(cursor, &type)) {
      return 0;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_END) {
      break;
    }
    if (!zz9k_archive_7z_read_number(cursor, &size64) ||
        size64 > 0x7fffffffULL ||
        cursor->pos > cursor->size ||
        (uint32_t)size64 > cursor->size - cursor->pos) {
      return 0;
    }
    property = cursor->data + cursor->pos;
    property_size = (uint32_t)size64;
    cursor->pos += property_size;

    if (type == ZZ9K_ARCHIVE_7Z_ID_EMPTY_STREAM) {
      if (property_size < ((num_files + 7U) >> 3U)) {
        return 0;
      }
      empty_streams = property;
      empty_streams_size = property_size;
      empty_count = zz9k_archive_7z_count_bits(empty_streams, num_files);
    } else if (type == ZZ9K_ARCHIVE_7Z_ID_EMPTY_FILE) {
      empty_files = property;
      empty_files_size = property_size;
    } else if (type == ZZ9K_ARCHIVE_7Z_ID_NAME) {
      if (property_size < 1U || property[0] != 0U) {
        return 0;
      }
      names = property + 1U;
      names_size = property_size - 1U;
    } else if (!zz9k_archive_7z_files_info_property_supported(type)) {
      return 0;
    }
  }

  if (num_files == 0U) {
    *entry_count = 0U;
    return 1;
  }
  if (!names ||
      (empty_streams && empty_streams_size < ((num_files + 7U) >> 3U)) ||
      (empty_files && empty_files_size < ((empty_count + 7U) >> 3U))) {
    return 0;
  }

  for (i = 0U; i < num_files; i++) {
    int is_empty = empty_streams ?
        zz9k_archive_7z_bit_is_set(empty_streams, i) : 0;
    ZZ9KArchiveEntry candidate;

    memset(&candidate, 0, sizeof(candidate));
    if (!zz9k_archive_7z_copy_utf16_name(
            names, names_size, &names_pos,
            candidate.name, sizeof(candidate.name))) {
      return 0;
    }
    candidate.method = ZZ9K_ARCHIVE_7Z_METHOD_COPY;
    if (is_empty) {
      candidate.is_dir = empty_files ?
          (zz9k_archive_7z_bit_is_set(empty_files, empty_index) ? 0U : 1U) :
          1U;
    } else {
      if (!streams || stream_index >= streams->count) {
        return 0;
      }
      candidate.data_offset = streams->data_offsets[stream_index];
      candidate.decoded_offset = streams->decoded_offsets[stream_index];
      candidate.compressed_size = streams->pack_sizes[stream_index];
      candidate.uncompressed_size = streams->unpack_sizes[stream_index];
      candidate.flags = streams->flags[stream_index];
      if (streams->crc_defined[stream_index]) {
        candidate.crc32 = streams->crcs[stream_index];
        candidate.flags |= ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32;
      }
      candidate.method = streams->methods[stream_index];
      candidate.method_props_size = streams->props_sizes[stream_index];
      if (candidate.method_props_size != 0U) {
        memcpy(candidate.method_props,
               streams->props +
               stream_index * ZZ9K_ARCHIVE_7Z_MAX_METHOD_PROPS,
               candidate.method_props_size);
      }
      stream_index++;
    }
    if (is_empty) {
      empty_index++;
    }
    if (zz9k_archive_7z_entry_is_root_metadata(&candidate)) {
      continue;
    }
    if (entries) {
      if (count >= entry_capacity) {
        return 0;
      }
      entries[count] = candidate;
    }
    count++;
  }
  if (streams && stream_index != streams->count) {
    return 0;
  }

  *entry_count = count;
  return 1;
}

static int zz9k_archive_7z_encoded_header_entry(
    const uint8_t *header_data,
    uint32_t header_length,
    uint32_t archive_length,
    ZZ9KArchiveEntry *entry)
{
  ZZ9KArchive7zCursor cursor;
  ZZ9KArchive7zStreams streams;
  uint64_t type;
  int ok = 0;

  if (!header_data || header_length == 0U || !entry) {
    return 0;
  }
  memset(entry, 0, sizeof(*entry));
  zz9k_archive_7z_streams_init(&streams);

  cursor.data = header_data;
  cursor.size = header_length;
  cursor.pos = 0U;
  if (!zz9k_archive_7z_read_number(&cursor, &type) ||
      type != ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER ||
      !zz9k_archive_7z_parse_streams_info(
          &cursor, archive_length, &streams) ||
      streams.count != 1U ||
      streams.data_offsets[0] > archive_length ||
      streams.pack_sizes[0] > archive_length - streams.data_offsets[0]) {
    goto out;
  }
  strcpy(entry->name, "7z-encoded-header");
  entry->method = streams.methods[0];
  entry->data_offset = streams.data_offsets[0];
  entry->compressed_size = streams.pack_sizes[0];
  entry->uncompressed_size = streams.unpack_sizes[0];
  entry->flags = streams.flags[0];
  entry->method_props_size = streams.props_sizes[0];
  if (entry->method_props_size != 0U) {
    memcpy(entry->method_props, streams.props,
           entry->method_props_size);
  }
  if (streams.crc_defined[0]) {
    entry->crc32 = streams.crcs[0];
    entry->flags |= ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32;
  }
  ok = 1;

out:
  zz9k_archive_7z_streams_free(&streams);
  return ok;
}

static int zz9k_archive_7z_copy_encoded_header(
    const uint8_t *header_data,
    uint32_t header_length,
    const uint8_t *archive_data,
    uint32_t archive_length,
    uint8_t **decoded_header,
    uint32_t *decoded_length)
{
  ZZ9KArchive7zCursor cursor;
  ZZ9KArchive7zStreams streams;
  uint64_t type;
  uint8_t *bytes = 0;
  int ok = 0;

  if (!header_data || header_length == 0U ||
      !archive_data || !decoded_header || !decoded_length) {
    return 0;
  }
  *decoded_header = 0;
  *decoded_length = 0U;
  zz9k_archive_7z_streams_init(&streams);

  cursor.data = header_data;
  cursor.size = header_length;
  cursor.pos = 0U;
  if (!zz9k_archive_7z_read_number(&cursor, &type) ||
      type != ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER ||
      !zz9k_archive_7z_parse_streams_info(
          &cursor, archive_length, &streams) ||
      streams.count != 1U ||
      streams.methods[0] != ZZ9K_ARCHIVE_7Z_METHOD_COPY ||
      streams.pack_sizes[0] != streams.unpack_sizes[0] ||
      streams.data_offsets[0] > archive_length ||
      streams.pack_sizes[0] > archive_length - streams.data_offsets[0]) {
    goto out;
  }
  if (streams.crc_defined[0] &&
      zz9k_archive_crc32(0U, archive_data + streams.data_offsets[0],
                         streams.pack_sizes[0]) != streams.crcs[0]) {
    printf("7z encoded header crc mismatch\n");
    goto out;
  }
  bytes = (uint8_t *)malloc((size_t)streams.unpack_sizes[0]);
  if (!bytes) {
    goto out;
  }
  memcpy(bytes, archive_data + streams.data_offsets[0],
         streams.unpack_sizes[0]);
  *decoded_header = bytes;
  *decoded_length = streams.unpack_sizes[0];
  bytes = 0;
  ok = 1;

out:
  free(bytes);
  zz9k_archive_7z_streams_free(&streams);
  return ok;
}

static int zz9k_archive_7z_copy_encoded_header_from_file(
    const uint8_t *header_data,
    uint32_t header_length,
    const char *archive_path,
    uint32_t archive_length,
    uint8_t **decoded_header,
    uint32_t *decoded_length)
{
  ZZ9KArchive7zCursor cursor;
  ZZ9KArchive7zStreams streams;
  uint64_t type;
  uint8_t *bytes = 0;
  int ok = 0;

  if (!header_data || header_length == 0U || !archive_path ||
      !decoded_header || !decoded_length) {
    return 0;
  }
  *decoded_header = 0;
  *decoded_length = 0U;
  zz9k_archive_7z_streams_init(&streams);

  cursor.data = header_data;
  cursor.size = header_length;
  cursor.pos = 0U;
  if (!zz9k_archive_7z_read_number(&cursor, &type) ||
      type != ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER ||
      !zz9k_archive_7z_parse_streams_info(
          &cursor, archive_length, &streams) ||
      streams.count != 1U ||
      streams.methods[0] != ZZ9K_ARCHIVE_7Z_METHOD_COPY ||
      streams.pack_sizes[0] != streams.unpack_sizes[0] ||
      !zz9k_archive_read_file_range(
          archive_path, streams.data_offsets[0], streams.pack_sizes[0],
          &bytes)) {
    goto out;
  }
  if (streams.crc_defined[0] &&
      zz9k_archive_crc32(0U, bytes, streams.pack_sizes[0]) !=
        streams.crcs[0]) {
    printf("7z encoded header crc mismatch: %s\n", archive_path);
    goto out;
  }
  *decoded_header = bytes;
  *decoded_length = streams.unpack_sizes[0];
  bytes = 0;
  ok = 1;

out:
  free(bytes);
  zz9k_archive_7z_streams_free(&streams);
  return ok;
}

static int zz9k_archive_7z_list_from_header(const uint8_t *header_data,
                                            uint32_t header_length,
                                            uint32_t archive_length,
                                            ZZ9KArchiveEntry *entries,
                                            uint32_t entry_capacity,
                                            uint32_t *entry_count)
{
  ZZ9KArchive7zCursor cursor;
  ZZ9KArchive7zStreams streams;
  uint64_t type;
  int ok = 0;

  if (!header_data || header_length == 0U || !entry_count) {
    return 0;
  }
  zz9k_archive_7z_streams_init(&streams);

  cursor.data = header_data;
  cursor.size = header_length;
  cursor.pos = 0U;
  if (!zz9k_archive_7z_read_number(&cursor, &type) ||
      type != ZZ9K_ARCHIVE_7Z_ID_HEADER) {
    goto out;
  }

  while (1) {
    if (!zz9k_archive_7z_read_number(&cursor, &type)) {
      goto out;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_END) {
      *entry_count = 0U;
      ok = 1;
      goto out;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_ARCHIVE_PROPERTIES) {
      while (1) {
        if (!zz9k_archive_7z_read_number(&cursor, &type)) {
          goto out;
        }
        if (type == ZZ9K_ARCHIVE_7Z_ID_END) {
          break;
        }
        if (!zz9k_archive_7z_skip_property(&cursor)) {
          goto out;
        }
      }
      continue;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_MAIN_STREAMS_INFO) {
      if (!zz9k_archive_7z_parse_streams_info(&cursor, archive_length,
                                              &streams)) {
        goto out;
      }
      continue;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_FILES_INFO) {
      uint32_t count;

      if (!zz9k_archive_7z_parse_files_info(
              &cursor, &streams, entries, entry_capacity, &count)) {
        goto out;
      }
      if (!zz9k_archive_7z_read_number(&cursor, &type) ||
          type != ZZ9K_ARCHIVE_7Z_ID_END) {
        goto out;
      }
      *entry_count = count;
      ok = 1;
      goto out;
    }
    if (type == ZZ9K_ARCHIVE_7Z_ID_ADDITIONAL_STREAMS_INFO ||
        type == ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER) {
      goto out;
    }
    goto out;
  }

out:
  zz9k_archive_7z_streams_free(&streams);
  return ok;
}

static int zz9k_archive_7z_list(const uint8_t *data,
                                uint32_t length,
                                ZZ9KArchiveEntry *entries,
                                uint32_t entry_capacity,
                                uint32_t *entry_count)
{
  ZZ9KArchive7zHeader header;
  const uint8_t *header_data;
  uint32_t header_length;
  uint8_t *decoded_header = 0;
  int ok;

  if (!zz9k_archive_7z_start_header(data, length, &header)) {
    return 0;
  }
  header_data = data + header.next_header_offset;
  header_length = header.next_header_size;
  if (header_length != 0U &&
      header_data[0] == ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER) {
    if (!zz9k_archive_7z_copy_encoded_header(
            header_data, header_length, data, length,
            &decoded_header, &header_length)) {
      return 0;
    }
    header_data = decoded_header;
  }
  ok = zz9k_archive_7z_list_from_header(
      header_data, header_length, length, entries, entry_capacity,
      entry_count);
  free(decoded_header);
  return ok;
}

static int zz9k_archive_7z_read_header_from_file(const char *path,
                                                 uint32_t archive_length,
                                                 uint8_t **header_data,
                                                 uint32_t *header_length)
{
  uint8_t start[ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE];
  ZZ9KArchive7zHeader header;
  FILE *file = 0;
  uint8_t *bytes = 0;
  int ok = 0;

  if (!path || !header_data || !header_length) {
    return 0;
  }
  *header_data = 0;
  *header_length = 0U;

  file = fopen(path, "rb");
  if (!file) {
    printf("open failed: %s\n", path);
    return 0;
  }
  if (fread(start, 1U, sizeof(start), file) != sizeof(start)) {
    printf("7z start header read failed: %s\n", path);
    goto out;
  }
  if (!zz9k_archive_7z_start_header_from_prefix(
          start, sizeof(start), archive_length, &header)) {
    goto out;
  }
  bytes = (uint8_t *)malloc((size_t)header.next_header_size);
  if (!bytes) {
    printf("7z header allocation failed: %lu bytes\n",
           (unsigned long)header.next_header_size);
    goto out;
  }
  if (fseek(file, (long)header.next_header_offset, SEEK_SET) != 0) {
    printf("7z header seek failed: %s\n", path);
    goto out;
  }
  if (fread(bytes, 1U, header.next_header_size, file) !=
      header.next_header_size) {
    printf("7z header read failed: %s\n", path);
    goto out;
  }
  if (zz9k_archive_crc32(0U, bytes, header.next_header_size) !=
      header.next_header_crc) {
    printf("7z header crc mismatch: %s\n", path);
    goto out;
  }
  if (header.next_header_size != 0U &&
      bytes[0] == ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER) {
    uint8_t *decoded = 0;
    uint32_t decoded_size = 0U;

    if (!zz9k_archive_7z_copy_encoded_header_from_file(
            bytes, header.next_header_size, path, archive_length,
            &decoded, &decoded_size)) {
      goto out;
    }
    free(bytes);
    bytes = decoded;
    header.next_header_size = decoded_size;
  }
  *header_data = bytes;
  *header_length = header.next_header_size;
  bytes = 0;
  ok = 1;

out:
  free(bytes);
  if (file) {
    fclose(file);
  }
  return ok;
}

static int zz9k_archive_find_eocd(const uint8_t *data, uint32_t length,
                                  uint32_t *eocd_offset)
{
  uint32_t min;
  uint32_t pos;

  if (!data || length < 22U || !eocd_offset) {
    return 0;
  }
  min = length > (22U + 65535U) ? length - (22U + 65535U) : 0U;
  pos = length - 22U;
  while (1) {
    if (zz9k_archive_get_le32(data + pos) == 0x06054b50UL) {
      uint32_t comment_len = zz9k_archive_get_le16(data + pos + 20U);
      if (pos + 22U + comment_len == length) {
        *eocd_offset = pos;
        return 1;
      }
    }
    if (pos == min) {
      break;
    }
    pos--;
  }
  return 0;
}

static int zz9k_archive_zip_u64_to_supported_u32(uint64_t value,
                                                 uint32_t *out)
{
  if (!out || value > ZZ9K_ARCHIVE_ZIP_SUPPORTED_U32_MAX) {
    return 0;
  }
  *out = (uint32_t)value;
  return 1;
}

static int zz9k_archive_zip_eocd_needs_zip64(const uint8_t *eocd)
{
  if (!eocd) {
    return 0;
  }
  return zz9k_archive_get_le16(eocd + 8U) == 0xffffU ||
         zz9k_archive_get_le16(eocd + 10U) == 0xffffU ||
         zz9k_archive_get_le32(eocd + 12U) == ZZ9K_ARCHIVE_ZIP_U32_SENTINEL ||
         zz9k_archive_get_le32(eocd + 16U) == ZZ9K_ARCHIVE_ZIP_U32_SENTINEL;
}

static int zz9k_archive_zip_parse_zip64_eocd_record(
    const uint8_t *record,
    uint32_t *total_entries,
    uint32_t *cd_size,
    uint32_t *cd_offset)
{
  uint64_t entries_on_disk;
  uint64_t entries_total;

  if (!record || !total_entries || !cd_size || !cd_offset ||
      zz9k_archive_get_le32(record) != 0x06064b50UL ||
      zz9k_archive_get_le64(record + 4U) < 44U ||
      zz9k_archive_get_le32(record + 16U) != 0U ||
      zz9k_archive_get_le32(record + 20U) != 0U) {
    return 0;
  }
  entries_on_disk = zz9k_archive_get_le64(record + 24U);
  entries_total = zz9k_archive_get_le64(record + 32U);
  if (entries_on_disk != entries_total) {
    return 0;
  }
  return zz9k_archive_zip_u64_to_supported_u32(
             entries_total, total_entries) &&
         zz9k_archive_zip_u64_to_supported_u32(
             zz9k_archive_get_le64(record + 40U), cd_size) &&
         zz9k_archive_zip_u64_to_supported_u32(
             zz9k_archive_get_le64(record + 48U), cd_offset);
}

static int zz9k_archive_zip_read_eocd(
    const uint8_t *data,
    uint32_t length,
    uint32_t eocd,
    uint32_t *total_entries,
    uint32_t *cd_size,
    uint32_t *cd_offset)
{
  const uint8_t *eocd_data;

  if (!data || !total_entries || !cd_size || !cd_offset ||
      eocd > length || length - eocd < 22U) {
    return 0;
  }
  eocd_data = data + eocd;
  if (zz9k_archive_get_le16(eocd_data + 4U) != 0U ||
      zz9k_archive_get_le16(eocd_data + 6U) != 0U) {
    return 0;
  }
  if (!zz9k_archive_zip_eocd_needs_zip64(eocd_data)) {
    *total_entries = zz9k_archive_get_le16(eocd_data + 10U);
    if (*total_entries != zz9k_archive_get_le16(eocd_data + 8U)) {
      return 0;
    }
    *cd_size = zz9k_archive_get_le32(eocd_data + 12U);
    *cd_offset = zz9k_archive_get_le32(eocd_data + 16U);
    return 1;
  }
  if (eocd < 20U ||
      zz9k_archive_get_le32(data + eocd - 20U) != 0x07064b50UL ||
      zz9k_archive_get_le32(data + eocd - 16U) != 0U ||
      zz9k_archive_get_le32(data + eocd - 4U) != 1U) {
    return 0;
  } else {
    uint64_t zip64_eocd_offset64;
    uint32_t zip64_eocd_offset;
    uint64_t record_size;

    zip64_eocd_offset64 = zz9k_archive_get_le64(data + eocd - 12U);
    if (!zz9k_archive_zip_u64_to_supported_u32(
            zip64_eocd_offset64, &zip64_eocd_offset) ||
        zip64_eocd_offset > length ||
        length - zip64_eocd_offset < 56U) {
      return 0;
    }
    record_size = zz9k_archive_get_le64(data + zip64_eocd_offset + 4U);
    if (record_size > (uint64_t)(length - zip64_eocd_offset - 12U)) {
      return 0;
    }
    return zz9k_archive_zip_parse_zip64_eocd_record(
        data + zip64_eocd_offset, total_entries, cd_size, cd_offset);
  }
}

static int zz9k_archive_zip_read_eocd_from_file(
    const char *path,
    uint32_t archive_length,
    const uint8_t *tail,
    uint32_t tail_length,
    uint32_t eocd,
    uint32_t *total_entries,
    uint32_t *cd_size,
    uint32_t *cd_offset)
{
  const uint8_t *eocd_data;
  uint8_t *record = 0;
  uint32_t zip64_eocd_offset;
  uint64_t zip64_eocd_offset64;
  uint64_t record_size;
  int ok;

  if (!path || !tail || !total_entries || !cd_size || !cd_offset ||
      eocd > tail_length || tail_length - eocd < 22U) {
    return 0;
  }
  eocd_data = tail + eocd;
  if (zz9k_archive_get_le16(eocd_data + 4U) != 0U ||
      zz9k_archive_get_le16(eocd_data + 6U) != 0U) {
    return 0;
  }
  if (!zz9k_archive_zip_eocd_needs_zip64(eocd_data)) {
    *total_entries = zz9k_archive_get_le16(eocd_data + 10U);
    if (*total_entries != zz9k_archive_get_le16(eocd_data + 8U)) {
      return 0;
    }
    *cd_size = zz9k_archive_get_le32(eocd_data + 12U);
    *cd_offset = zz9k_archive_get_le32(eocd_data + 16U);
    return 1;
  }
  if (eocd < 20U ||
      zz9k_archive_get_le32(tail + eocd - 20U) != 0x07064b50UL ||
      zz9k_archive_get_le32(tail + eocd - 16U) != 0U ||
      zz9k_archive_get_le32(tail + eocd - 4U) != 1U) {
    return 0;
  }
  zip64_eocd_offset64 = zz9k_archive_get_le64(tail + eocd - 12U);
  if (!zz9k_archive_zip_u64_to_supported_u32(
          zip64_eocd_offset64, &zip64_eocd_offset) ||
      zip64_eocd_offset > archive_length ||
      archive_length - zip64_eocd_offset < 56U) {
    return 0;
  }
  if (!zz9k_archive_read_file_range(
          path, zip64_eocd_offset, 56U, &record)) {
    return 0;
  }
  record_size = zz9k_archive_get_le64(record + 4U);
  ok = record_size <=
       (uint64_t)(archive_length - zip64_eocd_offset - 12U);
  if (ok) {
    ok = zz9k_archive_zip_parse_zip64_eocd_record(
        record, total_entries, cd_size, cd_offset);
  }
  free(record);
  return ok;
}

static int zz9k_archive_zip_external_attrs_is_dir(uint32_t attrs)
{
  uint32_t unix_mode = (attrs >> 16) & 0170000U;

  return (attrs & 0x10U) != 0U || unix_mode == 0040000U;
}

static int zz9k_archive_zip_read_zip64_u32(const uint8_t *extra,
                                           uint32_t extra_len,
                                           uint32_t *pos,
                                           uint32_t *value)
{
  uint64_t v;

  if (!extra || !pos || !value || *pos > extra_len ||
      extra_len - *pos < 8U) {
    return 0;
  }
  v = zz9k_archive_get_le64(extra + *pos);
  if (!zz9k_archive_zip_u64_to_supported_u32(v, value)) {
    return 0;
  }
  *pos += 8U;
  return 1;
}

static int zz9k_archive_zip_parse_zip64_extra(
    const uint8_t *extra,
    uint32_t extra_len,
    uint32_t *compressed_size,
    uint32_t *uncompressed_size,
    uint32_t *local_offset)
{
  uint32_t pos = 0U;
  int need_uncompressed;
  int need_compressed;
  int need_local_offset;

  if (!compressed_size || !uncompressed_size) {
    return 0;
  }
  need_compressed =
      *compressed_size == ZZ9K_ARCHIVE_ZIP_U32_SENTINEL ? 1 : 0;
  need_uncompressed =
      *uncompressed_size == ZZ9K_ARCHIVE_ZIP_U32_SENTINEL ? 1 : 0;
  need_local_offset =
      local_offset && *local_offset == ZZ9K_ARCHIVE_ZIP_U32_SENTINEL ? 1 : 0;
  if (!need_compressed && !need_uncompressed && !need_local_offset) {
    return 1;
  }
  if (!extra && extra_len != 0U) {
    return 0;
  }

  while (pos + 4U <= extra_len) {
    uint32_t header_id = zz9k_archive_get_le16(extra + pos);
    uint32_t data_size = zz9k_archive_get_le16(extra + pos + 2U);
    uint32_t data_pos;

    pos += 4U;
    if (data_size > extra_len - pos) {
      return 0;
    }
    if (header_id != ZZ9K_ARCHIVE_ZIP_EXTRA_ZIP64) {
      pos += data_size;
      continue;
    }
    data_pos = 0U;
    if (need_uncompressed &&
        !zz9k_archive_zip_read_zip64_u32(
            extra + pos, data_size, &data_pos, uncompressed_size)) {
      return 0;
    }
    if (need_compressed &&
        !zz9k_archive_zip_read_zip64_u32(
            extra + pos, data_size, &data_pos, compressed_size)) {
      return 0;
    }
    if (need_local_offset &&
        !zz9k_archive_zip_read_zip64_u32(
            extra + pos, data_size, &data_pos, local_offset)) {
      return 0;
    }
    return 1;
  }
  return 0;
}

static int zz9k_archive_zip_data_offset_checked(
    const uint8_t *data,
    uint32_t length,
    uint32_t local_offset,
    const uint8_t *expected_name,
    uint32_t expected_name_len,
    uint32_t expected_flags,
    uint32_t expected_method,
    uint32_t expected_crc32,
    uint32_t expected_compressed_size,
    uint32_t expected_uncompressed_size,
    uint32_t *data_offset)
{
  uint32_t name_len;
  uint32_t extra_len;
  uint32_t flags;
  uint32_t method;
  uint32_t offset;
  uint32_t local_compressed_size;
  uint32_t local_uncompressed_size;

  if (!data || !data_offset ||
      local_offset > length ||
      length - local_offset < 30U ||
      zz9k_archive_get_le32(data + local_offset) != 0x04034b50UL) {
    return 0;
  }
  flags = zz9k_archive_get_le16(data + local_offset + 6U);
  method = zz9k_archive_get_le16(data + local_offset + 8U);
  name_len = zz9k_archive_get_le16(data + local_offset + 26U);
  extra_len = zz9k_archive_get_le16(data + local_offset + 28U);
  offset = local_offset + 30U;
  if (name_len > length - offset ||
      extra_len > length - offset - name_len) {
    return 0;
  }
  offset += name_len + extra_len;
  if (
      flags != expected_flags ||
      method != expected_method ||
      name_len != expected_name_len ||
      !expected_name ||
      memcmp(data + local_offset + 30U, expected_name, name_len) != 0) {
    return 0;
  }
  if ((flags & 0x0008U) == 0U) {
    local_compressed_size = zz9k_archive_get_le32(data + local_offset + 18U);
    local_uncompressed_size = zz9k_archive_get_le32(data + local_offset + 22U);
    if (!zz9k_archive_zip_parse_zip64_extra(
            data + local_offset + 30U + name_len, extra_len,
            &local_compressed_size, &local_uncompressed_size, 0)) {
      return 0;
    }
    if (zz9k_archive_get_le32(data + local_offset + 14U) != expected_crc32 ||
        local_compressed_size != expected_compressed_size ||
        local_uncompressed_size != expected_uncompressed_size) {
      return 0;
    }
  }
  *data_offset = offset;
  return 1;
}

static int zz9k_archive_zip_list(const uint8_t *data, uint32_t length,
                                 ZZ9KArchiveEntry *entries,
                                 uint32_t entry_capacity,
                                 uint32_t *entry_count)
{
  uint32_t eocd;
  uint32_t total_entries;
  uint32_t cd_size;
  uint32_t cd_offset;
  uint32_t pos;
  uint32_t count = 0U;
  uint32_t i;

  if (!data || !entries || !entry_count ||
      !zz9k_archive_find_eocd(data, length, &eocd)) {
    return 0;
  }
  if (!zz9k_archive_zip_read_eocd(
          data, length, eocd, &total_entries, &cd_size, &cd_offset)) {
    return 0;
  }
  if (cd_offset > length || cd_size > length - cd_offset) {
    return 0;
  }
  if (total_entries == 0U) {
    *entry_count = 0U;
    return cd_size == 0U;
  }

  pos = cd_offset;
  for (i = 0U; i < total_entries; i++) {
    uint32_t flags;
    uint32_t method;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t name_len;
    uint32_t extra_len;
    uint32_t comment_len;
    uint32_t local_offset;
    uint32_t external_attrs;
    uint32_t data_offset;
    uint32_t record_length;
    ZZ9KArchiveEntry candidate;
    ZZ9KArchiveEntry *entry;

    if (pos + 46U > length ||
        zz9k_archive_get_le32(data + pos) != 0x02014b50UL) {
      return 0;
    }
    flags = zz9k_archive_get_le16(data + pos + 8U);
    method = zz9k_archive_get_le16(data + pos + 10U);
    compressed_size = zz9k_archive_get_le32(data + pos + 20U);
    uncompressed_size = zz9k_archive_get_le32(data + pos + 24U);
    name_len = zz9k_archive_get_le16(data + pos + 28U);
    extra_len = zz9k_archive_get_le16(data + pos + 30U);
    comment_len = zz9k_archive_get_le16(data + pos + 32U);
    external_attrs = zz9k_archive_get_le32(data + pos + 38U);
    local_offset = zz9k_archive_get_le32(data + pos + 42U);
    if (name_len == 0U || pos + 46U + name_len + extra_len + comment_len >
        length) {
      return 0;
    }
    if (!zz9k_archive_zip_parse_zip64_extra(
            data + pos + 46U + name_len, extra_len,
            &compressed_size, &uncompressed_size, &local_offset)) {
      return 0;
    }
    if (!zz9k_archive_zip_data_offset_checked(
            data, length, local_offset, data + pos + 46U, name_len,
            flags, method, zz9k_archive_get_le32(data + pos + 16U),
            compressed_size, uncompressed_size, &data_offset)) {
      return 0;
    }
    if (compressed_size > length || data_offset > length ||
        data_offset + compressed_size > length) {
      return 0;
    }

    record_length = 46U + name_len + extra_len + comment_len;
    memset(&candidate, 0, sizeof(candidate));
    if (!zz9k_archive_copy_zip_name(candidate.name, sizeof(candidate.name),
                                    data + pos + 46U, name_len)) {
      return 0;
    }
    candidate.method = method;
    candidate.flags = flags;
    candidate.crc32 = zz9k_archive_get_le32(data + pos + 16U);
    candidate.data_offset = data_offset;
    candidate.compressed_size = compressed_size;
    candidate.uncompressed_size = uncompressed_size;
    candidate.is_dir =
        zz9k_archive_name_ends_with_slash(candidate.name) ||
        zz9k_archive_zip_external_attrs_is_dir(external_attrs) ? 1U : 0U;
    pos += record_length;
    if (zz9k_archive_zip_entry_is_root_metadata(&candidate)) {
      continue;
    }
    if (count >= entry_capacity) {
      return 0;
    }
    entry = &entries[count++];
    *entry = candidate;
  }

  *entry_count = count;
  return 1;
}

static int zz9k_archive_zip_data_offset_from_file_checked(
    const char *path,
    uint32_t archive_length,
    uint32_t local_offset,
    const uint8_t *expected_name,
    uint32_t expected_name_len,
    uint32_t expected_flags,
    uint32_t expected_method,
    uint32_t expected_crc32,
    uint32_t expected_compressed_size,
    uint32_t expected_uncompressed_size,
    uint32_t *data_offset)
{
  uint8_t local[30];
  uint8_t *name_extra = 0;
  FILE *file;
  uint32_t name_len;
  uint32_t extra_len;
  uint32_t name_extra_len;
  uint32_t flags;
  uint32_t method;
  uint32_t offset;
  uint32_t local_compressed_size;
  uint32_t local_uncompressed_size;
  int ok = 0;

  if (!path || !data_offset ||
      local_offset > archive_length ||
      archive_length - local_offset < sizeof(local) ||
      local_offset > 0x7fffffffUL) {
    return 0;
  }
  file = fopen(path, "rb");
  if (!file) {
    printf("open failed: %s\n", path);
    return 0;
  }
  if (fseek(file, (long)local_offset, SEEK_SET) != 0 ||
      fread(local, 1U, sizeof(local), file) != sizeof(local)) {
    fclose(file);
    return 0;
  }
  fclose(file);

  if (zz9k_archive_get_le32(local) != 0x04034b50UL) {
    return 0;
  }
  flags = zz9k_archive_get_le16(local + 6U);
  method = zz9k_archive_get_le16(local + 8U);
  name_len = zz9k_archive_get_le16(local + 26U);
  extra_len = zz9k_archive_get_le16(local + 28U);
  offset = local_offset + 30U + name_len + extra_len;
  if (name_len > archive_length - local_offset - 30U ||
      extra_len > archive_length - local_offset - 30U - name_len ||
      offset < local_offset ||
      offset > archive_length) {
    return 0;
  }
  if (flags != expected_flags ||
      method != expected_method ||
      name_len != expected_name_len ||
      !expected_name) {
    return 0;
  }
  if (extra_len > UINT32_MAX - name_len) {
    return 0;
  }
  name_extra_len = name_len + extra_len;
  if (!zz9k_archive_read_file_range(
          path, local_offset + 30U, name_extra_len, &name_extra)) {
    return 0;
  }
  ok = memcmp(name_extra, expected_name, name_len) == 0;
  if (ok && (flags & 0x0008U) == 0U) {
    local_compressed_size = zz9k_archive_get_le32(local + 18U);
    local_uncompressed_size = zz9k_archive_get_le32(local + 22U);
    ok = zz9k_archive_zip_parse_zip64_extra(
        name_extra + name_len, extra_len,
        &local_compressed_size, &local_uncompressed_size, 0);
    if (ok) {
      ok = zz9k_archive_get_le32(local + 14U) == expected_crc32 &&
           local_compressed_size == expected_compressed_size &&
           local_uncompressed_size == expected_uncompressed_size;
    }
  }
  free(name_extra);
  if (!ok) {
    return 0;
  }
  *data_offset = offset;
  return 1;
}

static int zz9k_archive_zip_read_directory_from_file(
    const char *path,
    uint32_t archive_length,
    uint8_t **directory,
    uint32_t *directory_length,
    uint32_t *entry_count)
{
  FILE *file = 0;
  uint8_t *tail = 0;
  uint8_t *central = 0;
  uint32_t tail_length;
  uint32_t tail_offset;
  uint32_t eocd;
  uint32_t total_entries;
  uint32_t cd_size;
  uint32_t cd_offset;
  int ok = 0;

  if (!path || !directory || !directory_length || !entry_count ||
      archive_length < 22U) {
    return 0;
  }
  *directory = 0;
  *directory_length = 0U;
  *entry_count = 0U;

  tail_length = archive_length;
  if (tail_length > 22U + 65535U) {
    tail_length = 22U + 65535U;
  }
  tail_offset = archive_length - tail_length;
  if (tail_offset > 0x7fffffffUL) {
    return 0;
  }

  file = fopen(path, "rb");
  if (!file) {
    printf("open failed: %s\n", path);
    return 0;
  }
  tail = (uint8_t *)malloc((size_t)tail_length);
  if (!tail) {
    printf("zip tail allocation failed\n");
    goto out;
  }
  if (fseek(file, (long)tail_offset, SEEK_SET) != 0 ||
      fread(tail, 1U, tail_length, file) != tail_length) {
    printf("zip tail read failed: %s\n", path);
    goto out;
  }
  if (!zz9k_archive_find_eocd(tail, tail_length, &eocd)) {
    goto out;
  }
  if (!zz9k_archive_zip_read_eocd_from_file(
          path, archive_length, tail, tail_length, eocd,
          &total_entries, &cd_size, &cd_offset)) {
    goto out;
  }
  if (total_entries == 0U && cd_size == 0U) {
    *directory = 0;
    *directory_length = 0U;
    *entry_count = 0U;
    ok = 1;
    goto out;
  }
  if (total_entries == 0U || cd_size == 0U ||
      cd_offset > archive_length ||
      cd_size > archive_length - cd_offset ||
      cd_offset > 0x7fffffffUL) {
    goto out;
  }
  central = (uint8_t *)malloc((size_t)cd_size);
  if (!central) {
    printf("zip directory allocation failed: %lu bytes\n",
           (unsigned long)cd_size);
    goto out;
  }
  if (fseek(file, (long)cd_offset, SEEK_SET) != 0 ||
      fread(central, 1U, cd_size, file) != cd_size) {
    printf("zip directory read failed: %s\n", path);
    goto out;
  }

  *directory = central;
  *directory_length = cd_size;
  *entry_count = total_entries;
  central = 0;
  ok = 1;

out:
  free(central);
  free(tail);
  if (file) {
    fclose(file);
  }
  return ok;
}

static int zz9k_archive_zip_list_from_directory(
    const char *path,
    const uint8_t *directory,
    uint32_t directory_length,
    uint32_t archive_length,
    ZZ9KArchiveEntry *entries,
    uint32_t entry_capacity,
    uint32_t expected_entries,
    uint32_t *entry_count)
{
  uint32_t pos = 0U;
  uint32_t count = 0U;
  uint32_t i;

  if (!path || !entries || !entry_count ||
      (expected_entries != 0U && !directory)) {
    return 0;
  }
  if (expected_entries == 0U) {
    *entry_count = 0U;
    return directory_length == 0U;
  }

  for (i = 0U; i < expected_entries; i++) {
    uint32_t flags;
    uint32_t method;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint32_t name_len;
    uint32_t extra_len;
    uint32_t comment_len;
    uint32_t local_offset;
    uint32_t external_attrs;
    uint32_t data_offset;
    uint32_t record_length;
    ZZ9KArchiveEntry candidate;
    ZZ9KArchiveEntry *entry;

    if (pos + 46U > directory_length ||
        zz9k_archive_get_le32(directory + pos) != 0x02014b50UL) {
      return 0;
    }
    flags = zz9k_archive_get_le16(directory + pos + 8U);
    method = zz9k_archive_get_le16(directory + pos + 10U);
    compressed_size = zz9k_archive_get_le32(directory + pos + 20U);
    uncompressed_size = zz9k_archive_get_le32(directory + pos + 24U);
    name_len = zz9k_archive_get_le16(directory + pos + 28U);
    extra_len = zz9k_archive_get_le16(directory + pos + 30U);
    comment_len = zz9k_archive_get_le16(directory + pos + 32U);
    external_attrs = zz9k_archive_get_le32(directory + pos + 38U);
    local_offset = zz9k_archive_get_le32(directory + pos + 42U);
    if (name_len == 0U ||
        pos + 46U + name_len + extra_len + comment_len >
          directory_length) {
      return 0;
    }
    if (!zz9k_archive_zip_parse_zip64_extra(
            directory + pos + 46U + name_len, extra_len,
            &compressed_size, &uncompressed_size, &local_offset)) {
      return 0;
    }
    if (!zz9k_archive_zip_data_offset_from_file_checked(
            path, archive_length, local_offset, directory + pos + 46U,
            name_len, flags, method,
            zz9k_archive_get_le32(directory + pos + 16U),
            compressed_size, uncompressed_size, &data_offset)) {
      return 0;
    }
    if (compressed_size > archive_length ||
        data_offset > archive_length ||
        compressed_size > archive_length - data_offset) {
      return 0;
    }

    record_length = 46U + name_len + extra_len + comment_len;
    memset(&candidate, 0, sizeof(candidate));
    if (!zz9k_archive_copy_zip_name(candidate.name, sizeof(candidate.name),
                                    directory + pos + 46U, name_len)) {
      return 0;
    }
    candidate.method = method;
    candidate.flags = flags;
    candidate.crc32 = zz9k_archive_get_le32(directory + pos + 16U);
    candidate.data_offset = data_offset;
    candidate.compressed_size = compressed_size;
    candidate.uncompressed_size = uncompressed_size;
    candidate.is_dir =
        zz9k_archive_name_ends_with_slash(candidate.name) ||
        zz9k_archive_zip_external_attrs_is_dir(external_attrs) ? 1U : 0U;
    pos += record_length;
    if (zz9k_archive_zip_entry_is_root_metadata(&candidate)) {
      continue;
    }
    if (count >= entry_capacity) {
      return 0;
    }
    entry = &entries[count++];
    *entry = candidate;
  }

  *entry_count = count;
  while (pos < directory_length) {
    uint32_t extra_size;

    if (pos + 6U > directory_length ||
        zz9k_archive_get_le32(directory + pos) != 0x05054b50UL) {
      return 0;
    }
    extra_size = zz9k_archive_get_le16(directory + pos + 4U);
    if (extra_size > directory_length - pos - 6U) {
      return 0;
    }
    pos += 6U + extra_size;
  }
  return 1;
}

static int zz9k_archive_zip_list_file(const char *path,
                                      uint32_t archive_length,
                                      ZZ9KArchiveEntry *entries,
                                      uint32_t entry_capacity,
                                      uint32_t *entry_count)
{
  uint8_t *directory = 0;
  uint32_t directory_length = 0U;
  uint32_t count = 0U;
  int ok;

  if (!entry_count) {
    return 0;
  }
  if (!zz9k_archive_zip_read_directory_from_file(
          path, archive_length, &directory, &directory_length, &count)) {
    return 0;
  }
  ok = zz9k_archive_zip_list_from_directory(
      path, directory, directory_length, archive_length,
      entries, entry_capacity, count, entry_count);
  free(directory);
  return ok;
}

static int zz9k_archive_tar_name(const uint8_t *header, char *name,
                                 uint32_t capacity)
{
  uint32_t prefix_len;
  uint32_t name_len;

  name_len = 0U;
  while (name_len < 100U && header[name_len] != 0U) {
    name_len++;
  }
  prefix_len = 0U;
  while (prefix_len < 155U && header[345U + prefix_len] != 0U) {
    prefix_len++;
  }
  if (name_len == 0U) {
    return 0;
  }
  if (prefix_len != 0U) {
    if (prefix_len + 1U + name_len >= capacity) {
      return 0;
    }
    memcpy(name, header + 345U, prefix_len);
    name[prefix_len] = '/';
    memcpy(name + prefix_len + 1U, header, name_len);
    name[prefix_len + 1U + name_len] = '\0';
  } else {
    if (!zz9k_archive_copy_name(name, capacity, header, name_len)) {
      return 0;
    }
  }
  return 1;
}

static int zz9k_archive_tar_normalize_name(char *name, int *skip)
{
  size_t len;

  if (!name || !skip) {
    return 0;
  }
  *skip = 0;
  while (name[0] == '.' && name[1] == '/') {
    len = strlen(name + 2U);
    memmove(name, name + 2U, len + 1U);
  }
  zz9k_archive_strip_current_dir_prefix(name);
  if (name[0] == '\0' || strcmp(name, ".") == 0) {
    name[0] = '\0';
    *skip = 1;
  }
  return 1;
}

static int zz9k_archive_tar_copy_data_name(const uint8_t *data,
                                           uint32_t length,
                                           char *name,
                                           uint32_t capacity,
                                           int *skip)
{
  uint32_t name_len = 0U;

  if (!data || !name || !skip || capacity == 0U || length == 0U) {
    return 0;
  }
  *skip = 0;
  while (name_len < length && data[name_len] != 0U &&
         data[name_len] != (uint8_t)'\n') {
    name_len++;
  }
  if (name_len == 0U || name_len >= capacity) {
    return 0;
  }
  memcpy(name, data, name_len);
  name[name_len] = '\0';
  return zz9k_archive_tar_normalize_name(name, skip);
}

static int zz9k_archive_parse_decimal(const uint8_t *field,
                                      uint32_t length,
                                      uint32_t *value)
{
  uint32_t i;
  uint32_t out = 0U;

  if (!field || !value || length == 0U) {
    return 0;
  }
  for (i = 0U; i < length; i++) {
    uint8_t c = field[i];

    if (c < (uint8_t)'0' || c > (uint8_t)'9' ||
        out > 0x7fffffffUL / 10U) {
      return 0;
    }
    out *= 10U;
    if ((uint32_t)(c - (uint8_t)'0') > 0x7fffffffUL - out) {
      return 0;
    }
    out += (uint32_t)(c - (uint8_t)'0');
  }
  *value = out;
  return 1;
}

static int zz9k_archive_tar_parse_pax_info(const uint8_t *data,
                                           uint32_t length,
                                           ZZ9KArchiveTarPaxInfo *info)
{
  uint32_t pos = 0U;

  if (!data || !info) {
    return 0;
  }
  memset(info, 0, sizeof(*info));
  while (pos < length) {
    uint32_t record_len = 0U;
    uint32_t digits = 0U;
    uint32_t body_pos;
    uint32_t body_len;
    uint32_t value_len;
    uint32_t eq_pos;

    while (pos + digits < length &&
           data[pos + digits] >= (uint8_t)'0' &&
           data[pos + digits] <= (uint8_t)'9') {
      if (record_len > 99999U) {
        return 0;
      }
      record_len = record_len * 10U +
          (uint32_t)(data[pos + digits] - (uint8_t)'0');
      digits++;
    }
    if (digits == 0U || pos + digits >= length ||
        data[pos + digits] != (uint8_t)' ' ||
        record_len <= digits + 1U || pos + record_len > length) {
      return 0;
    }
    body_pos = pos + digits + 1U;
    body_len = record_len - digits - 1U;
    if (body_len == 0U || data[body_pos + body_len - 1U] != (uint8_t)'\n') {
      return 0;
    }
    value_len = body_len - 1U;
    eq_pos = 0U;
    while (eq_pos < value_len && data[body_pos + eq_pos] != (uint8_t)'=') {
      eq_pos++;
    }
    if (eq_pos == 4U && memcmp(data + body_pos, "path", 4U) == 0 &&
        eq_pos + 1U < value_len) {
      uint32_t path_len = value_len - eq_pos - 1U;
      int skip = 0;

      if (path_len >= sizeof(info->path)) {
        return 0;
      }
      memcpy(info->path, data + body_pos + eq_pos + 1U, path_len);
      info->path[path_len] = '\0';
      if (!zz9k_archive_tar_normalize_name(info->path, &skip)) {
        return 0;
      }
      info->has_path = 1;
      info->path_skip = skip;
    } else if (eq_pos == 4U &&
               memcmp(data + body_pos, "size", 4U) == 0 &&
               eq_pos + 1U < value_len) {
      uint32_t size_len = value_len - eq_pos - 1U;

      if (!zz9k_archive_parse_decimal(
              data + body_pos + eq_pos + 1U, size_len, &info->size)) {
        return 0;
      }
      info->has_size = 1;
    }
    pos += record_len;
  }
  return 1;
}

static int zz9k_archive_tar_parse_pax_path(const uint8_t *data,
                                           uint32_t length,
                                           char *name,
                                           uint32_t capacity)
{
  ZZ9KArchiveTarPaxInfo info;

  if (!name || capacity == 0U ||
      !zz9k_archive_tar_parse_pax_info(data, length, &info)) {
    return 0;
  }
  if (info.has_path) {
    if (strlen(info.path) >= capacity) {
      return 0;
    }
    strcpy(name, info.path);
  }
  return 1;
}

static int zz9k_archive_parse_octal(const uint8_t *field, uint32_t length,
                                    uint32_t *value)
{
  uint32_t i;
  uint32_t out = 0U;
  int saw_digit = 0;

  if (!field || !value) {
    return 0;
  }
  for (i = 0U; i < length; i++) {
    uint8_t c = field[i];
    if (c == 0U || c == (uint8_t)' ') {
      break;
    }
    if (c < (uint8_t)'0' || c > (uint8_t)'7') {
      return 0;
    }
    if (out > 0x1fffffffUL) {
      return 0;
    }
    out = (out << 3) | (uint32_t)(c - (uint8_t)'0');
    saw_digit = 1;
  }
  if (!saw_digit) {
    return 0;
  }
  *value = out;
  return 1;
}

static int zz9k_archive_parse_base256(const uint8_t *field, uint32_t length,
                                      uint32_t *value)
{
  uint32_t i;
  uint32_t out;

  if (!field || !value || length == 0U ||
      (field[0] & 0x80U) == 0U ||
      (field[0] & 0x40U) != 0U) {
    return 0;
  }
  out = (uint32_t)(field[0] & 0x3fU);
  for (i = 1U; i < length; i++) {
    if (out > 0x7fffffffUL >> 8) {
      return 0;
    }
    out = (out << 8) | (uint32_t)field[i];
  }
  if (out > 0x7fffffffUL) {
    return 0;
  }
  *value = out;
  return 1;
}

static int zz9k_archive_parse_tar_number(const uint8_t *field,
                                         uint32_t length,
                                         uint32_t *value)
{
  if (!field || length == 0U) {
    return 0;
  }
  if ((field[0] & 0x80U) != 0U) {
    return zz9k_archive_parse_base256(field, length, value);
  }
  return zz9k_archive_parse_octal(field, length, value);
}

static int zz9k_archive_tar_header_checksum_valid(const uint8_t *header)
{
  uint32_t stored;
  uint32_t sum = 0U;
  int32_t signed_sum = 0;
  uint32_t i;

  if (!header || !zz9k_archive_parse_octal(header + 148U, 8U, &stored)) {
    return 0;
  }
  for (i = 0U; i < 512U; i++) {
    if (i >= 148U && i < 156U) {
      sum += (uint32_t)' ';
      signed_sum += (int32_t)' ';
    } else {
      sum += (uint32_t)header[i];
      signed_sum += (int32_t)(int8_t)header[i];
    }
  }
  return stored == sum || stored == (uint32_t)signed_sum;
}

static int zz9k_archive_tar_header_empty(const uint8_t *header)
{
  uint32_t i;

  for (i = 0U; i < 512U; i++) {
    if (header[i] != 0U) {
      return 0;
    }
  }
  return 1;
}

static int zz9k_archive_tar_entry_from_header(const uint8_t *header,
                                              uint32_t data_offset,
                                              ZZ9KArchiveEntry *entry)
{
  uint32_t size;
  uint8_t typeflag;
  int skip = 0;

  if (!header || !entry ||
      !zz9k_archive_tar_header_checksum_valid(header) ||
      !zz9k_archive_tar_name(header, entry->name, sizeof(entry->name)) ||
      !zz9k_archive_parse_tar_number(header + 124U, 12U, &size) ||
      !zz9k_archive_tar_normalize_name(entry->name, &skip)) {
    return 0;
  }
  typeflag = header[156U];
  entry->method = ZZ9K_ARCHIVE_TAR_METHOD_STORE;
  entry->flags = skip ? ZZ9K_ARCHIVE_TAR_FLAG_SKIP : 0U;
  entry->data_offset = data_offset;
  entry->compressed_size = size;
  entry->uncompressed_size = size;
  entry->is_dir =
      typeflag == (uint8_t)'5' ||
      zz9k_archive_name_ends_with_slash(entry->name) ? 1U : 0U;
  if (typeflag == (uint8_t)'L') {
    entry->flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP |
                    ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME;
  } else if (typeflag == (uint8_t)'K') {
    entry->flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP;
  } else if (typeflag == (uint8_t)'x') {
    entry->flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP |
                    ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER;
  } else if (typeflag == (uint8_t)'g') {
    entry->flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP;
  } else if (typeflag != 0U && typeflag != (uint8_t)'0' &&
             typeflag != (uint8_t)'5') {
    entry->flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP;
  }
  return 1;
}

static int zz9k_archive_tar_list(const uint8_t *data, uint32_t length,
                                 ZZ9KArchiveEntry *entries,
                                 uint32_t entry_capacity,
                                 uint32_t *entry_count)
{
  uint32_t pos = 0U;
  uint32_t count = 0U;
  uint32_t pending_size = 0U;
  char pending_name[ZZ9K_ARCHIVE_MAX_NAME];
  int pending_size_valid = 0;
  int pending_name_skip = 0;

  pending_name[0] = '\0';

  if (!data || !entries || !entry_count || (length % 512U) != 0U) {
    return 0;
  }
  while (pos + 512U <= length) {
    const uint8_t *header = data + pos;
    uint32_t size;
    uint32_t blocks;
    ZZ9KArchiveEntry candidate;
    ZZ9KArchiveEntry *entry;

    if (zz9k_archive_tar_header_empty(header)) {
      *entry_count = count;
      return 1;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (!zz9k_archive_tar_entry_from_header(
            header, pos + 512U, &candidate)) {
      return 0;
    }
    entry = &candidate;
    size = entry->uncompressed_size;
    if (entry->data_offset > length || size > length ||
        entry->data_offset + size > length) {
      return 0;
    }
    if ((entry->flags & ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME) != 0U) {
      int skip = 0;

      if (!zz9k_archive_tar_copy_data_name(
              data + entry->data_offset, size,
              pending_name, sizeof(pending_name), &skip)) {
        return 0;
      }
      pending_name_skip = skip;
    } else if ((entry->flags & ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER) != 0U) {
      ZZ9KArchiveTarPaxInfo pax;

      if (!zz9k_archive_tar_parse_pax_info(
              data + entry->data_offset, size, &pax)) {
        return 0;
      }
      if (pax.has_path) {
        if (pax.path_skip) {
          pending_name[0] = '\0';
          pending_name_skip = 1;
        } else {
          strcpy(pending_name, pax.path);
          pending_name_skip = 0;
        }
      }
      if (pax.has_size) {
        pending_size = pax.size;
        pending_size_valid = 1;
      }
    } else if (pending_name_skip) {
      entry->flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP;
      pending_name_skip = 0;
    } else if (pending_name[0] != '\0') {
      strcpy(entry->name, pending_name);
      pending_name[0] = '\0';
    }
    if ((entry->flags & (ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME |
                         ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER)) == 0U &&
        pending_size_valid) {
      entry->compressed_size = pending_size;
      entry->uncompressed_size = pending_size;
      size = pending_size;
      pending_size_valid = 0;
    }
    if (entry->data_offset > length || size > length ||
        entry->data_offset + size > length) {
      return 0;
    }
    if ((entry->flags & ZZ9K_ARCHIVE_TAR_FLAG_SKIP) == 0U) {
      if (count >= entry_capacity) {
        return 0;
      }
      entries[count] = candidate;
      count++;
    }
    blocks = (size + 511U) / 512U;
    pos += 512U + blocks * 512U;
  }
  *entry_count = count;
  return 1;
}

static int zz9k_archive_require_codec_service(ZZ9KContext *ctx,
                                              ZZ9KServiceInfo *service)
{
  ZZ9KCaps caps;
  int status;

  status = zz9k_query_caps(ctx, &caps);
  if (status != ZZ9K_STATUS_OK) {
    printf("query caps failed: %s (%d)\n", zz9k_status_name(status), status);
    return 0;
  }
  if ((caps.capability_bits & ZZ9K_CAP_COMPRESSION) == 0U) {
    printf("%s capability not advertised\n",
           zz9k_capability_name(ZZ9K_CAP_COMPRESSION));
    return 0;
  }
  status = zz9k_query_service(ctx, ZZ9K_SERVICE_CODEC, service);
  if (status != ZZ9K_STATUS_OK) {
    printf("query codec service failed: %s (%d)\n",
           zz9k_status_name(status), status);
    return 0;
  }
  return 1;
}

static int zz9k_archive_service_supports(const ZZ9KServiceInfo *service,
                                         uint32_t algorithm)
{
  uint32_t flags;

  flags = zz9k_compression_required_service_flags(algorithm);
  return service && flags != 0U && (service->flags & flags) == flags;
}

static int zz9k_archive_service_supports_decompress_test(
    const ZZ9KServiceInfo *service, uint32_t algorithm)
{
  return zz9k_archive_service_supports(service, algorithm) &&
         (service->flags & ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_TEST) != 0U;
}

static int zz9k_archive_service_supports_decompress_stream(
    const ZZ9KServiceInfo *service, uint32_t algorithm)
{
  return zz9k_archive_service_supports(service, algorithm) &&
         (service->flags & ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_STREAM) != 0U;
}

static int zz9k_archive_service_supports_decompress_feed(
    const ZZ9KServiceInfo *service, uint32_t algorithm)
{
  uint32_t flags;

  if (algorithm == ZZ9K_COMPRESSION_DEFLATE_RAW &&
      (!service ||
       (service->flags & ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_FEED) == 0U)) {
    return 0;
  }
  flags = zz9k_compression_required_feed_service_flags(algorithm);
  return zz9k_archive_service_supports_decompress_stream(service, algorithm) &&
         flags != 0U && (service->flags & flags) == flags;
}

static int zz9k_archive_ensure_codec_open(ZZ9KContext **ctx,
                                          ZZ9KServiceInfo *service,
                                          int *codec_ready)
{
  int status;

  if (!ctx || !service || !codec_ready) {
    return 0;
  }
  if (*codec_ready) {
    return 1;
  }
  status = zz9k_open(ctx);
  if (status != ZZ9K_STATUS_OK) {
    printf("open failed: %s (%d)\n", zz9k_status_name(status), status);
    return 0;
  }
  if (!zz9k_archive_require_codec_service(*ctx, service)) {
    return 0;
  }
  *codec_ready = 1;
  return 1;
}

static void zz9k_archive_print_shared_diag(ZZ9KContext *ctx,
                                           const char *label,
                                           uint32_t requested)
{
  ZZ9KDiagInfo diag;
  int status;

  if (!ctx) {
    return;
  }

  memset(&diag, 0, sizeof(diag));
  status = zz9k_read_diag(ctx, &diag);
  if (status != ZZ9K_STATUS_OK) {
    printf("Archive diag (%s): unavailable: %s (%d)\n",
           label, zz9k_status_name(status), status);
    return;
  }

  printf("Archive diag (%s): requested=%lu bytes\n",
         label, (unsigned long)requested);
  printf("  Last status:         %s (%lu)\n",
         zz9k_status_name((int)diag.last_status),
         (unsigned long)diag.last_status);
  printf("  Shared buffers used: %lu\n",
         (unsigned long)diag.shared_buffers_used);
  printf("  Shared heap free:    %lu bytes\n",
         (unsigned long)diag.shared_heap_free);
  printf("  Largest free block:  %lu bytes\n",
         (unsigned long)diag.shared_heap_largest_free);
}

static int zz9k_archive_decompress_to_memory(ZZ9KContext *ctx,
                                             const ZZ9KServiceInfo *service,
                                             uint32_t algorithm,
                                             const uint8_t *compressed,
                                             uint32_t compressed_length,
                                             uint32_t output_capacity,
                                             uint8_t **output,
                                             ZZ9KDecompressResult *result)
{
  ZZ9KSharedBuffer input;
  ZZ9KSharedBuffer decoded;
  ZZ9KDecompressDesc desc;
  uint8_t *bytes;
  int status;
  int ok = 0;

  memset(&input, 0, sizeof(input));
  memset(&decoded, 0, sizeof(decoded));
  *output = 0;

  if (!zz9k_archive_service_supports(service, algorithm)) {
    printf("%s not advertised by codec service\n",
           zz9k_compression_algorithm_text(algorithm));
    return 0;
  }
  if (compressed_length == 0U || output_capacity == 0U) {
    printf("unsupported empty codec job\n");
    return 0;
  }
  bytes = (uint8_t *)malloc((size_t)output_capacity);
  if (!bytes) {
    printf("output allocation failed\n");
    return 0;
  }

  status = zz9k_alloc_shared(ctx, compressed_length, 16U, 0U, &input);
  if (status != ZZ9K_STATUS_OK) {
    printf("alloc compressed failed: %s (%d), requested=%lu bytes\n",
           zz9k_status_name(status), status,
           (unsigned long)compressed_length);
    zz9k_archive_print_shared_diag(ctx, "compressed input",
                                   compressed_length);
    goto out;
  }
  status = zz9k_alloc_shared(ctx, output_capacity, 16U, 0U, &decoded);
  if (status != ZZ9K_STATUS_OK) {
    printf("alloc decoded failed: %s (%d), requested=%lu bytes\n",
           zz9k_status_name(status), status,
           (unsigned long)output_capacity);
    zz9k_archive_print_shared_diag(ctx, "decoded output",
                                   output_capacity);
    goto out;
  }
  if (!zz9k_shared_copy_to(&input, 0U, compressed, compressed_length)) {
    printf("compressed copy failed\n");
    goto out;
  }
  if (!zz9k_compression_build_decompress_desc(
          &desc, algorithm, input.handle, 0U, compressed_length,
          decoded.handle, 0U, output_capacity,
          ZZ9K_DECOMPRESS_FLAG_EXPECT_END)) {
    printf("could not build decompression descriptor\n");
    goto out;
  }
  memset(result, 0, sizeof(*result));
  status = zz9k_decompress(ctx, &desc, result);
  if (status != ZZ9K_STATUS_OK) {
    printf("%s decompress failed: %s (%d), input=%lu output=%lu\n",
           zz9k_compression_algorithm_text(algorithm),
           zz9k_status_name(status), status,
           (unsigned long)compressed_length,
           (unsigned long)output_capacity);
    zz9k_archive_print_shared_diag(ctx, "codec failure",
                                   output_capacity);
    goto out;
  }
  if (result->bytes_written > output_capacity ||
      !zz9k_shared_copy_from(bytes, &decoded, 0U, result->bytes_written)) {
    printf("decoded copy failed\n");
    goto out;
  }
  *output = bytes;
  bytes = 0;
  ok = 1;

out:
  if (decoded.handle != 0U && decoded.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, decoded.handle);
  }
  if (input.handle != 0U && input.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, input.handle);
  }
  free(bytes);
  return ok;
}

static void zz9k_archive_print_entry(const ZZ9KArchiveEntry *entry)
{
  printf("%c %10lu %s\n",
         entry->is_dir ? 'd' : '-',
         (unsigned long)entry->uncompressed_size,
         entry->name);
}

static int zz9k_archive_alloc_entries(uint32_t capacity,
                                      ZZ9KArchiveEntry **entries)
{
  uint32_t alloc_capacity;

  if (!entries || capacity > 65535U) {
    return 0;
  }
  alloc_capacity = capacity == 0U ? 1U : capacity;
  *entries = (ZZ9KArchiveEntry *)calloc((size_t)alloc_capacity,
                                        sizeof(ZZ9KArchiveEntry));
  return *entries != 0;
}

static int zz9k_archive_count_zip_entries(const uint8_t *data,
                                          uint32_t length,
                                          uint32_t *count)
{
  uint32_t eocd;
  uint32_t cd_size;
  uint32_t cd_offset;

  if (!zz9k_archive_find_eocd(data, length, &eocd) ||
      !zz9k_archive_zip_read_eocd(
          data, length, eocd, count, &cd_size, &cd_offset)) {
    return 0;
  }
  return 1;
}

static int zz9k_archive_count_tar_entries(const uint8_t *data,
                                          uint32_t length,
                                          uint32_t *count)
{
  uint32_t pos = 0U;
  uint32_t entries = 0U;
  uint32_t pending_size = 0U;
  int pending_size_valid = 0;
  int pending_name_skip = 0;

  if (!data || (length % 512U) != 0U) {
    return 0;
  }
  while (pos + 512U <= length) {
    uint32_t size;
    uint32_t blocks;
    ZZ9KArchiveEntry entry;

    if (zz9k_archive_tar_header_empty(data + pos)) {
      *count = entries;
      return 1;
    }
    memset(&entry, 0, sizeof(entry));
    if (!zz9k_archive_tar_entry_from_header(
            data + pos, pos + 512U, &entry)) {
      return 0;
    }
    size = entry.uncompressed_size;
    if (pos + 512U + size > length) {
      return 0;
    }
    if ((entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER) != 0U) {
      ZZ9KArchiveTarPaxInfo pax;

      if (!zz9k_archive_tar_parse_pax_info(
              data + entry.data_offset, size, &pax)) {
        return 0;
      }
      if (pax.has_size) {
        pending_size = pax.size;
        pending_size_valid = 1;
      }
      if (pax.has_path) {
        pending_name_skip = pax.path_skip;
      }
    } else if ((entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME) != 0U) {
      char scratch[ZZ9K_ARCHIVE_MAX_NAME];
      int skip = 0;

      if (!zz9k_archive_tar_copy_data_name(
              data + entry.data_offset, size,
              scratch, sizeof(scratch), &skip)) {
        return 0;
      }
      pending_name_skip = skip;
    } else if ((entry.flags & (ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME |
                               ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER)) == 0U) {
      if (pending_name_skip) {
        entry.flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP;
        pending_name_skip = 0;
      }
      if (pending_size_valid) {
        size = pending_size;
        pending_size_valid = 0;
        if (pos + 512U + size > length) {
          return 0;
        }
      }
    }
    if ((entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_SKIP) == 0U) {
      entries++;
    }
    blocks = (size + 511U) / 512U;
    pos += 512U + blocks * 512U;
  }
  *count = entries;
  return 1;
}

static int zz9k_archive_write_entry(const char *output_dir,
                                    const ZZ9KArchiveEntry *entry,
                                    const uint8_t *data)
{
  char *path;
  ZZ9KArchiveEntry output_entry;
  int ok;

  if (!zz9k_archive_output_entry(entry, &output_entry)) {
    return 1;
  }
  if (!zz9k_archive_path_is_safe(output_entry.name)) {
    printf("unsafe path rejected: %s\n", entry->name);
    return 0;
  }
  path = zz9k_archive_join_path(output_dir, output_entry.name);
  if (!path) {
    printf("path allocation failed\n");
    return 0;
  }
  if (output_entry.is_dir) {
    zz9k_archive_trim_trailing_separators(path);
  }
  if (output_entry.is_dir && zz9k_archive_path_exists(path) &&
      !zz9k_archive_path_is_dir(path)) {
    printf("output path is a file: %s\n", path);
    free(path);
    return 0;
  }
  if (!output_entry.is_dir && zz9k_archive_path_is_dir(path)) {
    printf("output path is a directory: %s\n", path);
    free(path);
    return 0;
  }
  if (!output_entry.is_dir && zz9k_archive_skip_existing_outputs &&
      zz9k_archive_path_exists(path)) {
    printf("s %s\n", path);
    zz9k_archive_last_output_skipped = 1;
    free(path);
    return 1;
  }
  if (!output_entry.is_dir && !zz9k_archive_overwrite_outputs &&
      zz9k_archive_path_exists(path)) {
    printf("output exists, use --overwrite: %s\n", path);
    free(path);
    return 0;
  }
  if (zz9k_archive_dry_run_outputs) {
    printf("dry %s\n", output_entry.name);
    zz9k_archive_last_output_skipped = 0;
    zz9k_archive_last_output_dry_run = 1;
    free(path);
    return 1;
  }
  if (!zz9k_archive_ensure_parent_dirs(output_dir, output_entry.name)) {
    printf("could not create parent directories for %s\n", output_entry.name);
    free(path);
    return 0;
  }
  if (output_entry.is_dir) {
    ok = zz9k_archive_mkdir_one(path);
  } else {
    ok = zz9k_archive_write_file(path, data, output_entry.uncompressed_size);
  }
  if (ok && !zz9k_archive_last_output_skipped &&
      !zz9k_archive_last_output_dry_run) {
    printf("x %s\n", output_entry.name);
  }
  free(path);
  return ok;
}

static int zz9k_archive_write_file_range_entry(
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    const char *input_path)
{
  FILE *input = 0;
  FILE *output = 0;
  char *path = 0;
  ZZ9KArchiveEntry output_entry;
  uint8_t *chunk = 0;
  uint32_t remaining;
  int ok = 0;

  if (!entry || !input_path || entry->is_dir ||
      entry->compressed_size != entry->uncompressed_size ||
      entry->data_offset > 0x7fffffffUL) {
    return 0;
  }
  zz9k_archive_last_output_skipped = 0;
  zz9k_archive_last_output_dry_run = 0;
  if (!zz9k_archive_output_entry(entry, &output_entry)) {
    return 1;
  }
  if (!zz9k_archive_path_is_safe(output_entry.name)) {
    printf("unsafe path rejected: %s\n", entry->name);
    return 0;
  }
  path = zz9k_archive_join_path(output_dir, output_entry.name);
  if (!path) {
    printf("path allocation failed\n");
    return 0;
  }
  if (zz9k_archive_path_is_dir(path)) {
    printf("output path is a directory: %s\n", path);
    goto out;
  }
  if (zz9k_archive_skip_existing_outputs && zz9k_archive_path_exists(path)) {
    printf("s %s\n", path);
    zz9k_archive_last_output_skipped = 1;
    ok = 1;
    goto out;
  }
  if (!zz9k_archive_overwrite_outputs && zz9k_archive_path_exists(path)) {
    printf("output exists, use --overwrite: %s\n", path);
    goto out;
  }
  if (zz9k_archive_dry_run_outputs) {
    printf("dry %s\n", output_entry.name);
    zz9k_archive_last_output_dry_run = 1;
    ok = 1;
    goto out;
  }
  if (!zz9k_archive_ensure_parent_dirs(output_dir, output_entry.name)) {
    printf("could not create parent directories for %s\n", output_entry.name);
    goto out;
  }
  input = fopen(input_path, "rb");
  if (!input) {
    printf("open failed: %s\n", input_path);
    goto out;
  }
  if (fseek(input, (long)entry->data_offset, SEEK_SET) != 0) {
    printf("file range seek failed: %s\n", input_path);
    goto out;
  }
  output = fopen(path, "wb");
  if (!output) {
    printf("open output failed: %s\n", path);
    goto out;
  }
  chunk = (uint8_t *)malloc(ZZ9K_ARCHIVE_STREAM_CHUNK);
  if (!chunk) {
    printf("file range chunk allocation failed\n");
    goto out;
  }

  remaining = entry->compressed_size;
  while (remaining != 0U) {
    uint32_t part = remaining > ZZ9K_ARCHIVE_STREAM_CHUNK ?
        ZZ9K_ARCHIVE_STREAM_CHUNK : remaining;

    if (fread(chunk, 1U, part, input) != part) {
      printf("file range read failed: %s\n", input_path);
      goto out;
    }
    if (fwrite(chunk, 1U, part, output) != part) {
      printf("file range write failed: %s\n", output_entry.name);
      goto out;
    }
    remaining -= part;
  }

  ok = 1;

out:
  if (output && fclose(output) != 0) {
    ok = 0;
  }
  if (input) {
    fclose(input);
  }
  if (ok && !zz9k_archive_last_output_skipped &&
      !zz9k_archive_last_output_dry_run) {
    printf("x %s\n", output_entry.name);
  }
  free(chunk);
  free(path);
  return ok;
}

static int zz9k_archive_lha_method_supported(uint32_t method)
{
  return method == ZZ9K_ARCHIVE_LHA_METHOD_LH5 ||
         method == ZZ9K_ARCHIVE_LHA_METHOD_LH1 ||
         method == ZZ9K_ARCHIVE_LHA_METHOD_LH6 ||
         method == ZZ9K_ARCHIVE_LHA_METHOD_LH7;
}

static int zz9k_archive_lha_decode_method_to_file(
    const uint8_t *data,
    uint32_t length,
    const ZZ9KArchiveEntry *entry,
    FILE *output)
{
  FILE *input;
  uint16_t decoded_crc = 0U;
  int ok;

  if (!data || !entry || entry->data_offset > length ||
      entry->compressed_size > length - entry->data_offset ||
      !zz9k_archive_lha_method_supported(entry->method)) {
    return 0;
  }
  input = tmpfile();
  if (!input) {
    printf("lha temporary input failed: %s\n", entry->name);
    return 0;
  }
  if (entry->compressed_size != 0U &&
      fwrite(data + entry->data_offset, 1U, entry->compressed_size, input) !=
      entry->compressed_size) {
    fclose(input);
    printf("lha temporary input write failed: %s\n", entry->name);
    return 0;
  }
  if (fseek(input, 0L, SEEK_SET) != 0) {
    fclose(input);
    printf("lha temporary input seek failed: %s\n", entry->name);
    return 0;
  }
  ok = zz9k_lha_unix_decode_method(
      input, output, entry->uncompressed_size, entry->compressed_size,
      (uint16_t)entry->crc32,
      (entry->flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) != 0U,
      &decoded_crc,
      (int)entry->method);
  fclose(input);
  if (!ok) {
    printf("lha lh%lu decode failed: %s crc=0x%04x expected=0x%04lx\n",
           (unsigned long)entry->method, entry->name,
           (unsigned int)decoded_crc, (unsigned long)entry->crc32);
  }
  return ok;
}

static int zz9k_archive_lha_decode_lh5_to_file(const uint8_t *data,
                                               uint32_t length,
                                               const ZZ9KArchiveEntry *entry,
                                               FILE *output)
{
  return zz9k_archive_lha_decode_method_to_file(data, length, entry, output);
}

static int zz9k_archive_extract_lha_lh5(const uint8_t *data,
                                        uint32_t length,
                                        const char *output_dir,
                                        const ZZ9KArchiveEntry *entry)
{
  FILE *file = 0;
  int ok;

  if (!zz9k_archive_open_output_entry(output_dir, entry, &file)) {
    return 0;
  }
  ok = zz9k_archive_lha_decode_method_to_file(data, length, entry, file);
  if (fclose(file) != 0) {
    ok = 0;
  }
  if (ok && !zz9k_archive_last_output_skipped &&
      !zz9k_archive_last_output_dry_run) {
    printf("x %s\n", entry->name);
  }
  return ok;
}

static int zz9k_archive_handle_tar(ZZ9KContext *ctx,
                                   const ZZ9KServiceInfo *service,
                                   const uint8_t *data,
                                   uint32_t length,
                                   const char *command,
                                   const char *output_dir)
{
  ZZ9KArchiveEntry *entries;
  uint32_t count;
  uint32_t i;
  int ok = 1;

  (void)ctx;
  (void)service;
  if (!zz9k_archive_count_tar_entries(data, length, &count) ||
      !zz9k_archive_alloc_entries(count, &entries)) {
    printf("tar parse failed\n");
    return 0;
  }
  if (!zz9k_archive_tar_list(data, length, entries, count, &count)) {
    printf("tar parse failed\n");
    free(entries);
    return 0;
  }

  for (i = 0U; i < count; i++) {
    if (!zz9k_archive_entry_matches_filter(&entries[i])) {
      continue;
    }
    if (strcmp(command, "l") == 0) {
      zz9k_archive_print_entry(&entries[i]);
    } else if (strcmp(command, "t") == 0) {
      if (!zz9k_archive_path_is_safe(entries[i].name)) {
        printf("unsafe path rejected: %s\n", entries[i].name);
        ok = 0;
      }
    } else {
      ok &= zz9k_archive_write_entry(output_dir, &entries[i],
                                     data + entries[i].data_offset);
    }
  }
  if (strcmp(command, "t") == 0 && ok) {
    printf("tar test ok: %lu entries\n", (unsigned long)count);
  }
  free(entries);
  return ok;
}

static int zz9k_archive_decompress_test_to_result(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *compressed,
    uint32_t compressed_length,
    uint32_t output_limit,
    ZZ9KDecompressResult *result)
{
  ZZ9KSharedBuffer input;
  ZZ9KDecompressTestDesc desc;
  int status;
  int ok = 0;

  memset(&input, 0, sizeof(input));
  memset(result, 0, sizeof(*result));

  if (!zz9k_archive_service_supports_decompress_test(service, algorithm)) {
    printf("%s streamed test path not advertised by codec service\n",
           zz9k_compression_algorithm_text(algorithm));
    return 0;
  }
  if (compressed_length == 0U || output_limit == 0U) {
    printf("unsupported empty codec test job\n");
    return 0;
  }

  status = zz9k_alloc_shared(ctx, compressed_length, 16U, 0U, &input);
  if (status != ZZ9K_STATUS_OK) {
    printf("alloc compressed failed: %s (%d), requested=%lu bytes\n",
           zz9k_status_name(status), status,
           (unsigned long)compressed_length);
    zz9k_archive_print_shared_diag(ctx, "compressed input",
                                   compressed_length);
    goto out;
  }
  if (!zz9k_shared_copy_to(&input, 0U, compressed, compressed_length)) {
    printf("compressed copy failed\n");
    goto out;
  }
  if (!zz9k_compression_build_decompress_test_desc(
          &desc, algorithm, input.handle, 0U, compressed_length,
          output_limit, ZZ9K_DECOMPRESS_FLAG_EXPECT_END)) {
    printf("could not build decompression test descriptor\n");
    goto out;
  }

  status = zz9k_decompress_test(ctx, &desc, result);
  if (status != ZZ9K_STATUS_OK) {
    printf("%s streamed test failed: %s (%d), input=%lu limit=%lu\n",
           zz9k_compression_algorithm_text(algorithm),
           zz9k_status_name(status), status,
           (unsigned long)compressed_length,
           (unsigned long)output_limit);
    zz9k_archive_print_shared_diag(ctx, "codec test failure",
                                   compressed_length);
    goto out;
  }
  ok = 1;

out:
  if (input.handle != 0U && input.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, input.handle);
  }
  return ok;
}

static int zz9k_archive_open_output_entry(const char *output_dir,
                                          const ZZ9KArchiveEntry *entry,
                                          FILE **file)
{
  char *path;
  ZZ9KArchiveEntry output_entry;

  *file = 0;
  zz9k_archive_last_output_skipped = 0;
  zz9k_archive_last_output_dry_run = 0;
  if (!zz9k_archive_output_entry(entry, &output_entry)) {
    *file = zz9k_archive_open_discard_file();
    zz9k_archive_last_output_skipped = 1;
    return *file != 0;
  }
  if (!zz9k_archive_path_is_safe(output_entry.name)) {
    printf("unsafe path rejected: %s\n", entry->name);
    return 0;
  }
  path = zz9k_archive_join_path(output_dir, output_entry.name);
  if (!path) {
    printf("path allocation failed\n");
    return 0;
  }
  if (zz9k_archive_path_is_dir(path)) {
    printf("output path is a directory: %s\n", path);
    free(path);
    return 0;
  }
  if (zz9k_archive_skip_existing_outputs && zz9k_archive_path_exists(path)) {
    printf("s %s\n", path);
    zz9k_archive_last_output_skipped = 1;
    *file = zz9k_archive_open_discard_file();
    free(path);
    return *file != 0;
  }
  if (!zz9k_archive_overwrite_outputs && zz9k_archive_path_exists(path)) {
    printf("output exists, use --overwrite: %s\n", path);
    free(path);
    return 0;
  }
  if (zz9k_archive_dry_run_outputs) {
    printf("dry %s\n", output_entry.name);
    zz9k_archive_last_output_dry_run = 1;
    *file = zz9k_archive_open_discard_file();
    free(path);
    return *file != 0;
  }
  if (!zz9k_archive_ensure_parent_dirs(output_dir, output_entry.name)) {
    printf("could not create parent directories for %s\n", output_entry.name);
    free(path);
    return 0;
  }
  *file = fopen(path, "wb");
  if (!*file) {
    printf("open output failed: %s\n", path);
    free(path);
    return 0;
  }
  free(path);
  return 1;
}

static void zz9k_archive_tar_stream_init(ZZ9KArchiveTarStream *stream,
                                         const char *command,
                                         const char *output_dir)
{
  memset(stream, 0, sizeof(*stream));
  stream->command = command;
  stream->output_dir = output_dir;
  stream->ok = 1;
}

static void zz9k_archive_tar_stream_cleanup(ZZ9KArchiveTarStream *stream)
{
  if (stream && stream->file) {
    fclose(stream->file);
    stream->file = 0;
  }
  if (stream) {
    free(stream->pax_data);
    stream->pax_data = 0;
    stream->pax_capacity = 0U;
  }
}

static int zz9k_archive_tar_stream_prepare_pax(
    ZZ9KArchiveTarStream *stream,
    uint32_t size)
{
  uint8_t *bytes;
  uint32_t capacity = size == 0U ? 1U : size;

  if (!stream || size > ZZ9K_ARCHIVE_MAX_PAX_DATA) {
    return 0;
  }
  if (stream->pax_capacity >= capacity) {
    return 1;
  }
  bytes = (uint8_t *)malloc((size_t)capacity);
  if (!bytes) {
    return 0;
  }
  free(stream->pax_data);
  stream->pax_data = bytes;
  stream->pax_capacity = capacity;
  return 1;
}

static int zz9k_archive_tar_stream_close_file(ZZ9KArchiveTarStream *stream)
{
  if (!stream || !stream->file) {
    return 1;
  }
  if (fclose(stream->file) != 0) {
    stream->file = 0;
    stream->ok = 0;
    return 0;
  }
  stream->file = 0;
  if (!zz9k_archive_last_output_skipped &&
      !zz9k_archive_last_output_dry_run) {
    printf("x %s\n", stream->entry.name);
  }
  return 1;
}

static int zz9k_archive_tar_stream_start_entry(
    ZZ9KArchiveTarStream *stream)
{
  uint32_t size;

  if (!stream || !stream->ok) {
    return 0;
  }
  if (zz9k_archive_tar_header_empty(stream->header)) {
    stream->done = 1;
    stream->header_used = 0U;
    return 1;
  }
  memset(&stream->entry, 0, sizeof(stream->entry));
  if (!zz9k_archive_tar_entry_from_header(
          stream->header, 0U, &stream->entry)) {
    stream->ok = 0;
    return 0;
  }
  if ((stream->entry.flags &
       (ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME |
        ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER)) == 0U &&
      stream->pending_name_skip) {
    stream->entry.flags |= ZZ9K_ARCHIVE_TAR_FLAG_SKIP;
    stream->pending_name_skip = 0;
    stream->pending_name[0] = '\0';
  } else if ((stream->entry.flags &
       (ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME |
        ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER)) == 0U &&
      stream->pending_name[0] != '\0') {
    strcpy(stream->entry.name, stream->pending_name);
    stream->pending_name[0] = '\0';
  }
  if ((stream->entry.flags &
       (ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME |
        ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER)) == 0U &&
      stream->pending_size_valid) {
    stream->entry.compressed_size = stream->pending_size;
    stream->entry.uncompressed_size = stream->pending_size;
    stream->pending_size_valid = 0;
  }
  size = stream->entry.uncompressed_size;
  stream->entry_remaining = size;
  stream->pax_used = 0U;
  stream->padding_remaining = (ZZ9K_ARCHIVE_TAR_BLOCK -
      (size % ZZ9K_ARCHIVE_TAR_BLOCK)) % ZZ9K_ARCHIVE_TAR_BLOCK;
  stream->header_used = 0U;
  if ((stream->entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER) != 0U &&
      !zz9k_archive_tar_stream_prepare_pax(stream, size)) {
    stream->ok = 0;
    return 0;
  }
  if ((stream->entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_SKIP) != 0U) {
    return 1;
  }
  if (!zz9k_archive_entry_matches_filter(&stream->entry)) {
    return 1;
  }
  if (!zz9k_archive_path_is_safe(stream->entry.name)) {
    printf("unsafe path rejected: %s\n", stream->entry.name);
    stream->ok = 0;
    return 0;
  }
  stream->count++;

  if (strcmp(stream->command, "l") == 0) {
    zz9k_archive_print_entry(&stream->entry);
    return 1;
  }
  if (strcmp(stream->command, "t") == 0) {
    return 1;
  }
  if (strcmp(stream->command, "x") != 0) {
    stream->ok = 0;
    return 0;
  }

  if (stream->entry.is_dir || size == 0U) {
    if (!zz9k_archive_write_entry(stream->output_dir, &stream->entry, 0)) {
      stream->ok = 0;
      return 0;
    }
    return 1;
  }
  if (!zz9k_archive_open_output_entry(
          stream->output_dir, &stream->entry, &stream->file)) {
    stream->ok = 0;
    return 0;
  }
  return 1;
}

static int zz9k_archive_tar_stream_consume(ZZ9KArchiveTarStream *stream,
                                           const uint8_t *data,
                                           uint32_t length)
{
  uint32_t pos = 0U;

  if (!stream || !data || !stream->ok) {
    return 0;
  }
  while (pos < length) {
    uint32_t part;

    if (stream->done) {
      return 1;
    }
    if (stream->entry_remaining != 0U) {
      part = stream->entry_remaining;
      if (part > length - pos) {
        part = length - pos;
      }
      if ((stream->entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME) != 0U) {
        uint32_t used = (uint32_t)strlen(stream->pending_name);
        uint32_t copy_len = part;

        if (used + copy_len >= sizeof(stream->pending_name)) {
          copy_len = (uint32_t)sizeof(stream->pending_name) - 1U - used;
        }
        if (copy_len != 0U) {
          memcpy(stream->pending_name + used, data + pos, copy_len);
          stream->pending_name[used + copy_len] = '\0';
        }
      } else if ((stream->entry.flags &
                  ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER) != 0U) {
        if (!stream->pax_data ||
            stream->pax_used + part > stream->pax_capacity) {
          stream->ok = 0;
          return 0;
        }
        memcpy(stream->pax_data + stream->pax_used, data + pos, part);
        stream->pax_used += part;
      } else if (stream->file &&
          fwrite(data + pos, 1U, part, stream->file) != part) {
        printf("tar stream write failed: %s\n", stream->entry.name);
        stream->ok = 0;
        return 0;
      }
      stream->entry_remaining -= part;
      pos += part;
      if (stream->entry_remaining == 0U &&
          (stream->entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_GNU_LONG_NAME) != 0U) {
        int skip = 0;

        if (!zz9k_archive_tar_normalize_name(
                stream->pending_name, &skip)) {
          stream->ok = 0;
          return 0;
        }
        stream->pending_name_skip = skip;
      } else if (stream->entry_remaining == 0U &&
          (stream->entry.flags & ZZ9K_ARCHIVE_TAR_FLAG_PAX_HEADER) != 0U) {
        ZZ9KArchiveTarPaxInfo pax;

        if (!zz9k_archive_tar_parse_pax_info(
                stream->pax_data, stream->pax_used, &pax)) {
          stream->ok = 0;
          return 0;
        }
        if (pax.has_path) {
          if (pax.path_skip) {
            stream->pending_name[0] = '\0';
            stream->pending_name_skip = 1;
          } else {
            strcpy(stream->pending_name, pax.path);
            stream->pending_name_skip = 0;
          }
        }
        if (pax.has_size) {
          stream->pending_size = pax.size;
          stream->pending_size_valid = 1;
        }
      }
      if (stream->entry_remaining == 0U &&
          !zz9k_archive_tar_stream_close_file(stream)) {
        return 0;
      }
      continue;
    }
    if (stream->padding_remaining != 0U) {
      part = stream->padding_remaining;
      if (part > length - pos) {
        part = length - pos;
      }
      stream->padding_remaining -= part;
      pos += part;
      continue;
    }

    part = ZZ9K_ARCHIVE_TAR_BLOCK - stream->header_used;
    if (part > length - pos) {
      part = length - pos;
    }
    memcpy(stream->header + stream->header_used, data + pos, part);
    stream->header_used += part;
    pos += part;
    if (stream->header_used == ZZ9K_ARCHIVE_TAR_BLOCK &&
        !zz9k_archive_tar_stream_start_entry(stream)) {
      return 0;
    }
  }
  return 1;
}

static int zz9k_archive_tar_stream_finish(ZZ9KArchiveTarStream *stream)
{
  if (!stream || !stream->ok) {
    return 0;
  }
  if (stream->file || stream->entry_remaining != 0U ||
      stream->padding_remaining != 0U || stream->header_used != 0U) {
    zz9k_archive_tar_stream_cleanup(stream);
    return 0;
  }
  return 1;
}

static int zz9k_archive_tar_stream_chunk(void *user,
                                         const uint8_t *data,
                                         uint32_t length)
{
  return zz9k_archive_tar_stream_consume(
      (ZZ9KArchiveTarStream *)user, data, length);
}

static int zz9k_archive_decompress_stream_to_file(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *compressed,
    uint32_t compressed_length,
    uint32_t output_limit,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    ZZ9KDecompressResult *final_result)
{
  ZZ9KSharedBuffer input;
  ZZ9KSharedBuffer decoded;
  ZZ9KDecompressStreamBeginDesc begin_desc;
  ZZ9KDecompressStreamReadDesc read_desc;
  ZZ9KDecompressStreamResult stream_result;
  FILE *file = 0;
  uint8_t *chunk = 0;
  uint32_t chunk_capacity = ZZ9K_ARCHIVE_STREAM_CHUNK;
  uint32_t total_written = 0U;
  uint32_t session = 0U;
  int status;
  int ok = 0;

  memset(&input, 0, sizeof(input));
  memset(&decoded, 0, sizeof(decoded));
  memset(final_result, 0, sizeof(*final_result));

  if (!zz9k_archive_service_supports_decompress_stream(service, algorithm)) {
    printf("%s decompress-stream not advertised by codec service\n",
           zz9k_compression_algorithm_text(algorithm));
    return 0;
  }
  if (compressed_length == 0U || output_limit == 0U || !entry) {
    printf("unsupported empty codec stream job\n");
    return 0;
  }

  status = zz9k_alloc_shared(ctx, compressed_length, 16U, 0U, &input);
  if (status != ZZ9K_STATUS_OK) {
    printf("alloc compressed failed: %s (%d), requested=%lu bytes\n",
           zz9k_status_name(status), status,
           (unsigned long)compressed_length);
    zz9k_archive_print_shared_diag(ctx, "compressed input",
                                   compressed_length);
    goto out;
  }
  while (chunk_capacity >= ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
    status = zz9k_alloc_shared(ctx, chunk_capacity, 16U, 0U, &decoded);
    if (status == ZZ9K_STATUS_OK) {
      break;
    }
    if (status != ZZ9K_STATUS_NO_MEMORY ||
        chunk_capacity == ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
      printf("alloc stream output failed: %s (%d), requested=%lu bytes\n",
             zz9k_status_name(status), status,
             (unsigned long)chunk_capacity);
      zz9k_archive_print_shared_diag(ctx, "stream output",
                                     chunk_capacity);
      goto out;
    }
    chunk_capacity /= 2U;
  }
  chunk = (uint8_t *)malloc((size_t)chunk_capacity);
  if (!chunk) {
    printf("stream chunk allocation failed\n");
    goto out;
  }
  if (!zz9k_shared_copy_to(&input, 0U, compressed, compressed_length)) {
    printf("compressed copy failed\n");
    goto out;
  }
  if (!zz9k_compression_build_decompress_stream_begin_desc(
          &begin_desc, algorithm, input.handle, 0U, compressed_length,
          output_limit, ZZ9K_DECOMPRESS_FLAG_EXPECT_END)) {
    printf("could not build decompression stream begin descriptor\n");
    goto out;
  }

  status = zz9k_decompress_stream_begin(ctx, &begin_desc, &stream_result);
  if (status != ZZ9K_STATUS_OK) {
    printf("%s stream begin failed: %s (%d), input=%lu limit=%lu\n",
           zz9k_compression_algorithm_text(algorithm),
           zz9k_status_name(status), status,
           (unsigned long)compressed_length,
           (unsigned long)output_limit);
    zz9k_archive_print_shared_diag(ctx, "stream begin failure",
                                   compressed_length);
    goto out;
  }
  session = stream_result.session;
  if (!zz9k_archive_open_output_entry(output_dir, entry, &file)) {
    goto out;
  }

  while (1) {
    if (!zz9k_compression_build_decompress_stream_read_desc(
            &read_desc, session, decoded.handle, 0U, decoded.length, 0U)) {
      printf("could not build decompression stream read descriptor\n");
      goto out;
    }
    memset(&stream_result, 0, sizeof(stream_result));
    status = zz9k_decompress_stream_read(ctx, &read_desc, &stream_result);
    if (status != ZZ9K_STATUS_OK) {
      printf("%s stream read failed: %s (%d), written=%lu\n",
             zz9k_compression_algorithm_text(algorithm),
             zz9k_status_name(status), status,
             (unsigned long)total_written);
      zz9k_archive_print_shared_diag(ctx, "stream read failure",
                                     decoded.length);
      session = 0U;
      goto out;
    }
    if (stream_result.bytes_written > decoded.length ||
        total_written > output_limit ||
        stream_result.bytes_written > output_limit - total_written) {
      printf("%s stream output exceeded limit\n",
             zz9k_compression_algorithm_text(algorithm));
      goto out;
    }
    if (stream_result.bytes_written != 0U) {
      if (!zz9k_shared_copy_from(chunk, &decoded, 0U,
                                 stream_result.bytes_written)) {
        printf("stream copy failed\n");
        goto out;
      }
      if (fwrite(chunk, 1U, stream_result.bytes_written, file) !=
          stream_result.bytes_written) {
        printf("stream output write failed: %s\n", entry->name);
        goto out;
      }
      total_written += stream_result.bytes_written;
    }
    if ((stream_result.flags & ZZ9K_DECOMPRESS_RESULT_STREAM_END) != 0U) {
      final_result->bytes_consumed = stream_result.bytes_consumed;
      final_result->bytes_written = total_written;
      final_result->checksum = stream_result.checksum;
      final_result->algorithm = stream_result.algorithm;
      final_result->flags = stream_result.flags;
      ok = 1;
      break;
    }
    if (stream_result.bytes_written == 0U) {
      printf("%s stream made no output progress\n",
             zz9k_compression_algorithm_text(algorithm));
      goto out;
    }
  }

out:
  if (session != 0U) {
    zz9k_decompress_stream_close(ctx, session, 0U);
  }
  if (file) {
    if (fclose(file) != 0) {
      ok = 0;
    }
  }
  if (ok && !zz9k_archive_last_output_skipped &&
      !zz9k_archive_last_output_dry_run) {
    printf("x %s\n", entry->name);
  }
  if (decoded.handle != 0U && decoded.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, decoded.handle);
  }
  if (input.handle != 0U && input.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, input.handle);
  }
  free(chunk);
  return ok;
}

static int zz9k_archive_copy_feed_input_chunk(
    ZZ9KSharedBuffer *input,
    const uint8_t *prefix,
    uint32_t prefix_length,
    const uint8_t *compressed,
    uint32_t compressed_length,
    uint32_t input_offset,
    uint32_t max_length,
    uint32_t *copied)
{
  uint32_t total_length;
  uint32_t remaining;
  uint32_t written = 0U;

  if (!input || !copied || max_length == 0U ||
      prefix_length > 0xffffffffUL - compressed_length) {
    return 0;
  }
  if ((prefix_length != 0U && !prefix) ||
      (compressed_length != 0U && !compressed)) {
    return 0;
  }

  total_length = prefix_length + compressed_length;
  if (input_offset >= total_length) {
    return 0;
  }
  remaining = total_length - input_offset;
  if (remaining > max_length) {
    remaining = max_length;
  }
  if (remaining > input->length) {
    return 0;
  }

  if (input_offset < prefix_length) {
    uint32_t prefix_remaining = prefix_length - input_offset;
    uint32_t part = prefix_remaining < remaining ?
        prefix_remaining : remaining;

    if (!zz9k_shared_copy_to(input, written,
                             prefix + input_offset, part)) {
      return 0;
    }
    written += part;
    remaining -= part;
  }

  if (remaining != 0U) {
    uint32_t compressed_offset = input_offset + written - prefix_length;

    if (!zz9k_shared_copy_to(input, written,
                             compressed + compressed_offset,
                             remaining)) {
      return 0;
    }
    written += remaining;
  }

  *copied = written;
  return written != 0U;
}

static int zz9k_archive_copy_feed_file_input_chunk(
    ZZ9KSharedBuffer *input,
    FILE *input_file,
    const uint8_t *prefix,
    uint32_t prefix_length,
    uint32_t file_length,
    uint32_t input_offset,
    uint32_t max_length,
    uint32_t *copied)
{
  uint32_t total_length;
  uint32_t remaining;
  uint32_t written = 0U;

  if (!input || !input_file || !copied || max_length == 0U ||
      prefix_length > 0xffffffffUL - file_length) {
    return 0;
  }
  if (prefix_length != 0U && !prefix) {
    return 0;
  }

  total_length = prefix_length + file_length;
  if (input_offset >= total_length) {
    return 0;
  }
  remaining = total_length - input_offset;
  if (remaining > max_length) {
    remaining = max_length;
  }
  if (remaining > input->length) {
    return 0;
  }

  if (input_offset < prefix_length) {
    uint32_t prefix_remaining = prefix_length - input_offset;
    uint32_t part = prefix_remaining < remaining ?
        prefix_remaining : remaining;

    if (!zz9k_shared_copy_to(input, written,
                             prefix + input_offset, part)) {
      return 0;
    }
    written += part;
    remaining -= part;
  }

  if (remaining != 0U) {
    uint8_t *dst = (uint8_t *)(void *)input->data;

    if (fread(dst + written, 1U, (size_t)remaining, input_file) !=
        (size_t)remaining) {
      return 0;
    }
    written += remaining;
  }

  *copied = written;
  return written != 0U;
}

static int zz9k_archive_decompress_feed_stream_parts_to_file(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *prefix,
    uint32_t prefix_length,
    const uint8_t *compressed,
    uint32_t compressed_length,
    uint32_t output_limit,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    ZZ9KDecompressResult *final_result)
{
  ZZ9KSharedBuffer input;
  ZZ9KSharedBuffer decoded;
  ZZ9KDecompressStreamBeginDesc begin_desc;
  ZZ9KDecompressStreamFeedDesc feed_desc;
  ZZ9KDecompressStreamReadDesc read_desc;
  ZZ9KDecompressStreamResult stream_result;
  FILE *file = 0;
  uint8_t *chunk = 0;
  uint32_t input_capacity = ZZ9K_ARCHIVE_STREAM_CHUNK;
  uint32_t output_capacity = ZZ9K_ARCHIVE_STREAM_CHUNK;
  uint32_t input_offset = 0U;
  uint32_t total_input;
  uint32_t total_written = 0U;
  uint32_t session = 0U;
  int need_input = 1;
  int status;
  int ok = 0;

  memset(&input, 0, sizeof(input));
  memset(&decoded, 0, sizeof(decoded));
  memset(final_result, 0, sizeof(*final_result));

  if (!zz9k_archive_service_supports_decompress_feed(service, algorithm)) {
    printf("%s decompress-feed not advertised by codec service\n",
           zz9k_compression_algorithm_text(algorithm));
    return 0;
  }
  if (prefix_length > 0xffffffffUL - compressed_length) {
    printf("unsupported oversized codec feed stream job\n");
    return 0;
  }
  total_input = prefix_length + compressed_length;
  if (total_input == 0U || output_limit == 0U || !entry ||
      (prefix_length != 0U && !prefix) ||
      (compressed_length != 0U && !compressed)) {
    printf("unsupported empty codec feed stream job\n");
    return 0;
  }

  while (input_capacity >= ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
    status = zz9k_alloc_shared(ctx, input_capacity, 16U, 0U, &input);
    if (status == ZZ9K_STATUS_OK) {
      break;
    }
    if (status != ZZ9K_STATUS_NO_MEMORY ||
        input_capacity == ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
      printf("alloc stream input failed: %s (%d), requested=%lu bytes\n",
             zz9k_status_name(status), status,
             (unsigned long)input_capacity);
      zz9k_archive_print_shared_diag(ctx, "stream input",
                                     input_capacity);
      goto out;
    }
    input_capacity /= 2U;
  }
  while (output_capacity >= ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
    status = zz9k_alloc_shared(ctx, output_capacity, 16U, 0U, &decoded);
    if (status == ZZ9K_STATUS_OK) {
      break;
    }
    if (status != ZZ9K_STATUS_NO_MEMORY ||
        output_capacity == ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
      printf("alloc stream output failed: %s (%d), requested=%lu bytes\n",
             zz9k_status_name(status), status,
             (unsigned long)output_capacity);
      zz9k_archive_print_shared_diag(ctx, "stream output",
                                     output_capacity);
      goto out;
    }
    output_capacity /= 2U;
  }

  chunk = (uint8_t *)malloc((size_t)output_capacity);
  if (!chunk) {
    printf("stream chunk allocation failed\n");
    goto out;
  }
  if (!zz9k_compression_build_decompress_stream_begin_desc(
          &begin_desc, algorithm, ZZ9K_INVALID_HANDLE, 0U, 0U,
          output_limit,
          ZZ9K_DECOMPRESS_FLAG_EXPECT_END |
          ZZ9K_DECOMPRESS_FLAG_FEED_INPUT)) {
    printf("could not build decompression feed stream begin descriptor\n");
    goto out;
  }

  status = zz9k_decompress_stream_begin(ctx, &begin_desc, &stream_result);
  if (status != ZZ9K_STATUS_OK) {
    printf("%s feed stream begin failed: %s (%d), input=%lu limit=%lu\n",
           zz9k_compression_algorithm_text(algorithm),
           zz9k_status_name(status), status,
           (unsigned long)total_input,
           (unsigned long)output_limit);
    zz9k_archive_print_shared_diag(ctx, "feed stream begin failure",
                                   total_input);
    goto out;
  }
  session = stream_result.session;
  if (!zz9k_archive_open_output_entry(output_dir, entry, &file)) {
    goto out;
  }

  while (1) {
    if (need_input) {
      uint32_t feed_len;
      uint32_t feed_flags = 0U;

      if (input_offset >= total_input) {
        printf("%s feed stream exhausted input\n",
               zz9k_compression_algorithm_text(algorithm));
        goto out;
      }
      feed_len = total_input - input_offset;
      if (feed_len > input.length) {
        feed_len = input.length;
      }
      if (input_offset + feed_len == total_input) {
        feed_flags |= ZZ9K_DECOMPRESS_STREAM_FEED_EOF;
      }
      if (!zz9k_archive_copy_feed_input_chunk(
              &input, prefix, prefix_length, compressed,
              compressed_length, input_offset, feed_len, &feed_len)) {
        printf("stream input copy failed\n");
        goto out;
      }
      if (!zz9k_compression_build_decompress_stream_feed_desc(
              &feed_desc, session, input.handle, 0U, feed_len,
              feed_flags)) {
        printf("could not build decompression stream feed descriptor\n");
        goto out;
      }
      status = zz9k_decompress_stream_feed(ctx, &feed_desc,
                                           &stream_result);
      if (status != ZZ9K_STATUS_OK) {
        printf("%s feed stream feed failed: %s (%d), input=%lu/%lu\n",
               zz9k_compression_algorithm_text(algorithm),
               zz9k_status_name(status), status,
               (unsigned long)input_offset,
               (unsigned long)total_input);
        zz9k_archive_print_shared_diag(ctx, "feed stream failure",
                                       feed_len);
        session = 0U;
        goto out;
      }
      input_offset += feed_len;
      need_input = 0;
    }

    if (!zz9k_compression_build_decompress_stream_read_desc(
            &read_desc, session, decoded.handle, 0U, decoded.length, 0U)) {
      printf("could not build decompression stream read descriptor\n");
      goto out;
    }
    memset(&stream_result, 0, sizeof(stream_result));
    status = zz9k_decompress_stream_read(ctx, &read_desc, &stream_result);
    if (status != ZZ9K_STATUS_OK) {
      printf("%s feed stream read failed: %s (%d), written=%lu\n",
             zz9k_compression_algorithm_text(algorithm),
             zz9k_status_name(status), status,
             (unsigned long)total_written);
      zz9k_archive_print_shared_diag(ctx, "feed stream read failure",
                                     decoded.length);
      session = 0U;
      goto out;
    }
    if (stream_result.bytes_written > decoded.length ||
        total_written > output_limit ||
        stream_result.bytes_written > output_limit - total_written) {
      printf("%s feed stream output exceeded limit\n",
             zz9k_compression_algorithm_text(algorithm));
      goto out;
    }
    if (stream_result.bytes_written != 0U) {
      if (!zz9k_shared_copy_from(chunk, &decoded, 0U,
                                 stream_result.bytes_written)) {
        printf("feed stream output copy failed\n");
        goto out;
      }
      if (fwrite(chunk, 1U, stream_result.bytes_written, file) !=
          stream_result.bytes_written) {
        printf("feed stream output write failed: %s\n", entry->name);
        goto out;
      }
      total_written += stream_result.bytes_written;
    }
    if ((stream_result.flags & ZZ9K_DECOMPRESS_RESULT_STREAM_END) != 0U) {
      final_result->bytes_consumed = stream_result.bytes_consumed;
      final_result->bytes_written = total_written;
      final_result->checksum = stream_result.checksum;
      final_result->algorithm = stream_result.algorithm;
      final_result->flags = stream_result.flags;
      ok = 1;
      break;
    }
    if ((stream_result.flags & ZZ9K_DECOMPRESS_RESULT_NEED_INPUT) != 0U) {
      need_input = 1;
    } else if (stream_result.bytes_written == 0U) {
      printf("%s feed stream made no output progress\n",
             zz9k_compression_algorithm_text(algorithm));
      goto out;
    }
  }

out:
  if (session != 0U) {
    zz9k_decompress_stream_close(ctx, session, 0U);
  }
  if (file) {
    if (fclose(file) != 0) {
      ok = 0;
    }
  }
  if (ok && !zz9k_archive_last_output_skipped &&
      !zz9k_archive_last_output_dry_run) {
    printf("x %s\n", entry->name);
  }
  if (decoded.handle != 0U && decoded.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, decoded.handle);
  }
  if (input.handle != 0U && input.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, input.handle);
  }
  free(chunk);
  return ok;
}

static int zz9k_archive_decompress_feed_stream_to_file(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *compressed,
    uint32_t compressed_length,
    uint32_t output_limit,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    ZZ9KDecompressResult *final_result)
{
  return zz9k_archive_decompress_feed_stream_parts_to_file(
      ctx, service, algorithm, 0, 0U, compressed, compressed_length,
      output_limit, output_dir, entry, final_result);
}

static int zz9k_archive_decompress_feed_file_parts_core(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *prefix,
    uint32_t prefix_length,
    const char *input_path,
    uint32_t input_start,
    uint32_t input_length,
    uint32_t output_limit,
    int write_output,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    ZZ9KArchiveDecodedChunkFn on_chunk,
    void *on_chunk_user,
    ZZ9KDecompressResult *final_result,
    int *failure_status)
{
  ZZ9KSharedBuffer input;
  ZZ9KSharedBuffer decoded;
  ZZ9KDecompressStreamBeginDesc begin_desc;
  ZZ9KDecompressStreamFeedDesc feed_desc;
  ZZ9KDecompressStreamReadDesc read_desc;
  ZZ9KDecompressStreamResult stream_result;
  FILE *input_file = 0;
  FILE *file = 0;
  uint8_t *chunk = 0;
  uint32_t input_capacity = ZZ9K_ARCHIVE_STREAM_CHUNK;
  uint32_t output_capacity = ZZ9K_ARCHIVE_STREAM_CHUNK;
  uint32_t input_offset = 0U;
  uint32_t total_input;
  uint32_t total_written = 0U;
  uint32_t session = 0U;
  int need_input = 1;
  int status;
  int ok = 0;

  if (failure_status) {
    *failure_status = ZZ9K_STATUS_OK;
  }
  if (!final_result) {
    return 0;
  }

  memset(&input, 0, sizeof(input));
  memset(&decoded, 0, sizeof(decoded));
  memset(final_result, 0, sizeof(*final_result));

  if (!zz9k_archive_service_supports_decompress_feed(service, algorithm)) {
    printf("%s decompress-feed not advertised by codec service\n",
           zz9k_compression_algorithm_text(algorithm));
    return 0;
  }
  if (prefix_length > 0xffffffffUL - input_length) {
    printf("unsupported oversized codec feed file job\n");
    return 0;
  }
  total_input = prefix_length + input_length;
  if (!input_path || total_input == 0U || output_limit == 0U ||
      (write_output && (!entry || !output_dir)) ||
      (prefix_length != 0U && !prefix)) {
    printf("unsupported empty codec feed file job\n");
    return 0;
  }
  if (input_start > 0x7fffffffUL) {
    printf("unsupported codec feed file offset\n");
    return 0;
  }

  input_file = fopen(input_path, "rb");
  if (!input_file) {
    printf("open failed: %s\n", input_path);
    return 0;
  }
  if (fseek(input_file, (long)input_start, SEEK_SET) != 0) {
    printf("file stream input seek failed: %s\n", input_path);
    goto out;
  }

  while (input_capacity >= ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
    status = zz9k_alloc_shared(ctx, input_capacity, 16U, 0U, &input);
    if (status == ZZ9K_STATUS_OK) {
      break;
    }
    if (status != ZZ9K_STATUS_NO_MEMORY ||
        input_capacity == ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
      if (failure_status) {
        *failure_status = status;
      }
      printf("alloc file stream input failed: %s (%d), requested=%lu bytes\n",
             zz9k_status_name(status), status,
             (unsigned long)input_capacity);
      zz9k_archive_print_shared_diag(ctx, "file stream input",
                                     input_capacity);
      goto out;
    }
    input_capacity /= 2U;
  }
  while (output_capacity >= ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
    status = zz9k_alloc_shared(ctx, output_capacity, 16U, 0U, &decoded);
    if (status == ZZ9K_STATUS_OK) {
      break;
    }
    if (status != ZZ9K_STATUS_NO_MEMORY ||
        output_capacity == ZZ9K_ARCHIVE_STREAM_MIN_CHUNK) {
      if (failure_status) {
        *failure_status = status;
      }
      printf("alloc file stream output failed: %s (%d), requested=%lu bytes\n",
             zz9k_status_name(status), status,
             (unsigned long)output_capacity);
      zz9k_archive_print_shared_diag(ctx, "file stream output",
                                     output_capacity);
      goto out;
    }
    output_capacity /= 2U;
  }

  if (write_output || on_chunk) {
    chunk = (uint8_t *)malloc((size_t)output_capacity);
    if (!chunk) {
      printf("file stream chunk allocation failed\n");
      goto out;
    }
  }
  if (!zz9k_compression_build_decompress_stream_begin_desc(
          &begin_desc, algorithm, ZZ9K_INVALID_HANDLE, 0U, 0U,
          output_limit,
          ZZ9K_DECOMPRESS_FLAG_EXPECT_END |
          ZZ9K_DECOMPRESS_FLAG_FEED_INPUT)) {
    printf("could not build decompression feed file begin descriptor\n");
    goto out;
  }

  status = zz9k_decompress_stream_begin(ctx, &begin_desc, &stream_result);
  if (status != ZZ9K_STATUS_OK) {
    if (failure_status) {
      *failure_status = status;
    }
    printf("%s feed file begin failed: %s (%d), input=%lu limit=%lu\n",
           zz9k_compression_algorithm_text(algorithm),
           zz9k_status_name(status), status,
           (unsigned long)total_input,
           (unsigned long)output_limit);
    zz9k_archive_print_shared_diag(ctx, "feed file begin failure",
                                   total_input);
    goto out;
  }
  session = stream_result.session;
  if (write_output &&
      !zz9k_archive_open_output_entry(output_dir, entry, &file)) {
    goto out;
  }

  while (1) {
    if (need_input) {
      uint32_t feed_len;
      uint32_t feed_flags = 0U;

      if (input_offset >= total_input) {
        printf("%s feed file exhausted input\n",
               zz9k_compression_algorithm_text(algorithm));
        goto out;
      }
      feed_len = total_input - input_offset;
      if (feed_len > input.length) {
        feed_len = input.length;
      }
      if (!zz9k_archive_copy_feed_file_input_chunk(
              &input, input_file, prefix, prefix_length, input_length,
              input_offset, feed_len, &feed_len)) {
        printf("file stream input read failed: %s\n", input_path);
        goto out;
      }
      if (input_offset + feed_len == total_input) {
        feed_flags |= ZZ9K_DECOMPRESS_STREAM_FEED_EOF;
      }
      if (!zz9k_compression_build_decompress_stream_feed_desc(
              &feed_desc, session, input.handle, 0U, feed_len,
              feed_flags)) {
        printf("could not build decompression file feed descriptor\n");
        goto out;
      }
      status = zz9k_decompress_stream_feed(ctx, &feed_desc,
                                           &stream_result);
      if (status != ZZ9K_STATUS_OK) {
        if (failure_status) {
          *failure_status = status;
        }
        printf("%s feed file feed failed: %s (%d), input=%lu/%lu\n",
               zz9k_compression_algorithm_text(algorithm),
               zz9k_status_name(status), status,
               (unsigned long)input_offset,
               (unsigned long)total_input);
        zz9k_archive_print_shared_diag(ctx, "feed file failure",
                                       feed_len);
        session = 0U;
        goto out;
      }
      input_offset += feed_len;
      need_input = 0;
    }

    if (!zz9k_compression_build_decompress_stream_read_desc(
            &read_desc, session, decoded.handle, 0U, decoded.length, 0U)) {
      printf("could not build decompression file read descriptor\n");
      goto out;
    }
    memset(&stream_result, 0, sizeof(stream_result));
    status = zz9k_decompress_stream_read(ctx, &read_desc, &stream_result);
    if (status != ZZ9K_STATUS_OK) {
      if (failure_status) {
        *failure_status = status;
      }
      printf("%s feed file read failed: %s (%d), written=%lu\n",
             zz9k_compression_algorithm_text(algorithm),
             zz9k_status_name(status), status,
             (unsigned long)total_written);
      zz9k_archive_print_shared_diag(ctx, "feed file read failure",
                                     decoded.length);
      session = 0U;
      goto out;
    }
    if (stream_result.bytes_written > decoded.length ||
        total_written > output_limit ||
        stream_result.bytes_written > output_limit - total_written) {
      printf("%s feed file output exceeded limit\n",
             zz9k_compression_algorithm_text(algorithm));
      goto out;
    }
    if (stream_result.bytes_written != 0U) {
      if (write_output || on_chunk) {
        if (!zz9k_shared_copy_from(chunk, &decoded, 0U,
                                   stream_result.bytes_written)) {
          printf("feed file output copy failed\n");
          goto out;
        }
      }
      if (write_output) {
        if (fwrite(chunk, 1U, stream_result.bytes_written, file) !=
            stream_result.bytes_written) {
          printf("feed file output write failed: %s\n", entry->name);
          goto out;
        }
      }
      if (on_chunk &&
          !on_chunk(on_chunk_user, chunk, stream_result.bytes_written)) {
        printf("%s feed file output consumer failed\n",
               zz9k_compression_algorithm_text(algorithm));
        goto out;
      }
      total_written += stream_result.bytes_written;
    }
    if ((stream_result.flags & ZZ9K_DECOMPRESS_RESULT_STREAM_END) != 0U) {
      final_result->bytes_consumed = stream_result.bytes_consumed;
      final_result->bytes_written = total_written;
      final_result->checksum = stream_result.checksum;
      final_result->algorithm = stream_result.algorithm;
      final_result->flags = stream_result.flags;
      ok = 1;
      break;
    }
    if ((stream_result.flags & ZZ9K_DECOMPRESS_RESULT_NEED_INPUT) != 0U) {
      need_input = 1;
    } else if (stream_result.bytes_written == 0U) {
      printf("%s feed file made no output progress\n",
             zz9k_compression_algorithm_text(algorithm));
      goto out;
    }
  }

out:
  if (session != 0U) {
    zz9k_decompress_stream_close(ctx, session, 0U);
  }
  if (file) {
    if (fclose(file) != 0) {
      ok = 0;
    }
  }
  if (input_file) {
    fclose(input_file);
  }
  if (ok && write_output && !zz9k_archive_last_output_skipped &&
      !zz9k_archive_last_output_dry_run) {
    printf("x %s\n", entry->name);
  }
  if (decoded.handle != 0U && decoded.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, decoded.handle);
  }
  if (input.handle != 0U && input.handle != ZZ9K_INVALID_HANDLE) {
    zz9k_free_shared(ctx, input.handle);
  }
  free(chunk);
  return ok;
}

static int zz9k_archive_decompress_feed_file_parts_to_file(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *prefix,
    uint32_t prefix_length,
    const char *input_path,
    uint32_t input_start,
    uint32_t input_length,
    uint32_t output_limit,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    ZZ9KDecompressResult *final_result)
{
  return zz9k_archive_decompress_feed_file_parts_core(
      ctx, service, algorithm, prefix, prefix_length, input_path,
      input_start, input_length, output_limit, 1, output_dir, entry,
      0, 0, final_result, 0);
}

static int zz9k_archive_decompress_feed_file_parts_to_file_status(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *prefix,
    uint32_t prefix_length,
    const char *input_path,
    uint32_t input_start,
    uint32_t input_length,
    uint32_t output_limit,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    ZZ9KDecompressResult *final_result,
    int *failure_status)
{
  return zz9k_archive_decompress_feed_file_parts_core(
      ctx, service, algorithm, prefix, prefix_length, input_path,
      input_start, input_length, output_limit, 1, output_dir, entry,
      0, 0, final_result, failure_status);
}

static int zz9k_archive_decompress_feed_file_parts_to_result_status(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *prefix,
    uint32_t prefix_length,
    const char *input_path,
    uint32_t input_start,
    uint32_t input_length,
    uint32_t output_limit,
    ZZ9KDecompressResult *final_result,
    int *failure_status)
{
  return zz9k_archive_decompress_feed_file_parts_core(
      ctx, service, algorithm, prefix, prefix_length, input_path,
      input_start, input_length, output_limit, 0, 0, 0, 0, 0,
      final_result, failure_status);
}

static int zz9k_archive_decompress_feed_file_parts_to_result(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const uint8_t *prefix,
    uint32_t prefix_length,
    const char *input_path,
    uint32_t input_start,
    uint32_t input_length,
    uint32_t output_limit,
    ZZ9KDecompressResult *final_result)
{
  return zz9k_archive_decompress_feed_file_parts_to_result_status(
      ctx, service, algorithm, prefix, prefix_length, input_path,
      input_start, input_length, output_limit, final_result, 0);
}

static int zz9k_archive_decompress_feed_file_to_callback(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const char *input_path,
    uint32_t input_length,
    uint32_t output_limit,
    ZZ9KArchiveDecodedChunkFn on_chunk,
    void *on_chunk_user,
    ZZ9KDecompressResult *final_result)
{
  return zz9k_archive_decompress_feed_file_parts_core(
      ctx, service, algorithm, 0, 0U, input_path, 0U, input_length,
      output_limit, 0, 0, 0, on_chunk, on_chunk_user, final_result, 0);
}

static int zz9k_archive_decompress_feed_file_to_file(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const char *input_path,
    uint32_t input_length,
    uint32_t output_limit,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    ZZ9KDecompressResult *final_result)
{
  return zz9k_archive_decompress_feed_file_parts_to_file(
      ctx, service, algorithm, 0, 0U, input_path, 0U, input_length,
      output_limit, output_dir, entry, final_result);
}

static int zz9k_archive_decompress_feed_file_to_result(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    uint32_t algorithm,
    const char *input_path,
    uint32_t input_length,
    uint32_t output_limit,
    ZZ9KDecompressResult *final_result)
{
  return zz9k_archive_decompress_feed_file_parts_to_result(
      ctx, service, algorithm, 0, 0U, input_path, 0U, input_length,
      output_limit, final_result);
}

static int zz9k_archive_zip_file_can_test_entries(
    const ZZ9KArchiveEntry *entries,
    uint32_t count,
    int *needs_deflate)
{
  uint32_t i;

  if (!entries || count == 0U || !needs_deflate) {
    return 0;
  }
  *needs_deflate = 0;
  for (i = 0U; i < count; i++) {
    if (!zz9k_archive_entry_matches_filter(&entries[i])) {
      continue;
    }
    if (entries[i].is_dir) {
      continue;
    }
    if (entries[i].method == ZZ9K_ARCHIVE_ZIP_METHOD_STORE) {
      continue;
    }
    if (entries[i].method == ZZ9K_ARCHIVE_ZIP_METHOD_DEFLATE) {
      *needs_deflate = 1;
      continue;
    }
    return 0;
  }
  return 1;
}

static uint32_t zz9k_archive_zip_test_output_limit(uint32_t output_size)
{
  return output_size == UINT32_MAX ? output_size : output_size + 1U;
}

static int zz9k_archive_entry_has_crc32(const ZZ9KArchiveEntry *entry)
{
  return entry &&
         (entry->flags & ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32) != 0U;
}

static int zz9k_archive_7z_entry_has_unsupported_split(
    const ZZ9KArchiveEntry *entry)
{
  return entry &&
         (entry->flags &
          ZZ9K_ARCHIVE_ENTRY_FLAG_7Z_UNSUPPORTED_SPLIT) != 0U;
}

static int zz9k_archive_7z_entry_has_split_substream(
    const ZZ9KArchiveEntry *entry)
{
  return zz9k_archive_7z_entry_has_unsupported_split(entry);
}

static void zz9k_archive_print_7z_entry_unsupported(
    const ZZ9KArchiveEntry *entry)
{
  if (!entry) {
    return;
  }
  if (zz9k_archive_7z_entry_has_unsupported_split(entry)) {
    printf("7z non-Copy multi-substream unsupported: %s\n", entry->name);
  } else {
    printf("7z entry unsupported: %s\n", entry->name);
  }
}

static int zz9k_archive_zip_result_matches_entry(
    const ZZ9KArchiveEntry *entry,
    const ZZ9KDecompressResult *result)
{
  if (!entry || !result) {
    return 0;
  }
  return result->bytes_written == entry->uncompressed_size &&
         result->checksum == entry->crc32;
}

static int zz9k_archive_zip_stored_entry_crc_matches(
    const ZZ9KArchiveEntry *entry,
    const uint8_t *data)
{
  if (!entry || (!data && entry->uncompressed_size != 0U) ||
      entry->compressed_size != entry->uncompressed_size) {
    return 0;
  }
  return zz9k_archive_crc32(0U, data, entry->uncompressed_size) ==
         entry->crc32;
}

static int zz9k_archive_7z_copy_entry_crc_matches(
    const ZZ9KArchiveEntry *entry,
    const uint8_t *data)
{
  if (!entry ||
      entry->compressed_size != entry->uncompressed_size ||
      (!data && entry->uncompressed_size != 0U)) {
    return 0;
  }
  if (!zz9k_archive_entry_has_crc32(entry)) {
    return 1;
  }
  return zz9k_archive_crc32(0U, data, entry->uncompressed_size) ==
         entry->crc32;
}

static int zz9k_archive_7z_copy_file_crc_matches(
    const char *archive_path,
    const ZZ9KArchiveEntry *entry,
    uint32_t *actual_crc)
{
  uint32_t crc = 0U;

  if (!archive_path || !entry ||
      entry->compressed_size != entry->uncompressed_size) {
    return 0;
  }
  if (!zz9k_archive_entry_has_crc32(entry)) {
    return 1;
  }
  if (!zz9k_archive_file_range_crc32(
          archive_path, entry->data_offset, entry->uncompressed_size,
          &crc)) {
    return 0;
  }
  if (actual_crc) {
    *actual_crc = crc;
  }
  return crc == entry->crc32;
}

static int zz9k_archive_7z_result_matches_entry(
    const ZZ9KArchiveEntry *entry,
    const ZZ9KDecompressResult *result)
{
  if (!entry || !result ||
      result->bytes_written != entry->uncompressed_size) {
    return 0;
  }
  if (!zz9k_archive_entry_has_crc32(entry)) {
    return 1;
  }
  return result->checksum == entry->crc32;
}

static int zz9k_archive_zip_stored_file_crc_matches(
    const char *archive_path,
    const ZZ9KArchiveEntry *entry)
{
  uint8_t *data = 0;
  int ok;

  if (!archive_path || !entry) {
    return 0;
  }
  if (!zz9k_archive_read_file_range(
          archive_path, entry->data_offset, entry->compressed_size, &data)) {
    return 0;
  }
  ok = zz9k_archive_zip_stored_entry_crc_matches(entry, data);
  free(data);
  return ok;
}

static int zz9k_archive_zip_test_deflate_entry(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    const char *archive_path,
    const ZZ9KArchiveEntry *entry)
{
  uint8_t *compressed = 0;
  ZZ9KDecompressResult result;
  uint32_t output_limit;
  int ok = 0;

  if (!ctx || !service || !archive_path || !entry) {
    return 0;
  }
  output_limit =
      zz9k_archive_zip_test_output_limit(entry->uncompressed_size);
  if (zz9k_archive_service_supports_decompress_feed(
          service, ZZ9K_COMPRESSION_DEFLATE_RAW)) {
    if (!zz9k_archive_decompress_feed_file_parts_to_result(
            ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
            0, 0U, archive_path, entry->data_offset,
            entry->compressed_size, output_limit, &result)) {
      printf("zip deflate feed test failed: %s packed=%lu unpacked=%lu "
             "offset=%lu flags=0x%04lx limit=%lu\n",
             entry->name,
             (unsigned long)entry->compressed_size,
             (unsigned long)entry->uncompressed_size,
             (unsigned long)entry->data_offset,
             (unsigned long)entry->flags,
             (unsigned long)output_limit);
      goto out;
    }
    goto check_result;
  }
  if (service &&
      (service->flags & ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED) != 0U &&
      (service->flags & ZZ9K_SERVICE_FLAG_CODEC_DEFLATE_FEED) == 0U) {
    printf("zip deflate-feed not advertised; using one-shot test: %s\n",
           entry->name);
  }
  if (!zz9k_archive_read_file_range(
          archive_path, entry->data_offset, entry->compressed_size,
          &compressed)) {
    goto out;
  }
  if (!zz9k_archive_decompress_test_to_result(
          ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
          compressed, entry->compressed_size, output_limit, &result)) {
    printf("zip deflate test failed: %s packed=%lu unpacked=%lu "
           "offset=%lu flags=0x%04lx limit=%lu\n",
           entry->name,
           (unsigned long)entry->compressed_size,
           (unsigned long)entry->uncompressed_size,
           (unsigned long)entry->data_offset,
           (unsigned long)entry->flags,
           (unsigned long)output_limit);
    goto out;
  }
check_result:
  if (result.bytes_consumed != entry->compressed_size) {
    printf("zip entry input mismatch: %s consumed=%lu expected=%lu\n",
           entry->name,
           (unsigned long)result.bytes_consumed,
           (unsigned long)entry->compressed_size);
    goto out;
  }
  if (result.bytes_written != entry->uncompressed_size) {
    printf("zip entry size mismatch: %s decoded=%lu expected=%lu\n",
           entry->name,
           (unsigned long)result.bytes_written,
           (unsigned long)entry->uncompressed_size);
    goto out;
  }
  if (!zz9k_archive_zip_result_matches_entry(entry, &result)) {
    printf("zip entry crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
           entry->name,
           (unsigned long)result.checksum,
           (unsigned long)entry->crc32);
    goto out;
  }
  ok = 1;

out:
  free(compressed);
  return ok;
}

static int zz9k_archive_zip_extract_deflate_entry(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    const char *archive_path,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry)
{
  ZZ9KDecompressResult result;
  uint32_t output_limit;
  int ok = 0;

  if (!ctx || !service || !archive_path || !output_dir || !entry) {
    return 0;
  }
  if (!zz9k_archive_service_supports_decompress_feed(
          service, ZZ9K_COMPRESSION_DEFLATE_RAW)) {
    printf("zip deflate-feed not advertised; cannot stream extract: %s\n",
           entry->name);
    return 0;
  }
  if (entry->compressed_size == 0U && entry->uncompressed_size == 0U) {
    return zz9k_archive_write_entry(output_dir, entry, 0);
  }
  if (entry->uncompressed_size == 0U) {
    output_limit = zz9k_archive_zip_test_output_limit(
        entry->uncompressed_size);
    if (!zz9k_archive_decompress_feed_file_parts_to_result(
            ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW, 0, 0U,
            archive_path, entry->data_offset, entry->compressed_size,
            output_limit, &result)) {
      printf("zip deflate feed extract failed: %s packed=%lu unpacked=%lu "
             "offset=%lu flags=0x%04lx limit=%lu\n",
             entry->name,
             (unsigned long)entry->compressed_size,
             (unsigned long)entry->uncompressed_size,
             (unsigned long)entry->data_offset,
             (unsigned long)entry->flags,
             (unsigned long)output_limit);
      goto out;
    }
    if (result.bytes_consumed != entry->compressed_size) {
      printf("zip entry input mismatch: %s consumed=%lu expected=%lu\n",
             entry->name,
             (unsigned long)result.bytes_consumed,
             (unsigned long)entry->compressed_size);
      goto out;
    }
    if (result.bytes_written != 0U) {
      printf("zip entry size mismatch: %s decoded=%lu expected=0\n",
             entry->name,
             (unsigned long)result.bytes_written);
      goto out;
    }
    if (!zz9k_archive_zip_result_matches_entry(entry, &result)) {
      printf("zip entry crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
             entry->name,
             (unsigned long)result.checksum,
             (unsigned long)entry->crc32);
      goto out;
    }
    return zz9k_archive_write_entry(output_dir, entry, 0);
  }

  output_limit = entry->uncompressed_size;
  if (!zz9k_archive_decompress_feed_file_parts_to_file(
          ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW, 0, 0U,
          archive_path, entry->data_offset, entry->compressed_size,
          output_limit, output_dir, entry, &result)) {
    printf("zip deflate feed extract failed: %s packed=%lu unpacked=%lu "
           "offset=%lu flags=0x%04lx limit=%lu\n",
           entry->name,
           (unsigned long)entry->compressed_size,
           (unsigned long)entry->uncompressed_size,
           (unsigned long)entry->data_offset,
           (unsigned long)entry->flags,
           (unsigned long)output_limit);
    goto out;
  }
  if (result.bytes_consumed != entry->compressed_size) {
    printf("zip entry input mismatch: %s consumed=%lu expected=%lu\n",
           entry->name,
           (unsigned long)result.bytes_consumed,
           (unsigned long)entry->compressed_size);
    goto out;
  }
  if (result.bytes_written != entry->uncompressed_size) {
    printf("zip entry size mismatch: %s decoded=%lu expected=%lu\n",
           entry->name,
           (unsigned long)result.bytes_written,
           (unsigned long)entry->uncompressed_size);
    goto out;
  }
  if (!zz9k_archive_zip_result_matches_entry(entry, &result)) {
    printf("zip entry crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
           entry->name,
           (unsigned long)result.checksum,
           (unsigned long)entry->crc32);
    goto out;
  }
  ok = 1;

out:
  return ok;
}

static int zz9k_archive_zip_file_can_extract_entries(
    const ZZ9KArchiveEntry *entries,
    uint32_t count,
    int *needs_deflate)
{
  uint32_t i;

  if (!entries || count == 0U || !needs_deflate) {
    return 0;
  }
  *needs_deflate = 0;
  for (i = 0U; i < count; i++) {
    if (!zz9k_archive_entry_matches_filter(&entries[i])) {
      continue;
    }
    if (entries[i].is_dir) {
      continue;
    }
    if (entries[i].method == ZZ9K_ARCHIVE_ZIP_METHOD_STORE) {
      continue;
    }
    if (entries[i].method == ZZ9K_ARCHIVE_ZIP_METHOD_DEFLATE) {
      *needs_deflate = 1;
      continue;
    }
    return 0;
  }
  return 1;
}

static int zz9k_archive_handle_zip_file(ZZ9KContext **ctx,
                                        ZZ9KServiceInfo *service,
                                        int *codec_ready,
                                        const char *archive_path,
                                        uint32_t archive_length,
                                        const char *command,
                                        const char *output_dir,
                                        int *attempted)
{
  uint8_t *directory = 0;
  ZZ9KArchiveEntry *entries = 0;
  uint32_t directory_length = 0U;
  uint32_t count = 0U;
  uint32_t i;
  int is_list;
  int is_test;
  int is_extract;
  int needs_deflate = 0;
  int ok = 1;

  if (!attempted) {
    return 0;
  }
  *attempted = 0;
  is_list = command && strcmp(command, "l") == 0;
  is_test = command && strcmp(command, "t") == 0;
  is_extract = command && strcmp(command, "x") == 0;
  if (!archive_path || (!is_list && !is_test && !is_extract)) {
    return 0;
  }
  if (!zz9k_archive_zip_read_directory_from_file(
          archive_path, archive_length, &directory,
          &directory_length, &count) ||
      !zz9k_archive_alloc_entries(count, &entries)) {
    goto out;
  }
  if (!zz9k_archive_zip_list_from_directory(
          archive_path, directory, directory_length, archive_length,
          entries, count, count, &count)) {
    goto out;
  }

  if (is_list) {
    *attempted = 1;
    for (i = 0U; i < count; i++) {
      if (!zz9k_archive_entry_matches_filter(&entries[i])) {
        continue;
      }
      zz9k_archive_print_entry(&entries[i]);
    }
    goto out;
  }
  if (is_test) {
    if (!zz9k_archive_zip_file_can_test_entries(
            entries, count, &needs_deflate)) {
      goto out;
    }
    if (needs_deflate &&
        (!ctx || !service || !codec_ready ||
         !zz9k_archive_ensure_codec_open(ctx, service, codec_ready) ||
         !zz9k_archive_service_supports_decompress_test(
             service, ZZ9K_COMPRESSION_DEFLATE_RAW))) {
      goto out;
    }
  } else if (is_extract) {
    if (!zz9k_archive_zip_file_can_extract_entries(
            entries, count, &needs_deflate)) {
      goto out;
    }
    if (needs_deflate) {
      if (!ctx || !service || !codec_ready ||
          !zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
        goto out;
      }
      if (!zz9k_archive_service_supports_decompress_feed(
              service, ZZ9K_COMPRESSION_DEFLATE_RAW)) {
        printf("zip deflate-feed not advertised; using legacy extract path\n");
        goto out;
      }
    }
  }

  *attempted = 1;
  for (i = 0U; i < count; i++) {
    ZZ9KArchiveEntry *entry = &entries[i];

    if (!zz9k_archive_entry_matches_filter(entry)) {
      continue;
    }
    if (!zz9k_archive_path_is_safe(entry->name)) {
      printf("unsafe path rejected: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if ((entry->flags & 1U) != 0U) {
      printf("encrypted zip entry unsupported: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if (entry->is_dir) {
      if (is_extract) {
        ok &= zz9k_archive_write_entry(output_dir, entry, 0);
      }
      continue;
    }
    if (entry->method == ZZ9K_ARCHIVE_ZIP_METHOD_STORE &&
        entry->compressed_size != entry->uncompressed_size) {
      printf("stored zip entry size mismatch: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if (entry->method == ZZ9K_ARCHIVE_ZIP_METHOD_STORE &&
        !zz9k_archive_zip_stored_file_crc_matches(archive_path, entry)) {
      printf("stored zip entry crc mismatch: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if (entry->method == ZZ9K_ARCHIVE_ZIP_METHOD_STORE && is_extract) {
      ok &= zz9k_archive_write_file_range_entry(
          output_dir, entry, archive_path);
    } else if (entry->method == ZZ9K_ARCHIVE_ZIP_METHOD_DEFLATE &&
               is_test) {
      if (!ctx || !*ctx || !service ||
          !zz9k_archive_zip_test_deflate_entry(
              *ctx, service, archive_path, entry)) {
        ok = 0;
      }
    } else if (entry->method == ZZ9K_ARCHIVE_ZIP_METHOD_DEFLATE &&
               is_extract) {
      if (!ctx || !*ctx || !service ||
          !zz9k_archive_zip_extract_deflate_entry(
              *ctx, service, archive_path, output_dir, entry)) {
        ok = 0;
      }
    }
  }
  if (is_test && ok) {
    printf("zip test ok: %lu entries\n", (unsigned long)count);
  }

out:
  free(entries);
  free(directory);
  return ok;
}

static int zz9k_archive_handle_zip(ZZ9KContext *ctx,
                                   const ZZ9KServiceInfo *service,
                                   const uint8_t *data,
                                   uint32_t length,
                                   const char *command,
                                   const char *output_dir)
{
  ZZ9KArchiveEntry *entries;
  uint32_t count;
  uint32_t i;
  int ok = 1;

  if (!zz9k_archive_count_zip_entries(data, length, &count) ||
      !zz9k_archive_alloc_entries(count, &entries)) {
    printf("zip parse failed\n");
    return 0;
  }
  if (!zz9k_archive_zip_list(data, length, entries, count, &count)) {
    printf("zip parse failed\n");
    free(entries);
    return 0;
  }

  for (i = 0U; i < count; i++) {
    ZZ9KArchiveEntry *entry = &entries[i];
    uint8_t *decoded = 0;
    ZZ9KDecompressResult result;

    if (!zz9k_archive_entry_matches_filter(entry)) {
      continue;
    }
    if (strcmp(command, "l") == 0) {
      zz9k_archive_print_entry(entry);
      continue;
    }
    if (!zz9k_archive_path_is_safe(entry->name)) {
      printf("unsafe path rejected: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if ((entry->flags & 1U) != 0U) {
      printf("encrypted zip entry unsupported: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if (entry->is_dir) {
      if (strcmp(command, "x") == 0) {
        ok &= zz9k_archive_write_entry(output_dir, entry, data);
      }
      continue;
    }
    if (entry->method == ZZ9K_ARCHIVE_ZIP_METHOD_STORE) {
      if (entry->compressed_size != entry->uncompressed_size) {
        printf("stored zip entry size mismatch: %s\n", entry->name);
        ok = 0;
        continue;
      }
      if (!zz9k_archive_zip_stored_entry_crc_matches(
              entry, data + entry->data_offset)) {
        printf("stored zip entry crc mismatch: %s\n", entry->name);
        ok = 0;
        continue;
      }
      decoded = (uint8_t *)malloc((size_t)entry->uncompressed_size + 1U);
      if (!decoded) {
        printf("entry allocation failed: %s\n", entry->name);
        ok = 0;
        continue;
      }
      if (entry->uncompressed_size != 0U) {
        memcpy(decoded, data + entry->data_offset, entry->uncompressed_size);
      }
    } else if (entry->method == ZZ9K_ARCHIVE_ZIP_METHOD_DEFLATE) {
      if (!ctx || !service) {
        printf("codec service unavailable\n");
        ok = 0;
        continue;
      }
      if (!zz9k_archive_decompress_to_memory(
              ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
              data + entry->data_offset, entry->compressed_size,
              entry->uncompressed_size, &decoded, &result)) {
        printf("zip deflate legacy decode failed: %s packed=%lu "
               "unpacked=%lu offset=%lu flags=0x%04lx\n",
               entry->name,
               (unsigned long)entry->compressed_size,
               (unsigned long)entry->uncompressed_size,
               (unsigned long)entry->data_offset,
               (unsigned long)entry->flags);
        ok = 0;
        continue;
      }
      if (result.bytes_written != entry->uncompressed_size) {
        printf("zip entry size mismatch: %s\n", entry->name);
        free(decoded);
        ok = 0;
        continue;
      }
      if (!zz9k_archive_zip_result_matches_entry(entry, &result)) {
        printf("zip entry crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
               entry->name,
               (unsigned long)result.checksum,
               (unsigned long)entry->crc32);
        free(decoded);
        ok = 0;
        continue;
      }
    } else {
      printf("zip method unsupported: %lu %s\n",
             (unsigned long)entry->method, entry->name);
      ok = 0;
      continue;
    }

    if (strcmp(command, "x") == 0) {
      ok &= zz9k_archive_write_entry(output_dir, entry, decoded);
    }
    free(decoded);
  }
  if (strcmp(command, "t") == 0 && ok) {
    printf("zip test ok: %lu entries\n", (unsigned long)count);
  }
  free(entries);
  return ok;
}

static int zz9k_archive_handle_lha(const uint8_t *data,
                                   uint32_t length,
                                   const char *command,
                                   const char *output_dir)
{
  ZZ9KArchiveEntry *entries = 0;
  uint32_t count = 0U;
  uint32_t i;
  int ok = 1;

  if (!zz9k_archive_lha_list(data, length, 0, 0U, &count)) {
    printf("lha parse failed\n");
    return 0;
  }
  if (count != 0U) {
    entries = (ZZ9KArchiveEntry *)calloc(count, sizeof(*entries));
    if (!entries) {
      printf("lha entry allocation failed\n");
      return 0;
    }
  }
  if (!zz9k_archive_lha_list(data, length, entries, count, &count)) {
    printf("lha parse failed\n");
    free(entries);
    return 0;
  }

  for (i = 0U; i < count; i++) {
    ZZ9KArchiveEntry *entry = &entries[i];

    if (!zz9k_archive_entry_matches_filter(entry)) {
      continue;
    }
    if (strcmp(command, "l") == 0) {
      printf("%c %10lu %s\n", entry->is_dir ? 'd' : '-',
             (unsigned long)entry->uncompressed_size, entry->name);
      continue;
    }
    if (!zz9k_archive_path_is_safe(entry->name)) {
      printf("unsafe path rejected: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if (entry->is_dir) {
      if (strcmp(command, "x") == 0) {
        ok &= zz9k_archive_write_entry(output_dir, entry, 0);
      }
      continue;
    }
    if (zz9k_archive_lha_method_supported(entry->method)) {
      if (strcmp(command, "t") == 0) {
        ok &= zz9k_archive_lha_decode_method_to_file(data, length, entry, 0);
      } else if (strcmp(command, "x") == 0) {
        ok &= zz9k_archive_extract_lha_lh5(data, length, output_dir, entry);
      }
      continue;
    }
    if (entry->method != ZZ9K_ARCHIVE_LHA_METHOD_LH0) {
      printf("lha method unsupported: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if (entry->compressed_size != entry->uncompressed_size ||
        entry->data_offset > length ||
        entry->uncompressed_size > length - entry->data_offset) {
      printf("lha stored entry size mismatch: %s\n", entry->name);
      ok = 0;
      continue;
    }
    if (strcmp(command, "x") == 0) {
      ok &= zz9k_archive_write_entry(
          output_dir, entry, data + entry->data_offset);
    }
  }
  if (strcmp(command, "t") == 0 && ok) {
    printf("lha test ok: %lu entries\n", (unsigned long)count);
  }
  free(entries);
  return ok;
}

static int zz9k_archive_handle_gzip(ZZ9KContext *ctx,
                                    const ZZ9KServiceInfo *service,
                                    const uint8_t *data,
                                    uint32_t length,
                                    const char *command,
                                    const char *output_dir)
{
  ZZ9KArchiveGzipInfo info;
  ZZ9KDecompressResult result;
  uint8_t *decoded = 0;
  ZZ9KArchiveFormat inner;
  int ok = 0;

  if (!zz9k_archive_gzip_info(data, length, &info)) {
    printf("gzip parse failed\n");
    return 0;
  }
  if (!ctx || !service) {
    printf("codec service unavailable\n");
    return 0;
  }
  if (!zz9k_archive_decompress_to_memory(
          ctx, service, ZZ9K_COMPRESSION_GZIP,
          data, length, info.uncompressed_size, &decoded, &result)) {
    return 0;
  }
  if (result.bytes_written != info.uncompressed_size) {
    printf("gzip size mismatch\n");
    goto out;
  }
  if (result.checksum != info.crc32) {
    printf("gzip crc mismatch: decoded=0x%08lx expected=0x%08lx\n",
           (unsigned long)result.checksum,
           (unsigned long)info.crc32);
    goto out;
  }

  inner = zz9k_archive_detect_format(decoded, result.bytes_written);
  if (inner == ZZ9K_ARCHIVE_FORMAT_TAR) {
    ok = zz9k_archive_handle_tar(ctx, service, decoded,
                                 result.bytes_written, command, output_dir);
  } else if (strcmp(command, "l") == 0) {
    printf("- %10lu %s\n", (unsigned long)info.uncompressed_size, info.name);
    ok = 1;
  } else if (strcmp(command, "t") == 0) {
    printf("gzip test ok: out=%lu crc32=0x%08lx\n",
           (unsigned long)result.bytes_written,
           (unsigned long)result.checksum);
    ok = 1;
  } else {
    ZZ9KArchiveEntry entry;

    memset(&entry, 0, sizeof(entry));
    strcpy(entry.name, info.name);
    entry.method = ZZ9K_COMPRESSION_GZIP;
    entry.uncompressed_size = result.bytes_written;
    ok = zz9k_archive_write_entry(output_dir, &entry, decoded);
  }

out:
  free(decoded);
  return ok;
}

static int zz9k_archive_handle_tar_gzip_feed(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    const char *archive_path,
    uint32_t file_length,
    const char *command,
    const char *output_dir,
    const ZZ9KArchiveGzipInfo *info)
{
  ZZ9KArchiveTarStream tar_stream;
  ZZ9KDecompressResult result;
  uint32_t output_limit;
  int ok = 0;

  if (!ctx || !service || !archive_path || !command || !info) {
    return 0;
  }
  output_limit = zz9k_archive_zip_test_output_limit(info->uncompressed_size);
  zz9k_archive_tar_stream_init(&tar_stream, command, output_dir);
  if (!zz9k_archive_decompress_feed_file_to_callback(
          ctx, service, ZZ9K_COMPRESSION_GZIP, archive_path, file_length,
          output_limit, zz9k_archive_tar_stream_chunk, &tar_stream,
          &result)) {
    printf("tar.gz feed failed: packed=%lu unpacked=%lu limit=%lu\n",
           (unsigned long)info->compressed_size,
           (unsigned long)info->uncompressed_size,
           (unsigned long)output_limit);
    goto out;
  }
  if (!zz9k_archive_tar_stream_finish(&tar_stream)) {
    printf("tar.gz stream parse failed\n");
    goto out;
  }
  if (result.bytes_consumed != info->compressed_size) {
    printf("tar.gz input mismatch: consumed=%lu expected=%lu\n",
           (unsigned long)result.bytes_consumed,
           (unsigned long)info->compressed_size);
    goto out;
  }
  if (result.bytes_written != info->uncompressed_size) {
    printf("tar.gz size mismatch: decoded=%lu expected=%lu\n",
           (unsigned long)result.bytes_written,
           (unsigned long)info->uncompressed_size);
    goto out;
  }
  if (result.checksum != info->crc32) {
    printf("tar.gz crc mismatch: decoded=0x%08lx expected=0x%08lx\n",
           (unsigned long)result.checksum,
           (unsigned long)info->crc32);
    goto out;
  }
  if (strcmp(command, "t") == 0) {
    printf("tar test ok: %lu entries\n", (unsigned long)tar_stream.count);
  }
  ok = 1;

out:
  zz9k_archive_tar_stream_cleanup(&tar_stream);
  return ok;
}

static int zz9k_archive_handle_gzip_file(
    ZZ9KContext **ctx,
    ZZ9KServiceInfo *service,
    int *codec_ready,
    const char *archive_path,
    const uint8_t *probe,
    uint32_t probe_length,
    uint32_t file_length,
    const char *command,
    const char *output_dir,
    int *attempted)
{
  ZZ9KArchiveGzipInfo info;
  ZZ9KArchiveEntry entry;
  ZZ9KDecompressResult result;
  uint32_t output_limit;
  int is_list;
  int is_test;
  int is_extract;
  int ok = 0;

  if (!attempted) {
    return 0;
  }
  *attempted = 0;
  is_list = command && strcmp(command, "l") == 0;
  is_test = command && strcmp(command, "t") == 0;
  is_extract = command && strcmp(command, "x") == 0;
  if (!archive_path || !probe || (!is_list && !is_test && !is_extract) ||
      !zz9k_archive_gzip_info_from_file(
          archive_path, probe, probe_length, file_length, &info)) {
    return 0;
  }
  if (zz9k_archive_gzip_is_tar_candidate(archive_path, &info)) {
    if (!ctx || !service || !codec_ready ||
        !zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
      return 0;
    }
    if (!zz9k_archive_service_supports_decompress_feed(
            service, ZZ9K_COMPRESSION_GZIP)) {
      if ((service->flags & ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED) != 0U &&
          (service->flags & ZZ9K_SERVICE_FLAG_CODEC_GZIP_FEED) == 0U) {
        printf("gzip-feed not advertised; using legacy tar.gz path\n");
      }
      return 0;
    }
    *attempted = 1;
    return zz9k_archive_handle_tar_gzip_feed(
        *ctx, service, archive_path, file_length, command, output_dir, &info);
  }
  if (is_list) {
    *attempted = 1;
    printf("- %10lu %s\n", (unsigned long)info.uncompressed_size, info.name);
    return 1;
  }
  if (!ctx || !service || !codec_ready ||
      !zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
    return 0;
  }
  if (!zz9k_archive_service_supports_decompress_feed(
          service, ZZ9K_COMPRESSION_GZIP)) {
    if ((service->flags & ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED) != 0U &&
        (service->flags & ZZ9K_SERVICE_FLAG_CODEC_GZIP_FEED) == 0U) {
      printf("gzip-feed not advertised; using one-shot gzip path\n");
    }
    return 0;
  }

  *attempted = 1;
  output_limit = zz9k_archive_zip_test_output_limit(info.uncompressed_size);
  if (is_test) {
    if (!zz9k_archive_decompress_feed_file_to_result(
            *ctx, service, ZZ9K_COMPRESSION_GZIP, archive_path,
            file_length, output_limit, &result)) {
      printf("gzip feed test failed: packed=%lu unpacked=%lu limit=%lu\n",
             (unsigned long)info.compressed_size,
             (unsigned long)info.uncompressed_size,
             (unsigned long)output_limit);
      goto out;
    }
    if (result.bytes_consumed != info.compressed_size) {
      printf("gzip input mismatch: consumed=%lu expected=%lu\n",
             (unsigned long)result.bytes_consumed,
             (unsigned long)info.compressed_size);
      goto out;
    }
    if (result.bytes_written != info.uncompressed_size) {
      printf("gzip size mismatch: decoded=%lu expected=%lu\n",
             (unsigned long)result.bytes_written,
             (unsigned long)info.uncompressed_size);
      goto out;
    }
    if (!zz9k_archive_gzip_result_matches_footer(&info, &result)) {
      printf("gzip crc mismatch: decoded=0x%08lx expected=0x%08lx\n",
             (unsigned long)result.checksum,
             (unsigned long)info.crc32);
      goto out;
    }
    printf("gzip test ok: out=%lu crc32=0x%08lx\n",
           (unsigned long)result.bytes_written,
           (unsigned long)result.checksum);
    ok = 1;
    goto out;
  }

  memset(&entry, 0, sizeof(entry));
  strcpy(entry.name, info.name);
  entry.method = ZZ9K_COMPRESSION_GZIP;
  entry.compressed_size = info.compressed_size;
  entry.uncompressed_size = info.uncompressed_size;
  if (info.uncompressed_size == 0U) {
    if (!zz9k_archive_decompress_feed_file_to_result(
            *ctx, service, ZZ9K_COMPRESSION_GZIP, archive_path,
            file_length, output_limit, &result)) {
      printf("gzip feed extract failed: %s packed=%lu unpacked=%lu "
             "limit=%lu\n",
             entry.name,
             (unsigned long)info.compressed_size,
             (unsigned long)info.uncompressed_size,
             (unsigned long)output_limit);
      goto out;
    }
    if (result.bytes_written != 0U) {
      printf("gzip size mismatch: decoded=%lu expected=0\n",
             (unsigned long)result.bytes_written);
      goto out;
    }
    if (!zz9k_archive_gzip_result_matches_footer(&info, &result)) {
      printf("gzip crc mismatch: decoded=0x%08lx expected=0x%08lx\n",
             (unsigned long)result.checksum,
             (unsigned long)info.crc32);
      goto out;
    }
    ok = zz9k_archive_write_entry(output_dir, &entry, 0);
    goto out;
  }
  if (!zz9k_archive_decompress_feed_file_to_file(
          *ctx, service, ZZ9K_COMPRESSION_GZIP, archive_path, file_length,
          info.uncompressed_size, output_dir, &entry, &result)) {
    printf("gzip feed extract failed: %s packed=%lu unpacked=%lu limit=%lu\n",
           entry.name,
           (unsigned long)info.compressed_size,
           (unsigned long)info.uncompressed_size,
           (unsigned long)info.uncompressed_size);
    goto out;
  }
  if (result.bytes_consumed != info.compressed_size) {
    printf("gzip input mismatch: consumed=%lu expected=%lu\n",
           (unsigned long)result.bytes_consumed,
           (unsigned long)info.compressed_size);
    goto out;
  }
  if (result.bytes_written != info.uncompressed_size) {
    printf("gzip size mismatch: decoded=%lu expected=%lu\n",
           (unsigned long)result.bytes_written,
           (unsigned long)info.uncompressed_size);
    goto out;
  }
  if (!zz9k_archive_gzip_result_matches_footer(&info, &result)) {
    printf("gzip crc mismatch: decoded=0x%08lx expected=0x%08lx\n",
           (unsigned long)result.checksum,
           (unsigned long)info.crc32);
    goto out;
  }
  ok = 1;

out:
  return ok;
}

static int zz9k_archive_handle_lzma(ZZ9KContext *ctx,
                                    const ZZ9KServiceInfo *service,
                                    const uint8_t *data,
                                    uint32_t length,
                                    const char *command,
                                    const char *output_dir,
                                    uint32_t requested_capacity)
{
  ZZ9KArchiveLzmaInfo info;
  ZZ9KDecompressResult result;
  uint8_t *decoded = 0;
  uint32_t output_capacity;
  int ok = 0;

  if (!zz9k_archive_lzma_info(data, length, &info)) {
    printf("lzma-alone parse failed\n");
    return 0;
  }
  if (strcmp(command, "l") == 0) {
    if (info.size_known) {
      printf("- %10lu %s\n", (unsigned long)info.uncompressed_size,
             info.name);
    } else {
      printf("-          ? %s\n", info.name);
    }
    return 1;
  }
  if (!zz9k_archive_lzma_output_capacity(&info, requested_capacity,
                                         &output_capacity)) {
    if (!info.size_known) {
      printf("lzma-alone unknown output size requires --capacity bytes\n");
    } else {
      printf("lzma-alone output capacity too small\n");
    }
    return 0;
  }
  if (!ctx || !service) {
    printf("codec service unavailable\n");
    return 0;
  }
  if (strcmp(command, "t") == 0 &&
      zz9k_archive_service_supports_decompress_test(
          service, ZZ9K_COMPRESSION_LZMA_ALONE)) {
    if (!zz9k_archive_decompress_test_to_result(
            ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
            data, length, output_capacity, &result)) {
      return 0;
    }
    if (info.size_known && result.bytes_written != info.uncompressed_size) {
      printf("lzma-alone size mismatch\n");
      return 0;
    }
    printf("lzma-alone test ok: out=%lu crc32=0x%08lx\n",
           (unsigned long)result.bytes_written,
           (unsigned long)result.checksum);
    return 1;
  }
  if (strcmp(command, "x") == 0 &&
      zz9k_archive_service_supports_decompress_feed(
          service, ZZ9K_COMPRESSION_LZMA_ALONE)) {
    ZZ9KArchiveEntry entry;

    memset(&entry, 0, sizeof(entry));
    strcpy(entry.name, info.name);
    entry.method = ZZ9K_COMPRESSION_LZMA_ALONE;
    entry.uncompressed_size = info.size_known ?
        info.uncompressed_size : output_capacity;
    if (!zz9k_archive_decompress_feed_stream_to_file(
            ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
            data, length, output_capacity, output_dir, &entry, &result)) {
      return 0;
    }
    if (info.size_known && result.bytes_written != info.uncompressed_size) {
      printf("lzma-alone size mismatch\n");
      return 0;
    }
    return 1;
  }
  if (strcmp(command, "x") == 0 &&
      zz9k_archive_service_supports_decompress_stream(
          service, ZZ9K_COMPRESSION_LZMA_ALONE)) {
    ZZ9KArchiveEntry entry;

    memset(&entry, 0, sizeof(entry));
    strcpy(entry.name, info.name);
    entry.method = ZZ9K_COMPRESSION_LZMA_ALONE;
    entry.uncompressed_size = info.size_known ?
        info.uncompressed_size : output_capacity;
    if (!zz9k_archive_decompress_stream_to_file(
            ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
            data, length, output_capacity, output_dir, &entry, &result)) {
      return 0;
    }
    if (info.size_known && result.bytes_written != info.uncompressed_size) {
      printf("lzma-alone size mismatch\n");
      return 0;
    }
    return 1;
  }
  if (!zz9k_archive_decompress_to_memory(
          ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
          data, length, output_capacity, &decoded, &result)) {
    return 0;
  }
  if (info.size_known && result.bytes_written != info.uncompressed_size) {
    printf("lzma-alone size mismatch\n");
    goto out;
  }

  if (strcmp(command, "t") == 0) {
    printf("lzma-alone test ok: out=%lu crc32=0x%08lx\n",
           (unsigned long)result.bytes_written,
           (unsigned long)result.checksum);
    ok = 1;
  } else {
    ZZ9KArchiveEntry entry;

    memset(&entry, 0, sizeof(entry));
    strcpy(entry.name, info.name);
    entry.method = ZZ9K_COMPRESSION_LZMA_ALONE;
    entry.uncompressed_size = result.bytes_written;
    ok = zz9k_archive_write_entry(output_dir, &entry, decoded);
  }

out:
  free(decoded);
  return ok;
}

static int zz9k_archive_7z_lzma_alone_header(
    const ZZ9KArchiveEntry *entry,
    uint8_t header[13])
{
  uint32_t i;

  if (!entry || !header ||
      entry->method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA ||
      entry->method_props_size != 5U) {
    return 0;
  }

  memcpy(header, entry->method_props, 5U);
  for (i = 0U; i < 8U; i++) {
    header[5U + i] =
        (uint8_t)(((uint64_t)entry->uncompressed_size >> (i * 8U)) & 0xffU);
  }
  return 1;
}

static int zz9k_archive_7z_build_lzma_alone_payload(
    const ZZ9KArchiveEntry *entry,
    const uint8_t *compressed,
    uint32_t compressed_length,
    uint8_t **payload,
    uint32_t *payload_length)
{
  uint8_t *bytes;

  if (!entry || !compressed || !payload || !payload_length ||
      entry->method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA ||
      entry->method_props_size != 5U ||
      compressed_length != entry->compressed_size ||
      compressed_length > 0x7fffffffUL - 13U) {
    return 0;
  }

  bytes = (uint8_t *)malloc((size_t)compressed_length + 13U);
  if (!bytes) {
    return 0;
  }
  if (!zz9k_archive_7z_lzma_alone_header(entry, bytes)) {
    free(bytes);
    return 0;
  }
  memcpy(bytes + 13U, compressed, compressed_length);
  *payload = bytes;
  *payload_length = compressed_length + 13U;
  return 1;
}

static int zz9k_archive_7z_build_lzma2_payload(
    const ZZ9KArchiveEntry *entry,
    const uint8_t *compressed,
    uint32_t compressed_length,
    uint8_t **payload,
    uint32_t *payload_length)
{
  uint8_t *bytes;

  if (!entry || !compressed || !payload || !payload_length ||
      entry->method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA2 ||
      entry->method_props_size != 1U ||
      compressed_length != entry->compressed_size ||
      compressed_length > 0x7fffffffUL - 1U) {
    return 0;
  }

  bytes = (uint8_t *)malloc((size_t)compressed_length + 1U);
  if (!bytes) {
    return 0;
  }
  bytes[0] = entry->method_props[0];
  memcpy(bytes + 1U, compressed, compressed_length);
  *payload = bytes;
  *payload_length = compressed_length + 1U;
  return 1;
}

static int zz9k_archive_7z_lzma2_feed_output_limit(
    const ZZ9KArchiveEntry *entry,
    uint32_t *output_limit)
{
  if (!entry || !output_limit ||
      entry->method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA2 ||
      entry->uncompressed_size == 0xffffffffUL) {
    return 0;
  }
  *output_limit = entry->uncompressed_size + 1U;
  return *output_limit != 0U;
}

static void zz9k_archive_print_7z_lzma_diag(const ZZ9KArchiveEntry *entry)
{
  uint32_t dict_size;
  uint32_t i;

  if (!entry) {
    return;
  }

  printf("7z LZMA diagnostics: name=%s packed=%lu output=%lu",
         entry->name,
         (unsigned long)entry->compressed_size,
         (unsigned long)entry->uncompressed_size);
  if (zz9k_archive_lzma_props_dict_size(
          entry->method_props, entry->method_props_size, &dict_size)) {
    printf(" dictionary=%lu", (unsigned long)dict_size);
  } else {
    printf(" dictionary=?");
  }
  printf(" props=");
  for (i = 0U; i < entry->method_props_size; i++) {
    printf("%02x", (unsigned int)entry->method_props[i]);
  }
  printf("\n");
}

static void zz9k_archive_print_7z_lzma2_diag(const ZZ9KArchiveEntry *entry)
{
  uint32_t dict_size;

  if (!entry) {
    return;
  }

  printf("7z LZMA2 diagnostics: name=%s packed=%lu output=%lu",
         entry->name,
         (unsigned long)entry->compressed_size,
         (unsigned long)entry->uncompressed_size);
  if (entry->method_props_size == 1U) {
    printf(" prop=0x%02x", (unsigned int)entry->method_props[0]);
    if (zz9k_archive_lzma2_prop_dict_size(
            entry->method_props[0], &dict_size)) {
      if (dict_size == 0xffffffffUL) {
        printf(" LZMA2 dictionary=4GiB");
      } else {
        printf(" LZMA2 dictionary=%lu", (unsigned long)dict_size);
      }
    }
  } else {
    printf(" invalid-props=%lu",
           (unsigned long)entry->method_props_size);
  }
  printf("\n");
}

static int zz9k_archive_7z_lzma2_fallback_file_range(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    const char *archive_path,
    const char *output_dir,
    const ZZ9KArchiveEntry *entry,
    int write_output,
    ZZ9KDecompressResult *result)
{
  uint8_t *packed = 0;
  uint8_t *wrapped = 0;
  uint8_t *decoded = 0;
  uint32_t wrapped_length = 0U;
  int ok = 0;

  if (!ctx || !service || !archive_path || !entry || !result ||
      entry->method != ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
    return 0;
  }
  if (write_output && !output_dir) {
    return 0;
  }
  if (!zz9k_archive_read_file_range(
          archive_path, entry->data_offset, entry->compressed_size,
          &packed)) {
    return 0;
  }
  if (!zz9k_archive_7z_build_lzma2_payload(
          entry, packed, entry->compressed_size, &wrapped,
          &wrapped_length)) {
    goto out;
  }
  if (!zz9k_archive_decompress_to_memory(
          ctx, service, ZZ9K_COMPRESSION_LZMA2,
          wrapped, wrapped_length, entry->uncompressed_size,
          &decoded, result)) {
    goto out;
  }
  if (write_output) {
    if (!zz9k_archive_write_entry(output_dir, entry, decoded)) {
      goto out;
    }
  }
  ok = 1;

out:
  free(decoded);
  free(wrapped);
  free(packed);
  return ok;
}

typedef struct ZZ9KArchive7zSplitWriter {
  const char *output_dir;
  const ZZ9KArchiveEntry *entries;
  uint32_t count;
  uint32_t current;
  uint32_t entry_written;
  uint32_t entry_crc;
  uint32_t total_written;
  FILE *file;
  int write_output;
  int entry_started;
} ZZ9KArchive7zSplitWriter;

static int zz9k_archive_7z_same_split_folder(
    const ZZ9KArchiveEntry *a,
    const ZZ9KArchiveEntry *b)
{
  if (!a || !b ||
      !zz9k_archive_7z_entry_has_split_substream(a) ||
      !zz9k_archive_7z_entry_has_split_substream(b) ||
      a->method != b->method ||
      a->data_offset != b->data_offset ||
      a->compressed_size != b->compressed_size ||
      a->method_props_size != b->method_props_size) {
    return 0;
  }
  if (a->method_props_size != 0U &&
      memcmp(a->method_props, b->method_props, a->method_props_size) != 0) {
    return 0;
  }
  return 1;
}

static uint32_t zz9k_archive_7z_split_group_count(
    const ZZ9KArchiveEntry *entries,
    uint32_t count,
    uint32_t first)
{
  uint32_t group_count = 0U;

  if (!entries || first >= count ||
      !zz9k_archive_7z_entry_has_split_substream(&entries[first])) {
    return 0U;
  }
  while (first + group_count < count &&
         zz9k_archive_7z_same_split_folder(
             &entries[first], &entries[first + group_count])) {
    group_count++;
  }
  return group_count;
}

static int zz9k_archive_7z_split_group_has_match(
    const ZZ9KArchiveEntry *entries,
    uint32_t count)
{
  uint32_t i;

  if (!entries) {
    return 0;
  }
  for (i = 0U; i < count; i++) {
    if (zz9k_archive_entry_matches_filter(&entries[i])) {
      return 1;
    }
  }
  return 0;
}

static int zz9k_archive_7z_split_group_is_safe(
    const ZZ9KArchiveEntry *entries,
    uint32_t count)
{
  uint32_t i;

  if (!entries) {
    return 0;
  }
  for (i = 0U; i < count; i++) {
    if (zz9k_archive_entry_matches_filter(&entries[i]) &&
        !zz9k_archive_path_is_safe(entries[i].name)) {
      printf("unsafe path rejected: %s\n", entries[i].name);
      return 0;
    }
  }
  return 1;
}

static int zz9k_archive_7z_split_group_unpacked_size(
    const ZZ9KArchiveEntry *entries,
    uint32_t count,
    uint32_t *unpacked_size)
{
  uint32_t total = 0U;
  uint32_t i;

  if (!entries || !unpacked_size || count == 0U) {
    return 0;
  }
  for (i = 0U; i < count; i++) {
    if (entries[i].decoded_offset != total ||
        entries[i].uncompressed_size > 0x7fffffffUL - total) {
      return 0;
    }
    total += entries[i].uncompressed_size;
  }
  *unpacked_size = total;
  return 1;
}

static void zz9k_archive_7z_split_writer_init(
    ZZ9KArchive7zSplitWriter *writer,
    const char *output_dir,
    const ZZ9KArchiveEntry *entries,
    uint32_t count,
    int write_output)
{
  memset(writer, 0, sizeof(*writer));
  writer->output_dir = output_dir;
  writer->entries = entries;
  writer->count = count;
  writer->write_output = write_output;
}

static int zz9k_archive_7z_split_writer_close_entry(
    ZZ9KArchive7zSplitWriter *writer)
{
  int ok = 1;

  if (!writer) {
    return 0;
  }
  if (writer->file) {
    if (fclose(writer->file) != 0) {
      ok = 0;
    }
    writer->file = 0;
    if (ok && writer->write_output &&
        !zz9k_archive_last_output_skipped &&
        !zz9k_archive_last_output_dry_run) {
      printf("x %s\n", writer->entries[writer->current].name);
    }
  }
  return ok;
}

static void zz9k_archive_7z_split_writer_cleanup(
    ZZ9KArchive7zSplitWriter *writer)
{
  if (writer && writer->file) {
    fclose(writer->file);
    writer->file = 0;
  }
}

static int zz9k_archive_7z_split_writer_start_entry(
    ZZ9KArchive7zSplitWriter *writer)
{
  const ZZ9KArchiveEntry *entry;

  if (!writer || writer->current >= writer->count) {
    return 0;
  }
  if (writer->entry_started) {
    return 1;
  }
  entry = &writer->entries[writer->current];
  writer->entry_written = 0U;
  writer->entry_crc = 0U;
  if (writer->write_output &&
      zz9k_archive_entry_matches_filter(entry) &&
      !zz9k_archive_open_output_entry(
          writer->output_dir, entry, &writer->file)) {
    return 0;
  }
  writer->entry_started = 1;
  return 1;
}

static int zz9k_archive_7z_split_writer_finish_entry(
    ZZ9KArchive7zSplitWriter *writer)
{
  const ZZ9KArchiveEntry *entry;
  int ok = 1;

  if (!writer || writer->current >= writer->count ||
      !writer->entry_started) {
    return 0;
  }
  entry = &writer->entries[writer->current];
  if (zz9k_archive_entry_matches_filter(entry) &&
      zz9k_archive_entry_has_crc32(entry) &&
      writer->entry_crc != entry->crc32) {
    printf("7z split crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
           entry->name,
           (unsigned long)writer->entry_crc,
           (unsigned long)entry->crc32);
    ok = 0;
  }
  if (!zz9k_archive_7z_split_writer_close_entry(writer)) {
    ok = 0;
  }
  writer->current++;
  writer->entry_written = 0U;
  writer->entry_crc = 0U;
  writer->entry_started = 0;
  return ok;
}

static int zz9k_archive_7z_split_writer_advance_empty(
    ZZ9KArchive7zSplitWriter *writer)
{
  while (writer && writer->current < writer->count &&
         writer->entries[writer->current].uncompressed_size == 0U) {
    if (!zz9k_archive_7z_split_writer_start_entry(writer) ||
        !zz9k_archive_7z_split_writer_finish_entry(writer)) {
      return 0;
    }
  }
  return writer != 0;
}

static int zz9k_archive_7z_split_writer_chunk(void *user,
                                              const uint8_t *data,
                                              uint32_t length)
{
  ZZ9KArchive7zSplitWriter *writer =
      (ZZ9KArchive7zSplitWriter *)user;
  uint32_t pos = 0U;

  if (!writer || (!data && length != 0U) ||
      !zz9k_archive_7z_split_writer_advance_empty(writer)) {
    return 0;
  }

  while (pos < length) {
    const ZZ9KArchiveEntry *entry;
    uint32_t remaining;
    uint32_t part;

    if (writer->current >= writer->count ||
        !zz9k_archive_7z_split_writer_start_entry(writer)) {
      printf("7z split output exceeded expected substreams\n");
      return 0;
    }
    entry = &writer->entries[writer->current];
    remaining = entry->uncompressed_size - writer->entry_written;
    part = length - pos;
    if (part > remaining) {
      part = remaining;
    }
    if (part == 0U) {
      if (!zz9k_archive_7z_split_writer_finish_entry(writer)) {
        return 0;
      }
      continue;
    }
    writer->entry_crc = zz9k_archive_crc32(
        writer->entry_crc, data + pos, part);
    if (writer->file &&
        fwrite(data + pos, 1U, part, writer->file) != part) {
      printf("7z split output write failed: %s\n", entry->name);
      return 0;
    }
    writer->entry_written += part;
    writer->total_written += part;
    pos += part;
    if (writer->entry_written == entry->uncompressed_size &&
        !zz9k_archive_7z_split_writer_finish_entry(writer)) {
      return 0;
    }
  }

  return zz9k_archive_7z_split_writer_advance_empty(writer);
}

static int zz9k_archive_7z_split_writer_finish(
    ZZ9KArchive7zSplitWriter *writer)
{
  if (!zz9k_archive_7z_split_writer_advance_empty(writer)) {
    return 0;
  }
  if (writer->current != writer->count) {
    printf("7z split output truncated: %s\n",
           writer->entries[writer->current].name);
    return 0;
  }
  return 1;
}

static int zz9k_archive_handle_7z_split_group_file(
    ZZ9KContext *ctx,
    const ZZ9KServiceInfo *service,
    const char *command,
    const char *archive_path,
    const char *output_dir,
    const ZZ9KArchiveEntry *entries,
    uint32_t count)
{
  ZZ9KArchiveEntry folder_entry;
  ZZ9KArchive7zSplitWriter writer;
  ZZ9KDecompressResult result;
  uint8_t lzma_header[13];
  const uint8_t *prefix = 0;
  uint32_t prefix_length = 0U;
  uint32_t algorithm = 0U;
  uint32_t unpacked_size = 0U;
  uint32_t output_limit = 0U;
  int is_test;
  int is_extract;
  int ok = 0;

  if (!ctx || !service || !command || !archive_path || !entries ||
      count == 0U) {
    return 0;
  }
  is_test = strcmp(command, "t") == 0;
  is_extract = strcmp(command, "x") == 0;
  if (!is_test && !is_extract) {
    return 0;
  }
  if (!zz9k_archive_7z_split_group_has_match(entries, count)) {
    return 1;
  }
  if (!zz9k_archive_7z_split_group_is_safe(entries, count)) {
    return 0;
  }
  if (!zz9k_archive_7z_split_group_unpacked_size(
          entries, count, &unpacked_size)) {
    printf("7z split group not contiguous: %s\n", entries[0].name);
    return 0;
  }

  folder_entry = entries[0];
  folder_entry.decoded_offset = 0U;
  folder_entry.uncompressed_size = unpacked_size;
  folder_entry.flags &= ~ZZ9K_ARCHIVE_ENTRY_FLAG_CRC32;
  folder_entry.crc32 = 0U;

  if (folder_entry.method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
    algorithm = ZZ9K_COMPRESSION_DEFLATE_RAW;
    output_limit = is_test ?
        zz9k_archive_zip_test_output_limit(unpacked_size) : unpacked_size;
  } else if (folder_entry.method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
    algorithm = ZZ9K_COMPRESSION_LZMA_ALONE;
    output_limit = unpacked_size;
    if (!zz9k_archive_7z_lzma_alone_header(
            &folder_entry, lzma_header)) {
      zz9k_archive_print_7z_lzma_diag(&folder_entry);
      return 0;
    }
    prefix = lzma_header;
    prefix_length = sizeof(lzma_header);
  } else if (folder_entry.method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
    algorithm = ZZ9K_COMPRESSION_LZMA2;
    if (!zz9k_archive_7z_lzma2_feed_output_limit(
            &folder_entry, &output_limit)) {
      zz9k_archive_print_7z_lzma2_diag(&folder_entry);
      return 0;
    }
    prefix = folder_entry.method_props;
    prefix_length = folder_entry.method_props_size;
  } else {
    zz9k_archive_print_7z_entry_unsupported(&folder_entry);
    return 0;
  }

  if (unpacked_size == 0U) {
    zz9k_archive_7z_split_writer_init(
        &writer, output_dir, entries, count, is_extract);
    ok = zz9k_archive_7z_split_writer_finish(&writer);
    zz9k_archive_7z_split_writer_cleanup(&writer);
    return ok;
  }
  if (!zz9k_archive_service_supports_decompress_feed(service, algorithm)) {
    printf("7z compressed multi-substream requires decompress-feed: %s\n",
           folder_entry.name);
    return 0;
  }

  zz9k_archive_7z_split_writer_init(
      &writer, output_dir, entries, count, is_extract);
  if (!zz9k_archive_decompress_feed_file_parts_core(
          ctx, service, algorithm, prefix, prefix_length, archive_path,
          folder_entry.data_offset, folder_entry.compressed_size,
          output_limit, 0, 0, 0, zz9k_archive_7z_split_writer_chunk,
          &writer, &result, 0)) {
    printf("7z compressed multi-substream %s failed: %s\n",
           is_test ? "test" : "extract", folder_entry.name);
    goto out;
  }
  if (!zz9k_archive_7z_split_writer_finish(&writer)) {
    goto out;
  }
  if (result.bytes_written != unpacked_size ||
      writer.total_written != unpacked_size) {
    printf("7z compressed multi-substream size mismatch: %s\n",
           folder_entry.name);
    goto out;
  }
  ok = 1;

out:
  zz9k_archive_7z_split_writer_cleanup(&writer);
  return ok;
}

static int zz9k_archive_7z_decode_encoded_header_from_file(
    ZZ9KContext **ctx,
    ZZ9KServiceInfo *service,
    int *codec_ready,
    const uint8_t *encoded_header,
    uint32_t encoded_header_length,
    const char *archive_path,
    uint32_t archive_length,
    uint8_t **decoded_header,
    uint32_t *decoded_length)
{
  ZZ9KArchiveEntry entry;
  ZZ9KDecompressResult result;
  uint8_t *packed = 0;
  uint8_t *wrapped = 0;
  uint8_t *decoded = 0;
  const uint8_t *codec_input = 0;
  uint32_t codec_input_length = 0U;
  uint32_t algorithm = 0U;
  int ok = 0;

  if (!ctx || !service || !codec_ready || !encoded_header ||
      !archive_path || !decoded_header || !decoded_length) {
    return 0;
  }
  *decoded_header = 0;
  *decoded_length = 0U;
  memset(&result, 0, sizeof(result));
  if (!zz9k_archive_7z_encoded_header_entry(
          encoded_header, encoded_header_length, archive_length, &entry)) {
    return 0;
  }
  if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_COPY) {
    if (entry.compressed_size != entry.uncompressed_size ||
        !zz9k_archive_read_file_range(
            archive_path, entry.data_offset, entry.compressed_size,
            &decoded)) {
      goto out;
    }
    if (zz9k_archive_entry_has_crc32(&entry) &&
        zz9k_archive_crc32(0U, decoded, entry.uncompressed_size) !=
          entry.crc32) {
      printf("7z encoded header crc mismatch: %s\n", archive_path);
      goto out;
    }
    *decoded_header = decoded;
    *decoded_length = entry.uncompressed_size;
    decoded = 0;
    ok = 1;
    goto out;
  }
  if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
    algorithm = ZZ9K_COMPRESSION_DEFLATE_RAW;
  } else if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
    algorithm = ZZ9K_COMPRESSION_LZMA_ALONE;
  } else if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
    algorithm = ZZ9K_COMPRESSION_LZMA2;
  } else {
    printf("unsupported encoded 7z header method: 0x%08lx\n",
           (unsigned long)entry.method);
    goto out;
  }
  if (!zz9k_archive_ensure_codec_open(ctx, service, codec_ready) ||
      !zz9k_archive_read_file_range(
          archive_path, entry.data_offset, entry.compressed_size, &packed)) {
    goto out;
  }
  codec_input = packed;
  codec_input_length = entry.compressed_size;
  if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
    if (!zz9k_archive_7z_build_lzma_alone_payload(
            &entry, packed, entry.compressed_size,
            &wrapped, &codec_input_length)) {
      printf("7z encoded header LZMA decode failed: %s\n", archive_path);
      goto out;
    }
    codec_input = wrapped;
  } else if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
    if (!zz9k_archive_7z_build_lzma2_payload(
            &entry, packed, entry.compressed_size,
            &wrapped, &codec_input_length)) {
      printf("7z encoded header LZMA2 decode failed: %s\n", archive_path);
      goto out;
    }
    codec_input = wrapped;
  }
  if (!zz9k_archive_decompress_to_memory(
          *ctx, service, algorithm, codec_input, codec_input_length,
          entry.uncompressed_size, &decoded, &result)) {
    if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
      printf("7z encoded header Deflate decode failed: %s\n", archive_path);
    } else if (entry.method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
      printf("7z encoded header LZMA decode failed: %s\n", archive_path);
    } else {
      printf("7z encoded header LZMA2 decode failed: %s\n", archive_path);
    }
    goto out;
  }
  if (!zz9k_archive_7z_result_matches_entry(&entry, &result)) {
    printf("7z encoded header crc/size mismatch: %s\n", archive_path);
    goto out;
  }
  *decoded_header = decoded;
  *decoded_length = result.bytes_written;
  decoded = 0;
  ok = 1;

out:
  free(decoded);
  free(wrapped);
  free(packed);
  return ok;
}

static int zz9k_archive_7z_read_header_from_file_with_codec(
    ZZ9KContext **ctx,
    ZZ9KServiceInfo *service,
    int *codec_ready,
    const char *path,
    uint32_t archive_length,
    uint8_t **header_data,
    uint32_t *header_length)
{
  uint8_t *start = 0;
  uint8_t *bytes = 0;
  ZZ9KArchive7zHeader header;
  int ok = 0;

  if (!path || !header_data || !header_length) {
    return 0;
  }
  *header_data = 0;
  *header_length = 0U;
  if (!zz9k_archive_read_file_range(
          path, 0U, ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE, &start)) {
    return 0;
  }
  if (!zz9k_archive_7z_start_header_from_prefix(
          start, ZZ9K_ARCHIVE_7Z_START_HEADER_SIZE,
          archive_length, &header)) {
    goto out;
  }
  if (!zz9k_archive_read_file_range(
          path, header.next_header_offset, header.next_header_size,
          &bytes)) {
    goto out;
  }
  if (zz9k_archive_crc32(0U, bytes, header.next_header_size) !=
      header.next_header_crc) {
    printf("7z header crc mismatch: %s\n", path);
    goto out;
  }
  if (header.next_header_size != 0U &&
      bytes[0] == ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER) {
    uint8_t *decoded = 0;
    uint32_t decoded_size = 0U;

    if (!zz9k_archive_7z_decode_encoded_header_from_file(
            ctx, service, codec_ready, bytes, header.next_header_size,
            path, archive_length, &decoded, &decoded_size)) {
      goto out;
    }
    free(bytes);
    bytes = decoded;
    header.next_header_size = decoded_size;
  }
  *header_data = bytes;
  *header_length = header.next_header_size;
  bytes = 0;
  ok = 1;

out:
  free(bytes);
  free(start);
  return ok;
}

static int zz9k_archive_7z_file_can_handle_entries(
    const ZZ9KArchiveEntry *entries,
    uint32_t count,
    uint32_t archive_length,
    int *needs_deflate,
    int *needs_lzma,
    int *needs_lzma2)
{
  uint32_t i;

  if (!needs_deflate || !needs_lzma || !needs_lzma2) {
    return 0;
  }
  *needs_deflate = 0;
  *needs_lzma = 0;
  *needs_lzma2 = 0;
  if (count == 0U) {
    return 1;
  }
  if (!entries) {
    return 0;
  }
  for (i = 0U; i < count; i++) {
    if (!zz9k_archive_entry_matches_filter(&entries[i])) {
      continue;
    }
    if (entries[i].is_dir) {
      continue;
    }
    if (entries[i].data_offset > archive_length ||
        entries[i].compressed_size >
          archive_length - entries[i].data_offset) {
      return 0;
    }
    if (zz9k_archive_7z_entry_has_split_substream(&entries[i]) &&
        entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_COPY) {
      return 0;
    }
    if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_COPY) {
      if (entries[i].compressed_size != entries[i].uncompressed_size) {
        return 0;
      }
    } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
      *needs_deflate = 1;
    } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
      *needs_lzma = 1;
    } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
      *needs_lzma2 = 1;
    } else {
      return 0;
    }
  }
  return 1;
}

static int zz9k_archive_handle_7z_file(ZZ9KContext **ctx,
                                       ZZ9KServiceInfo *service,
                                       int *codec_ready,
                                       const char *command,
                                       const char *archive_path,
                                       uint32_t archive_length,
                                       const char *output_dir,
                                       int *attempted)
{
  uint8_t *header = 0;
  ZZ9KArchiveEntry *entries = 0;
  uint32_t header_length = 0U;
  uint32_t count = 0U;
  uint32_t i;
  int is_list;
  int is_test;
  int is_extract;
  int needs_deflate = 0;
  int needs_lzma = 0;
  int needs_lzma2 = 0;
  int ok = 1;

  if (!attempted) {
    return 0;
  }
  *attempted = 0;
  if (!ctx || !service || !codec_ready || !archive_path) {
    return 0;
  }
  is_list = command && strcmp(command, "l") == 0;
  is_test = command && strcmp(command, "t") == 0;
  is_extract = command && strcmp(command, "x") == 0;
  if (!is_list && !is_test && !is_extract) {
    return 0;
  }
  if (!zz9k_archive_7z_read_header_from_file_with_codec(
          ctx, service, codec_ready, archive_path, archive_length,
          &header, &header_length)) {
    goto out;
  }
  if (header_length == 0U ||
      header[0] == ZZ9K_ARCHIVE_7Z_ID_ENCODED_HEADER) {
    goto out;
  }
  if (!zz9k_archive_7z_list_from_header(
          header, header_length, archive_length, 0, 0U, &count) ||
      !zz9k_archive_alloc_entries(count, &entries)) {
    goto out;
  }
  if (!zz9k_archive_7z_list_from_header(
          header, header_length, archive_length, entries, count, &count)) {
    goto out;
  }

  if (is_list) {
    *attempted = 1;
    for (i = 0U; i < count; i++) {
      if (!zz9k_archive_entry_matches_filter(&entries[i])) {
        continue;
      }
      zz9k_archive_print_entry(&entries[i]);
    }
    goto out;
  }

  if (!zz9k_archive_7z_file_can_handle_entries(
          entries, count, archive_length, &needs_deflate,
          &needs_lzma, &needs_lzma2)) {
    goto out;
  }
  if (needs_deflate || needs_lzma || needs_lzma2) {
    if (!zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
      *attempted = 1;
      ok = 0;
      goto out;
    }
    if ((needs_deflate &&
         !zz9k_archive_service_supports_decompress_feed(
             service, ZZ9K_COMPRESSION_DEFLATE_RAW)) ||
        (needs_lzma &&
         !zz9k_archive_service_supports_decompress_feed(
             service, ZZ9K_COMPRESSION_LZMA_ALONE)) ||
        (needs_lzma2 &&
         !zz9k_archive_service_supports_decompress_feed(
             service, ZZ9K_COMPRESSION_LZMA2))) {
      goto out;
    }
  }

  *attempted = 1;
  for (i = 0U; i < count; i++) {
    if (zz9k_archive_7z_entry_has_split_substream(&entries[i])) {
      uint32_t group_count;

      group_count = zz9k_archive_7z_split_group_count(entries, count, i);
      if (group_count == 0U) {
        zz9k_archive_print_7z_entry_unsupported(&entries[i]);
        ok = 0;
        continue;
      }
      if (!zz9k_archive_handle_7z_split_group_file(
              *ctx, service, command, archive_path, output_dir,
              &entries[i], group_count)) {
        ok = 0;
      }
      i += group_count - 1U;
      continue;
    }
    if (!zz9k_archive_entry_matches_filter(&entries[i])) {
      continue;
    }
    if (!zz9k_archive_path_is_safe(entries[i].name)) {
      printf("unsafe path rejected: %s\n", entries[i].name);
      ok = 0;
      continue;
    }
    if (entries[i].is_dir) {
      if (is_extract) {
        ok &= zz9k_archive_write_entry(output_dir, &entries[i], header);
      }
    } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_COPY) {
      uint32_t actual_crc = 0U;

      if (!zz9k_archive_7z_copy_file_crc_matches(
              archive_path, &entries[i], &actual_crc)) {
        printf("7z entry crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
               entries[i].name,
               (unsigned long)actual_crc,
               (unsigned long)entries[i].crc32);
        ok = 0;
        continue;
      }
      if (is_extract) {
        ok &= zz9k_archive_write_file_range_entry(
            output_dir, &entries[i], archive_path);
      }
    } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
      ZZ9KDecompressResult result;
      uint32_t output_limit;

      output_limit = zz9k_archive_zip_test_output_limit(
          entries[i].uncompressed_size);
      if (is_test) {
        if (!zz9k_archive_decompress_feed_file_parts_to_result(
                *ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
                0, 0U, archive_path, entries[i].data_offset,
                entries[i].compressed_size, output_limit, &result)) {
          printf("7z Deflate test failed: %s packed=%lu unpacked=%lu "
                 "offset=%lu limit=%lu\n",
                 entries[i].name,
                 (unsigned long)entries[i].compressed_size,
                 (unsigned long)entries[i].uncompressed_size,
                 (unsigned long)entries[i].data_offset,
                 (unsigned long)output_limit);
          ok = 0;
          continue;
        }
      } else if (!zz9k_archive_decompress_feed_file_parts_to_file(
                     *ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
                     0, 0U, archive_path, entries[i].data_offset,
                     entries[i].compressed_size, output_limit,
                     output_dir, &entries[i], &result)) {
        printf("7z Deflate extract failed: %s packed=%lu unpacked=%lu "
               "offset=%lu limit=%lu\n",
               entries[i].name,
               (unsigned long)entries[i].compressed_size,
               (unsigned long)entries[i].uncompressed_size,
               (unsigned long)entries[i].data_offset,
               (unsigned long)output_limit);
        ok = 0;
        continue;
      }
      if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
        if (result.bytes_written == entries[i].uncompressed_size &&
            zz9k_archive_entry_has_crc32(&entries[i])) {
          printf("7z Deflate crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                 entries[i].name,
                 (unsigned long)result.checksum,
                 (unsigned long)entries[i].crc32);
        } else {
          printf("7z Deflate size mismatch: %s\n", entries[i].name);
        }
        ok = 0;
      }
    } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
      uint8_t lzma_header[13];
      ZZ9KDecompressResult result;

      if (!zz9k_archive_7z_lzma_alone_header(&entries[i], lzma_header)) {
        zz9k_archive_print_7z_lzma_diag(&entries[i]);
        printf("7z LZMA %s failed: %s\n",
               is_test ? "test" : "extract", entries[i].name);
        ok = 0;
        continue;
      }
      if (is_test) {
        if (!zz9k_archive_decompress_feed_file_parts_to_result(
                *ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
                lzma_header, sizeof(lzma_header), archive_path,
                entries[i].data_offset, entries[i].compressed_size,
                entries[i].uncompressed_size, &result)) {
          zz9k_archive_print_7z_lzma_diag(&entries[i]);
          printf("7z LZMA test failed: %s\n", entries[i].name);
          ok = 0;
          continue;
        }
      } else if (!zz9k_archive_decompress_feed_file_parts_to_file(
                     *ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
                     lzma_header, sizeof(lzma_header), archive_path,
                     entries[i].data_offset, entries[i].compressed_size,
                     entries[i].uncompressed_size, output_dir, &entries[i],
                     &result)) {
        zz9k_archive_print_7z_lzma_diag(&entries[i]);
        printf("7z LZMA extract failed: %s\n", entries[i].name);
        ok = 0;
        continue;
      }
      if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
        if (result.bytes_written == entries[i].uncompressed_size &&
            zz9k_archive_entry_has_crc32(&entries[i])) {
          printf("7z LZMA crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                 entries[i].name,
                 (unsigned long)result.checksum,
                 (unsigned long)entries[i].crc32);
        } else {
          printf("7z LZMA size mismatch: %s\n", entries[i].name);
        }
        ok = 0;
      }
    } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
      ZZ9KDecompressResult result;
      int failure_status = ZZ9K_STATUS_OK;
      uint32_t lzma2_output_limit;

      if (entries[i].method_props_size != 1U ||
          !zz9k_archive_7z_lzma2_feed_output_limit(
              &entries[i], &lzma2_output_limit)) {
        zz9k_archive_print_7z_lzma2_diag(&entries[i]);
        printf("7z LZMA2 %s failed: %s\n",
               is_test ? "test" : "extract", entries[i].name);
        ok = 0;
        continue;
      }
      if (is_test) {
        if (!zz9k_archive_decompress_feed_file_parts_to_result_status(
                *ctx, service, ZZ9K_COMPRESSION_LZMA2,
                entries[i].method_props, entries[i].method_props_size,
                archive_path, entries[i].data_offset,
                entries[i].compressed_size, lzma2_output_limit,
                &result, &failure_status) &&
            (failure_status != ZZ9K_STATUS_NO_MEMORY ||
             !zz9k_archive_7z_lzma2_fallback_file_range(
                 *ctx, service, archive_path, output_dir, &entries[i],
                 0, &result))) {
          zz9k_archive_print_7z_lzma2_diag(&entries[i]);
          printf("7z LZMA2 test failed: %s\n", entries[i].name);
          ok = 0;
          continue;
        }
      } else if (!zz9k_archive_decompress_feed_file_parts_to_file_status(
                     *ctx, service, ZZ9K_COMPRESSION_LZMA2,
                     entries[i].method_props, entries[i].method_props_size,
                     archive_path, entries[i].data_offset,
                     entries[i].compressed_size, lzma2_output_limit,
                     output_dir, &entries[i], &result, &failure_status) &&
                 (failure_status != ZZ9K_STATUS_NO_MEMORY ||
                  !zz9k_archive_7z_lzma2_fallback_file_range(
                      *ctx, service, archive_path, output_dir, &entries[i],
                      1, &result))) {
        zz9k_archive_print_7z_lzma2_diag(&entries[i]);
        printf("7z LZMA2 extract failed: %s\n", entries[i].name);
        ok = 0;
        continue;
      }
      if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
        if (result.bytes_written == entries[i].uncompressed_size &&
            zz9k_archive_entry_has_crc32(&entries[i])) {
          printf("7z LZMA2 crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                 entries[i].name,
                 (unsigned long)result.checksum,
                 (unsigned long)entries[i].crc32);
        } else {
          printf("7z LZMA2 size mismatch: %s\n", entries[i].name);
        }
        ok = 0;
      }
    } else {
      zz9k_archive_print_7z_entry_unsupported(&entries[i]);
      ok = 0;
    }
  }

out:
  if (*attempted && is_test && ok) {
    printf("7z list ok: %lu entries\n", (unsigned long)count);
  }
  free(entries);
  free(header);
  return ok;
}

static int zz9k_archive_handle_7z_feed_file(ZZ9KContext **ctx,
                                            ZZ9KServiceInfo *service,
                                            int *codec_ready,
                                            const char *command,
                                            const char *archive_path,
                                            uint32_t archive_length,
                                            const char *output_dir,
                                            int *attempted)
{
  return zz9k_archive_handle_7z_file(
      ctx, service, codec_ready, command, archive_path, archive_length,
      output_dir, attempted);
}

static int zz9k_archive_handle_7z(ZZ9KContext **ctx,
                                  ZZ9KServiceInfo *service,
                                  int *codec_ready,
                                  const uint8_t *data,
                                  uint32_t length,
                                  const char *command,
                                  const char *output_dir)
{
  ZZ9KArchiveEntry *entries;
  uint32_t count;
  uint32_t i;
  int ok = 1;

  if (!zz9k_archive_7z_list(data, length, 0, 0U, &count) ||
      !zz9k_archive_alloc_entries(count, &entries)) {
    if (zz9k_archive_7z_header_is_encoded(data, length)) {
      printf("unsupported encoded 7z header\n");
    } else {
      printf("7z parse failed or unsupported layout\n");
    }
    return 0;
  }
  if (!zz9k_archive_7z_list(data, length, entries, count, &count)) {
    printf("7z parse failed or unsupported layout\n");
    free(entries);
    return 0;
  }

  for (i = 0U; i < count; i++) {
    if (!zz9k_archive_entry_matches_filter(&entries[i])) {
      continue;
    }
    if (strcmp(command, "l") == 0) {
      zz9k_archive_print_entry(&entries[i]);
    } else if (strcmp(command, "t") == 0) {
      if (!zz9k_archive_path_is_safe(entries[i].name)) {
        printf("unsafe path rejected: %s\n", entries[i].name);
        ok = 0;
        continue;
      }
      if (zz9k_archive_7z_entry_has_unsupported_split(&entries[i])) {
        zz9k_archive_print_7z_entry_unsupported(&entries[i]);
        ok = 0;
        continue;
      }
      if (!entries[i].is_dir &&
          entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_COPY) {
        uint32_t actual_crc = 0U;

        if (entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset ||
            entries[i].compressed_size != entries[i].uncompressed_size) {
          zz9k_archive_print_7z_entry_unsupported(&entries[i]);
          ok = 0;
          continue;
        }
        if (zz9k_archive_entry_has_crc32(&entries[i])) {
          actual_crc = zz9k_archive_crc32(
              0U, data + entries[i].data_offset,
              entries[i].uncompressed_size);
        }
        if (!zz9k_archive_7z_copy_entry_crc_matches(
                &entries[i], data + entries[i].data_offset)) {
          printf("7z entry crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                 entries[i].name,
                 (unsigned long)actual_crc,
                 (unsigned long)entries[i].crc32);
          ok = 0;
          continue;
        }
      } else if (!entries[i].is_dir &&
                 entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
        uint8_t *decoded = 0;
        ZZ9KDecompressResult result;
        uint32_t output_limit;

        output_limit = zz9k_archive_zip_test_output_limit(
            entries[i].uncompressed_size);
        if (entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset ||
            !zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
          printf("7z Deflate test failed: %s\n", entries[i].name);
          ok = 0;
          free(decoded);
          continue;
        }
        if (zz9k_archive_service_supports_decompress_test(
                service, ZZ9K_COMPRESSION_DEFLATE_RAW)) {
          if (!zz9k_archive_decompress_test_to_result(
                  *ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
                  data + entries[i].data_offset, entries[i].compressed_size,
                  output_limit, &result)) {
            printf("7z Deflate test failed: %s\n", entries[i].name);
            ok = 0;
            continue;
          }
        } else if (!zz9k_archive_decompress_to_memory(
                       *ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
                       data + entries[i].data_offset,
                       entries[i].compressed_size, output_limit,
                       &decoded, &result)) {
          printf("7z Deflate test failed: %s\n", entries[i].name);
          ok = 0;
          free(decoded);
          continue;
        }
        if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
          if (result.bytes_written == entries[i].uncompressed_size &&
              zz9k_archive_entry_has_crc32(&entries[i])) {
            printf("7z Deflate crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                   entries[i].name,
                   (unsigned long)result.checksum,
                   (unsigned long)entries[i].crc32);
          } else {
            printf("7z Deflate size mismatch: %s\n", entries[i].name);
          }
          ok = 0;
        }
        free(decoded);
      } else if (!entries[i].is_dir &&
                 entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
        uint8_t *wrapped = 0;
        uint8_t *decoded = 0;
        uint32_t wrapped_length = 0U;
        ZZ9KDecompressResult result;

        if (entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset ||
            !zz9k_archive_ensure_codec_open(ctx, service, codec_ready) ||
            !zz9k_archive_7z_build_lzma_alone_payload(
                &entries[i], data + entries[i].data_offset,
                entries[i].compressed_size, &wrapped, &wrapped_length)) {
          zz9k_archive_print_7z_lzma_diag(&entries[i]);
          printf("7z LZMA test failed: %s\n", entries[i].name);
          ok = 0;
          free(wrapped);
          free(decoded);
          continue;
        }
        if (zz9k_archive_service_supports_decompress_test(
                service, ZZ9K_COMPRESSION_LZMA_ALONE)) {
          if (!zz9k_archive_decompress_test_to_result(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
                  wrapped, wrapped_length, entries[i].uncompressed_size,
                  &result)) {
            zz9k_archive_print_7z_lzma_diag(&entries[i]);
            printf("7z LZMA test failed: %s\n", entries[i].name);
            ok = 0;
            free(wrapped);
            continue;
          }
        } else if (!zz9k_archive_decompress_to_memory(
                       *ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
                       wrapped, wrapped_length, entries[i].uncompressed_size,
                       &decoded, &result)) {
          zz9k_archive_print_7z_lzma_diag(&entries[i]);
          printf("7z LZMA test failed: %s\n", entries[i].name);
          ok = 0;
          free(wrapped);
          free(decoded);
          continue;
        }
        if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
          if (result.bytes_written == entries[i].uncompressed_size &&
              zz9k_archive_entry_has_crc32(&entries[i])) {
            printf("7z LZMA crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                   entries[i].name,
                   (unsigned long)result.checksum,
                   (unsigned long)entries[i].crc32);
          } else {
            printf("7z LZMA size mismatch: %s\n", entries[i].name);
          }
          ok = 0;
        }
        free(wrapped);
        free(decoded);
      } else if (!entries[i].is_dir &&
                 entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
        uint8_t *wrapped = 0;
        uint8_t *decoded = 0;
        uint32_t wrapped_length = 0U;
        ZZ9KDecompressResult result;

        if (entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset ||
            !zz9k_archive_ensure_codec_open(ctx, service, codec_ready) ||
            !zz9k_archive_7z_build_lzma2_payload(
                &entries[i], data + entries[i].data_offset,
                entries[i].compressed_size, &wrapped, &wrapped_length)) {
          zz9k_archive_print_7z_lzma2_diag(&entries[i]);
          printf("7z LZMA2 test failed: %s\n", entries[i].name);
          ok = 0;
          free(wrapped);
          free(decoded);
          continue;
        }
        if (zz9k_archive_service_supports_decompress_test(
                service, ZZ9K_COMPRESSION_LZMA2)) {
          if (!zz9k_archive_decompress_test_to_result(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA2,
                  wrapped, wrapped_length, entries[i].uncompressed_size,
                  &result)) {
            zz9k_archive_print_7z_lzma2_diag(&entries[i]);
            printf("7z LZMA2 test failed: %s\n", entries[i].name);
            ok = 0;
            free(wrapped);
            continue;
          }
        } else if (!zz9k_archive_decompress_to_memory(
                       *ctx, service, ZZ9K_COMPRESSION_LZMA2,
                       wrapped, wrapped_length, entries[i].uncompressed_size,
                       &decoded, &result)) {
          zz9k_archive_print_7z_lzma2_diag(&entries[i]);
          printf("7z LZMA2 test failed: %s\n", entries[i].name);
          ok = 0;
          free(wrapped);
          free(decoded);
          continue;
        }
        if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
          if (result.bytes_written == entries[i].uncompressed_size &&
              zz9k_archive_entry_has_crc32(&entries[i])) {
            printf("7z LZMA2 crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                   entries[i].name,
                   (unsigned long)result.checksum,
                   (unsigned long)entries[i].crc32);
          } else {
            printf("7z LZMA2 size mismatch: %s\n", entries[i].name);
          }
          ok = 0;
        }
        free(wrapped);
        free(decoded);
      }
    } else {
      if (entries[i].is_dir) {
        ok &= zz9k_archive_write_entry(output_dir, &entries[i], data);
      } else if (zz9k_archive_7z_entry_has_unsupported_split(&entries[i])) {
        zz9k_archive_print_7z_entry_unsupported(&entries[i]);
        ok = 0;
        continue;
      } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_COPY) {
        if (entries[i].compressed_size != entries[i].uncompressed_size ||
            entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset) {
          zz9k_archive_print_7z_entry_unsupported(&entries[i]);
          ok = 0;
          continue;
        }
        if (!zz9k_archive_7z_copy_entry_crc_matches(
                &entries[i], data + entries[i].data_offset)) {
          printf("7z entry crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                 entries[i].name,
                 (unsigned long)zz9k_archive_crc32(
                     0U, data + entries[i].data_offset,
                     entries[i].uncompressed_size),
                 (unsigned long)entries[i].crc32);
          ok = 0;
          continue;
        }
        ok &= zz9k_archive_write_entry(
            output_dir, &entries[i], data + entries[i].data_offset);
      } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_DEFLATE) {
        uint8_t *decoded = 0;
        ZZ9KDecompressResult result;
        uint32_t output_limit;

        output_limit = zz9k_archive_zip_test_output_limit(
            entries[i].uncompressed_size);
        if (entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset ||
            !zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
          printf("7z Deflate extract failed: %s\n", entries[i].name);
          ok = 0;
          continue;
        }
        if (zz9k_archive_service_supports_decompress_feed(
                service, ZZ9K_COMPRESSION_DEFLATE_RAW)) {
          if (!zz9k_archive_decompress_feed_stream_parts_to_file(
                  *ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
                  0, 0U, data + entries[i].data_offset,
                  entries[i].compressed_size, output_limit,
                  output_dir, &entries[i], &result)) {
            printf("7z Deflate extract failed: %s\n", entries[i].name);
            ok = 0;
            continue;
          }
        } else if (zz9k_archive_service_supports_decompress_stream(
                service, ZZ9K_COMPRESSION_DEFLATE_RAW)) {
          if (!zz9k_archive_decompress_stream_to_file(
                  *ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
                  data + entries[i].data_offset, entries[i].compressed_size,
                  output_limit, output_dir, &entries[i], &result)) {
            printf("7z Deflate extract failed: %s\n", entries[i].name);
            ok = 0;
            continue;
          }
        } else if (!zz9k_archive_decompress_to_memory(
                       *ctx, service, ZZ9K_COMPRESSION_DEFLATE_RAW,
                       data + entries[i].data_offset,
                       entries[i].compressed_size, output_limit,
                       &decoded, &result)) {
          printf("7z Deflate extract failed: %s\n", entries[i].name);
          ok = 0;
          free(decoded);
          continue;
        }
        if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
          if (result.bytes_written == entries[i].uncompressed_size &&
              zz9k_archive_entry_has_crc32(&entries[i])) {
            printf("7z Deflate crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                   entries[i].name,
                   (unsigned long)result.checksum,
                   (unsigned long)entries[i].crc32);
          } else {
            printf("7z Deflate size mismatch: %s\n", entries[i].name);
          }
          ok = 0;
        } else if (decoded) {
          entries[i].uncompressed_size = result.bytes_written;
          ok &= zz9k_archive_write_entry(output_dir, &entries[i], decoded);
        }
        free(decoded);
      } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA) {
        uint8_t lzma_header[13];
        uint8_t *wrapped = 0;
        uint8_t *decoded = 0;
        uint32_t wrapped_length = 0U;
        ZZ9KDecompressResult result;

        if (entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset ||
            !zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
          zz9k_archive_print_7z_lzma_diag(&entries[i]);
          printf("7z LZMA extract failed: %s\n", entries[i].name);
          ok = 0;
          continue;
        }
        if (zz9k_archive_service_supports_decompress_feed(
                service, ZZ9K_COMPRESSION_LZMA_ALONE)) {
          if (!zz9k_archive_7z_lzma_alone_header(
                  &entries[i], lzma_header) ||
              !zz9k_archive_decompress_feed_stream_parts_to_file(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
                  lzma_header, sizeof(lzma_header),
                  data + entries[i].data_offset, entries[i].compressed_size,
                  entries[i].uncompressed_size, output_dir, &entries[i],
                  &result)) {
            zz9k_archive_print_7z_lzma_diag(&entries[i]);
            printf("7z LZMA extract failed: %s\n", entries[i].name);
            ok = 0;
            continue;
          }
        } else if (zz9k_archive_service_supports_decompress_stream(
                service, ZZ9K_COMPRESSION_LZMA_ALONE)) {
          if (!zz9k_archive_7z_build_lzma_alone_payload(
                  &entries[i], data + entries[i].data_offset,
                  entries[i].compressed_size, &wrapped, &wrapped_length)) {
            zz9k_archive_print_7z_lzma_diag(&entries[i]);
            printf("7z LZMA extract failed: %s\n", entries[i].name);
            ok = 0;
            continue;
          }
          if (!zz9k_archive_decompress_stream_to_file(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
                  wrapped, wrapped_length, entries[i].uncompressed_size,
                  output_dir, &entries[i], &result)) {
            zz9k_archive_print_7z_lzma_diag(&entries[i]);
            printf("7z LZMA extract failed: %s\n", entries[i].name);
            ok = 0;
            free(wrapped);
            continue;
          }
        } else {
          if (!zz9k_archive_7z_build_lzma_alone_payload(
                  &entries[i], data + entries[i].data_offset,
                  entries[i].compressed_size, &wrapped, &wrapped_length) ||
              !zz9k_archive_decompress_to_memory(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA_ALONE,
                  wrapped, wrapped_length, entries[i].uncompressed_size,
                  &decoded, &result)) {
            zz9k_archive_print_7z_lzma_diag(&entries[i]);
            printf("7z LZMA extract failed: %s\n", entries[i].name);
            ok = 0;
            free(wrapped);
            free(decoded);
            continue;
          }
        }
        if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
          if (result.bytes_written == entries[i].uncompressed_size &&
              zz9k_archive_entry_has_crc32(&entries[i])) {
            printf("7z LZMA crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                   entries[i].name,
                   (unsigned long)result.checksum,
                   (unsigned long)entries[i].crc32);
          } else {
            printf("7z LZMA size mismatch: %s\n", entries[i].name);
          }
          ok = 0;
        } else if (decoded) {
          entries[i].uncompressed_size = result.bytes_written;
          ok &= zz9k_archive_write_entry(output_dir, &entries[i], decoded);
        }
        free(wrapped);
        free(decoded);
      } else if (entries[i].method == ZZ9K_ARCHIVE_7Z_METHOD_LZMA2) {
        uint8_t *wrapped = 0;
        uint8_t *decoded = 0;
        uint32_t wrapped_length = 0U;
        uint32_t lzma2_output_limit;
        ZZ9KDecompressResult result;

        if (entries[i].data_offset > length ||
            entries[i].compressed_size > length - entries[i].data_offset ||
            !zz9k_archive_ensure_codec_open(ctx, service, codec_ready)) {
          zz9k_archive_print_7z_lzma2_diag(&entries[i]);
          printf("7z LZMA2 extract failed: %s\n", entries[i].name);
          ok = 0;
          continue;
        }
        if (zz9k_archive_service_supports_decompress_feed(
                service, ZZ9K_COMPRESSION_LZMA2)) {
          if (entries[i].method_props_size != 1U ||
              !zz9k_archive_7z_lzma2_feed_output_limit(
                  &entries[i], &lzma2_output_limit) ||
              !zz9k_archive_decompress_feed_stream_parts_to_file(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA2,
                  entries[i].method_props, entries[i].method_props_size,
                  data + entries[i].data_offset, entries[i].compressed_size,
                  lzma2_output_limit, output_dir, &entries[i],
                  &result)) {
            zz9k_archive_print_7z_lzma2_diag(&entries[i]);
            printf("7z LZMA2 extract failed: %s\n", entries[i].name);
            ok = 0;
            continue;
          }
        } else if (zz9k_archive_service_supports_decompress_stream(
                service, ZZ9K_COMPRESSION_LZMA2)) {
          if (!zz9k_archive_7z_build_lzma2_payload(
                  &entries[i], data + entries[i].data_offset,
                  entries[i].compressed_size, &wrapped, &wrapped_length)) {
            zz9k_archive_print_7z_lzma2_diag(&entries[i]);
            printf("7z LZMA2 extract failed: %s\n", entries[i].name);
            ok = 0;
            continue;
          }
          if (!zz9k_archive_decompress_stream_to_file(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA2,
                  wrapped, wrapped_length, entries[i].uncompressed_size,
                  output_dir, &entries[i], &result)) {
            zz9k_archive_print_7z_lzma2_diag(&entries[i]);
            printf("7z LZMA2 extract failed: %s\n", entries[i].name);
            ok = 0;
            free(wrapped);
            continue;
          }
        } else {
          if (!zz9k_archive_7z_build_lzma2_payload(
                  &entries[i], data + entries[i].data_offset,
                  entries[i].compressed_size, &wrapped, &wrapped_length) ||
              !zz9k_archive_decompress_to_memory(
                  *ctx, service, ZZ9K_COMPRESSION_LZMA2,
                  wrapped, wrapped_length, entries[i].uncompressed_size,
                  &decoded, &result)) {
            zz9k_archive_print_7z_lzma2_diag(&entries[i]);
            printf("7z LZMA2 extract failed: %s\n", entries[i].name);
            ok = 0;
            free(wrapped);
            free(decoded);
            continue;
          }
        }
        if (!zz9k_archive_7z_result_matches_entry(&entries[i], &result)) {
          if (result.bytes_written == entries[i].uncompressed_size &&
              zz9k_archive_entry_has_crc32(&entries[i])) {
            printf("7z LZMA2 crc mismatch: %s decoded=0x%08lx expected=0x%08lx\n",
                   entries[i].name,
                   (unsigned long)result.checksum,
                   (unsigned long)entries[i].crc32);
          } else {
            printf("7z LZMA2 size mismatch: %s\n", entries[i].name);
          }
          ok = 0;
        } else if (decoded) {
          entries[i].uncompressed_size = result.bytes_written;
          ok &= zz9k_archive_write_entry(output_dir, &entries[i], decoded);
        }
        free(wrapped);
        free(decoded);
      } else {
        zz9k_archive_print_7z_entry_unsupported(&entries[i]);
        ok = 0;
      }
    }
  }
  if (strcmp(command, "t") == 0 && ok) {
    printf("7z list ok: %lu entries\n", (unsigned long)count);
  }
  free(entries);
  return ok;
}

static int zz9k_archive_run(const char *command, const char *archive_path,
                            const char *output_dir,
                            uint32_t lzma_capacity)
{
  uint8_t *data = 0;
  uint8_t probe[ZZ9K_ARCHIVE_PROBE_BYTES];
  uint32_t probe_length = 0U;
  uint32_t file_length = 0U;
  uint32_t length = 0U;
  ZZ9KArchiveFormat format;
  ZZ9KContext *ctx = 0;
  ZZ9KServiceInfo service;
  int need_codec;
  int codec_ready = 0;
  int status;
  int ok = 0;

  memset(&service, 0, sizeof(service));
  if (!zz9k_archive_probe_file(archive_path, probe, sizeof(probe),
                               &probe_length, &file_length)) {
    return 0;
  }
  format = zz9k_archive_detect_format(probe, probe_length);
  printf("archive: %s (%s)\n", archive_path, zz9k_archive_format_name(format));

  if (format == ZZ9K_ARCHIVE_FORMAT_LZMA_ALONE &&
      (strcmp(command, "t") == 0 || strcmp(command, "x") == 0)) {
    ZZ9KArchiveLzmaInfo info;
    ZZ9KArchiveEntry entry;
    ZZ9KDecompressResult result;
    uint32_t output_capacity;

    if (!zz9k_archive_lzma_info_from_header(
            probe, probe_length, file_length, &info)) {
      printf("lzma-alone parse failed\n");
      goto out;
    }
    if (!zz9k_archive_lzma_output_capacity(&info, lzma_capacity,
                                           &output_capacity)) {
      if (!info.size_known) {
        printf("lzma-alone unknown output size requires --capacity bytes\n");
      } else {
        printf("lzma-alone output capacity too small\n");
      }
      goto out;
    }
    status = zz9k_open(&ctx);
    if (status != ZZ9K_STATUS_OK) {
      printf("open failed: %s (%d)\n", zz9k_status_name(status), status);
      goto out;
    }
    if (!zz9k_archive_require_codec_service(ctx, &service)) {
      goto out;
    }
    codec_ready = 1;
    if (zz9k_archive_service_supports_decompress_feed(
            &service, ZZ9K_COMPRESSION_LZMA_ALONE)) {
      if (strcmp(command, "t") == 0) {
        if (!zz9k_archive_decompress_feed_file_to_result(
                ctx, &service, ZZ9K_COMPRESSION_LZMA_ALONE,
                archive_path, file_length, output_capacity, &result)) {
          goto out;
        }
      } else {
        memset(&entry, 0, sizeof(entry));
        strcpy(entry.name, info.name);
        entry.method = ZZ9K_COMPRESSION_LZMA_ALONE;
        entry.uncompressed_size = info.size_known ?
            info.uncompressed_size : output_capacity;
        if (!zz9k_archive_decompress_feed_file_to_file(
                ctx, &service, ZZ9K_COMPRESSION_LZMA_ALONE,
                archive_path, file_length, output_capacity,
                output_dir, &entry, &result)) {
          goto out;
        }
      }
      if (info.size_known && result.bytes_written != info.uncompressed_size) {
        printf("lzma-alone size mismatch\n");
        goto out;
      }
      if (strcmp(command, "t") == 0) {
        printf("lzma-alone test ok: out=%lu crc32=0x%08lx\n",
               (unsigned long)result.bytes_written,
               (unsigned long)result.checksum);
      }
      ok = 1;
      goto out;
    }
  }

  if (format == ZZ9K_ARCHIVE_FORMAT_7Z) {
    int file_attempted = 0;
    int file_ok = zz9k_archive_handle_7z_file(
        &ctx, &service, &codec_ready, command, archive_path, file_length,
        output_dir, &file_attempted);

    if (file_attempted) {
      ok = file_ok;
      goto out;
    }
  }

  if (format == ZZ9K_ARCHIVE_FORMAT_GZIP) {
    int file_attempted = 0;
    int file_ok = zz9k_archive_handle_gzip_file(
        &ctx, &service, &codec_ready, archive_path, probe, probe_length,
        file_length, command, output_dir, &file_attempted);

    if (file_attempted) {
      ok = file_ok;
      goto out;
    }
  }

  if (format == ZZ9K_ARCHIVE_FORMAT_ZIP) {
    int file_attempted = 0;
    int file_ok = zz9k_archive_handle_zip_file(
        &ctx, &service, &codec_ready, archive_path, file_length,
        command, output_dir, &file_attempted);

    if (file_attempted) {
      ok = file_ok;
      goto out;
    }
  }

  if (!zz9k_archive_read_file(archive_path, &data, &length)) {
    goto out;
  }
  format = zz9k_archive_detect_format(data, length);

  need_codec = format == ZZ9K_ARCHIVE_FORMAT_GZIP ||
               (strcmp(command, "l") != 0 &&
                 format == ZZ9K_ARCHIVE_FORMAT_LZMA_ALONE) ||
                (strcmp(command, "l") != 0 &&
                 format == ZZ9K_ARCHIVE_FORMAT_ZIP);
  if (need_codec && !codec_ready) {
    status = zz9k_open(&ctx);
    if (status != ZZ9K_STATUS_OK) {
      printf("open failed: %s (%d)\n", zz9k_status_name(status), status);
      goto out;
    }
    if (!zz9k_archive_require_codec_service(ctx, &service)) {
      goto out;
    }
    codec_ready = 1;
  }

  if (format == ZZ9K_ARCHIVE_FORMAT_GZIP) {
    ok = zz9k_archive_handle_gzip(ctx, &service, data, length,
                                  command, output_dir);
  } else if (format == ZZ9K_ARCHIVE_FORMAT_ZIP) {
    ok = zz9k_archive_handle_zip(ctx, need_codec ? &service : 0,
                                 data, length, command, output_dir);
  } else if (format == ZZ9K_ARCHIVE_FORMAT_TAR) {
    ok = zz9k_archive_handle_tar(ctx, &service, data, length,
                                 command, output_dir);
  } else if (format == ZZ9K_ARCHIVE_FORMAT_LHA) {
    ok = zz9k_archive_handle_lha(data, length, command, output_dir);
  } else if (format == ZZ9K_ARCHIVE_FORMAT_LZMA_ALONE) {
    ok = zz9k_archive_handle_lzma(ctx, need_codec ? &service : 0,
                                  data, length, command, output_dir,
                                  lzma_capacity);
  } else if (format == ZZ9K_ARCHIVE_FORMAT_7Z) {
    ok = zz9k_archive_handle_7z(&ctx, &service, &codec_ready, data, length,
                                command, output_dir);
  } else {
    printf("unsupported archive format\n");
    ok = 0;
  }

out:
  if (ctx) {
    zz9k_close(ctx);
  }
  free(data);
  return ok;
}

#ifndef ZZ9K_ARCHIVE_NO_MAIN
static void usage(const char *name)
{
  printf("usage: %s l|t|x [-o output-dir] [--capacity bytes] [--match text] "
         "[--strip-components n] [--dry-run] "
         "[--overwrite|--skip-existing] <archive>\n",
         name);
  printf("       commands: l=list, t=test, x=extract\n");
  printf("       formats: gzip, lzma-alone, tar, tar.gz, lha -lh0-, "
         "zip store/deflate; 7z Copy/Deflate/LZMA/LZMA2 unencoded headers\n");
}

int main(int argc, char **argv)
{
  const char *command = 0;
  const char *archive_path = 0;
  const char *output_dir = "";
  uint32_t lzma_capacity = 0U;
  int arg;

  if (argc < 3) {
    usage(argv[0]);
    return 2;
  }
  command = argv[1];
  if (strcmp(command, "l") != 0 &&
      strcmp(command, "t") != 0 &&
      strcmp(command, "x") != 0) {
    usage(argv[0]);
    return 2;
  }

  arg = 2;
  while (arg < argc) {
    if (strcmp(argv[arg], "-o") == 0) {
      arg++;
      if (arg >= argc) {
        usage(argv[0]);
        return 2;
      }
      output_dir = argv[arg++];
    } else if (strcmp(argv[arg], "--capacity") == 0) {
      char *end = 0;
      unsigned long parsed;

      arg++;
      if (arg >= argc) {
        usage(argv[0]);
        return 2;
      }
      parsed = strtoul(argv[arg], &end, 0);
      if (!end || *end != '\0' || parsed == 0UL ||
          parsed > 0x7fffffffUL) {
        usage(argv[0]);
        return 2;
      }
      lzma_capacity = (uint32_t)parsed;
      arg++;
    } else if (strcmp(argv[arg], "--match") == 0) {
      arg++;
      if (arg >= argc || argv[arg][0] == '\0') {
        usage(argv[0]);
        return 2;
      }
      zz9k_archive_match_filter = argv[arg++];
    } else if (strcmp(argv[arg], "--strip-components") == 0) {
      char *end = 0;
      unsigned long parsed;

      arg++;
      if (arg >= argc) {
        usage(argv[0]);
        return 2;
      }
      parsed = strtoul(argv[arg], &end, 0);
      if (!end || *end != '\0' || parsed > 255UL) {
        usage(argv[0]);
        return 2;
      }
      zz9k_archive_strip_components = (uint32_t)parsed;
      arg++;
    } else if (strcmp(argv[arg], "--overwrite") == 0) {
      zz9k_archive_overwrite_outputs = 1;
      zz9k_archive_skip_existing_outputs = 0;
      arg++;
    } else if (strcmp(argv[arg], "--skip-existing") == 0) {
      zz9k_archive_skip_existing_outputs = 1;
      zz9k_archive_overwrite_outputs = 0;
      arg++;
    } else if (strcmp(argv[arg], "--dry-run") == 0) {
      zz9k_archive_dry_run_outputs = 1;
      arg++;
    } else {
      if (archive_path) {
        usage(argv[0]);
        return 2;
      }
      archive_path = argv[arg++];
    }
  }
  if (!archive_path) {
    usage(argv[0]);
    return 2;
  }

  return zz9k_archive_run(command, archive_path, output_dir,
                          lzma_capacity) ? 0 : 1;
}
#endif
