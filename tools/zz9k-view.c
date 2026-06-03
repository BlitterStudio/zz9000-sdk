/*
 * Small image viewer launcher for the ZZ9000 SDK image tools.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum ZZ9KViewCodec {
	ZZ9K_VIEW_CODEC_UNKNOWN = 0,
	ZZ9K_VIEW_CODEC_JPEG,
	ZZ9K_VIEW_CODEC_PNG
} ZZ9KViewCodec;

static ZZ9KViewCodec zz9k_view_detect_codec(const uint8_t *bytes,
                                            uint32_t length)
{
	static const uint8_t png_signature[8] = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
	};

	if (!bytes)
		return ZZ9K_VIEW_CODEC_UNKNOWN;
	if (length >= 3U &&
	    bytes[0] == 0xffU && bytes[1] == 0xd8U && bytes[2] == 0xffU) {
		return ZZ9K_VIEW_CODEC_JPEG;
	}
	if (length >= (uint32_t)sizeof(png_signature) &&
	    memcmp(bytes, png_signature, sizeof(png_signature)) == 0) {
		return ZZ9K_VIEW_CODEC_PNG;
	}
	return ZZ9K_VIEW_CODEC_UNKNOWN;
}

static const char *zz9k_view_tool_name(ZZ9KViewCodec codec)
{
	switch (codec) {
	case ZZ9K_VIEW_CODEC_JPEG:
		return "zz9k-jpeg";
	case ZZ9K_VIEW_CODEC_PNG:
		return "zz9k-png";
	default:
		return 0;
	}
}

static int zz9k_view_path_is_quotable(const char *path)
{
	const unsigned char *cursor;

	if (!path || path[0] == '\0')
		return 0;
	cursor = (const unsigned char *)path;
	while (*cursor) {
		if (*cursor == '"' || *cursor == '\r' || *cursor == '\n')
			return 0;
		cursor++;
	}
	return 1;
}

static int zz9k_view_build_command(ZZ9KViewCodec codec,
                                   const char *path,
                                   char *command,
                                   size_t command_capacity)
{
	const char *tool;
	int needed;

	tool = zz9k_view_tool_name(codec);
	if (!tool || !command || command_capacity == 0U ||
	    !zz9k_view_path_is_quotable(path)) {
		return 0;
	}

	needed = snprintf(command, command_capacity, "%s --view \"%s\"",
	                  tool, path);
	return needed > 0 && (size_t)needed < command_capacity;
}

#ifndef ZZ9K_VIEW_NO_MAIN
static void zz9k_view_usage(void)
{
	printf("usage: zz9k-view file.jpg|file.png\n");
}

int main(int argc, char **argv)
{
	uint8_t header[16];
	FILE *file;
	size_t bytes_read;
	ZZ9KViewCodec codec;
	char command[512];
	int status;

	if (argc != 2) {
		zz9k_view_usage();
		return 2;
	}

	file = fopen(argv[1], "rb");
	if (!file) {
		printf("zz9k-view: failed to open '%s'\n", argv[1]);
		return 1;
	}
	bytes_read = fread(header, 1U, sizeof(header), file);
	fclose(file);

	codec = zz9k_view_detect_codec(header, (uint32_t)bytes_read);
	if (codec == ZZ9K_VIEW_CODEC_UNKNOWN) {
		printf("zz9k-view: unsupported image type for '%s'\n", argv[1]);
		return 1;
	}
	if (!zz9k_view_build_command(codec, argv[1], command,
	                             sizeof(command))) {
		printf("zz9k-view: could not build viewer command for '%s'\n",
		       argv[1]);
		return 1;
	}

	printf("zz9k-view: %s\n", command);
	status = system(command);
	return status == 0 ? 0 : 1;
}
#endif
