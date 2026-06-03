/*
 * Logic checks for shared zz9k-view picture viewer helpers.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "../tools/zz9k-picture-viewer.h"

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
	char *viewer_argv[4] = {
		"zz9k-view",
		"Work:Pictures/test.jpg",
		"Work:Pictures/alpha test.png",
		"RAM:bad.bin"
	};
	char *empty_argv[1] = {"zz9k-view"};
	char *option_argv[2] = {"zz9k-view", "--resize"};
	char *null_file_argv[2] = {"zz9k-view", 0};
	char *empty_file_argv[2] = {"zz9k-view", ""};
	ZZ9KPictureViewerArgs args;
	ZZ9KPictureViewerImage image;
	char title[128];

	if (zz9k_picture_viewer_detect_codec(jpeg_header, sizeof(jpeg_header)) !=
	    ZZ9K_PICTURE_VIEWER_CODEC_JPEG) {
		printf("did not detect JPEG header\n");
		return 1;
	}
	if (zz9k_picture_viewer_detect_codec(png_header, sizeof(png_header)) !=
	    ZZ9K_PICTURE_VIEWER_CODEC_PNG) {
		printf("did not detect PNG header\n");
		return 2;
	}
	if (zz9k_picture_viewer_detect_codec(unknown_header,
	                                     sizeof(unknown_header)) !=
	    ZZ9K_PICTURE_VIEWER_CODEC_UNKNOWN) {
		printf("accepted unknown picture header\n");
		return 3;
	}

	memset(&args, 0, sizeof(args));
	if (!zz9k_picture_viewer_parse_args(4, viewer_argv, &args) ||
	    args.file_count != 3U ||
	    strcmp(args.files[1], "Work:Pictures/alpha test.png") != 0) {
		printf("did not parse viewer file list\n");
		return 4;
	}
	if (zz9k_picture_viewer_parse_args(1, empty_argv, &args)) {
		printf("accepted empty viewer file list\n");
		return 5;
	}
	if (zz9k_picture_viewer_parse_args(2, option_argv, &args)) {
		printf("accepted unsupported viewer option\n");
		return 6;
	}
	if (zz9k_picture_viewer_parse_args(2, 0, &args)) {
		printf("accepted null viewer argv\n");
		return 11;
	}
	if (zz9k_picture_viewer_parse_args(4, viewer_argv, 0)) {
		printf("accepted null viewer args output\n");
		return 12;
	}
	if (zz9k_picture_viewer_parse_args(2, null_file_argv, &args)) {
		printf("accepted null viewer file arg\n");
		return 13;
	}
	if (zz9k_picture_viewer_parse_args(2, empty_file_argv, &args)) {
		printf("accepted empty viewer file arg\n");
		return 14;
	}

	if (strcmp(zz9k_picture_viewer_basename("Work:Pictures/test.jpg"),
	           "test.jpg") != 0 ||
	    strcmp(zz9k_picture_viewer_basename("SYS:Prefs/alpha test.png"),
	           "alpha test.png") != 0 ||
	    strcmp(zz9k_picture_viewer_basename(
		           "Work:Pictures\\nested\\test.png"),
	           "test.png") != 0 ||
	    strcmp(zz9k_picture_viewer_basename("plain.jpg"),
	           "plain.jpg") != 0) {
		printf("did not format viewer basenames\n");
		return 7;
	}

	zz9k_picture_viewer_image_init(&image);
	image.codec = ZZ9K_PICTURE_VIEWER_CODEC_PNG;
	image.path = "Work:Pictures/alpha test.png";
	image.width = 640U;
	image.height = 480U;
	if (!zz9k_picture_viewer_format_title(title, sizeof(title), 2U, 3U,
	                                      &image) ||
	    strcmp(title,
	           "ZZ9000 View 2/3 PNG 640 x 480 - alpha test.png") != 0) {
		printf("did not format viewer title: %s\n", title);
		return 8;
	}

	if (zz9k_picture_viewer_next_index(2U, 3U) != 0U ||
	    zz9k_picture_viewer_previous_index(0U, 3U) != 2U ||
	    zz9k_picture_viewer_next_index(0U, 0U) != 0U ||
	    zz9k_picture_viewer_previous_index(0U, 0U) != 0U ||
	    zz9k_picture_viewer_next_index(0U, 1U) != 0U ||
	    zz9k_picture_viewer_previous_index(0U, 1U) != 0U) {
		printf("did not wrap viewer navigation\n");
		return 9;
	}

	if (zz9k_picture_viewer_action_from_keys('q', 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_QUIT ||
	    zz9k_picture_viewer_action_from_keys('Q', 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_QUIT ||
	    zz9k_picture_viewer_action_from_keys(0x1bU, 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_QUIT ||
	    zz9k_picture_viewer_action_from_keys(0U, 0x45U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_QUIT ||
	    zz9k_picture_viewer_action_from_keys(' ', 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_NEXT ||
	    zz9k_picture_viewer_action_from_keys(0U, 0x4dU) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_NEXT ||
	    zz9k_picture_viewer_action_from_keys(0U, 0x4eU) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_NEXT ||
	    zz9k_picture_viewer_action_from_keys(0x08U, 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_PREVIOUS ||
	    zz9k_picture_viewer_action_from_keys(0U, 0x41U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_PREVIOUS ||
	    zz9k_picture_viewer_action_from_keys(0U, 0x4cU) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_PREVIOUS ||
	    zz9k_picture_viewer_action_from_keys(0U, 0x4fU) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_PREVIOUS ||
	    zz9k_picture_viewer_action_from_keys('r', 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_REDRAW ||
	    zz9k_picture_viewer_action_from_keys('R', 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_REDRAW ||
	    zz9k_picture_viewer_action_from_keys('x', 0U) !=
	        ZZ9K_PICTURE_VIEWER_ACTION_NONE) {
		printf("did not map viewer keys\n");
		return 10;
	}

	return 0;
}
