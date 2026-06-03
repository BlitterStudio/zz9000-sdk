/*
 * Logic checks for zz9k-view image launcher.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define ZZ9K_VIEW_NO_MAIN 1
#include "../tools/zz9k-view.c"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	uint8_t jpeg_header[4] = {0xff, 0xd8, 0xff, 0xe0};
	uint8_t png_header[8] = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
	};
	uint8_t unknown_header[8] = {0};
	char command[128];

	if (zz9k_view_detect_codec(jpeg_header, sizeof(jpeg_header)) !=
	    ZZ9K_VIEW_CODEC_JPEG) {
		printf("did not detect JPEG header\n");
		return 1;
	}
	if (zz9k_view_detect_codec(png_header, sizeof(png_header)) !=
	    ZZ9K_VIEW_CODEC_PNG) {
		printf("did not detect PNG header\n");
		return 2;
	}
	if (zz9k_view_detect_codec(unknown_header, sizeof(unknown_header)) !=
	    ZZ9K_VIEW_CODEC_UNKNOWN) {
		printf("accepted unknown picture header\n");
		return 3;
	}
	if (!zz9k_view_build_command(ZZ9K_VIEW_CODEC_JPEG,
	                             "Work:Pictures/Test Image.jpg",
	                             command, sizeof(command)) ||
	    strcmp(command,
	           "zz9k-jpeg --view \"Work:Pictures/Test Image.jpg\"") != 0) {
		printf("did not build JPEG view command: %s\n", command);
		return 4;
	}
	if (!zz9k_view_build_command(ZZ9K_VIEW_CODEC_PNG,
	                             "Work:Pictures/Test.png",
	                             command, sizeof(command)) ||
	    strcmp(command, "zz9k-png --view \"Work:Pictures/Test.png\"") != 0) {
		printf("did not build PNG view command: %s\n", command);
		return 5;
	}
	if (zz9k_view_build_command(ZZ9K_VIEW_CODEC_UNKNOWN,
	                            "Work:Pictures/Test.bin",
	                            command, sizeof(command))) {
		printf("accepted unknown codec command\n");
		return 6;
	}
	if (zz9k_view_build_command(ZZ9K_VIEW_CODEC_JPEG,
	                            "Work:Bad\"Name.jpg",
	                            command, sizeof(command))) {
		printf("accepted unquotable path\n");
		return 7;
	}

	return 0;
}
