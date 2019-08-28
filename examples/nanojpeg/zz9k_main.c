/*
 * MNT ZZ9000 Amiga Graphics and ARM Coprocessor SDK
 *            Code example: "nanojpeg"
 *
 * Copyright (C) 2019, Lukas F. Hartmann <lukas@mntre.com>
 *                     MNT Research GmbH, Berlin
 *                     https://mntre.com
 *
 * More Info: https://mntre.com/zz9000
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * GNU General Public License v3.0 or later
 *
 * https://spdx.org/licenses/GPL-3.0-or-later.html
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "printf/printf.h"
#include "zz9k_env.h"

struct ZZ9K_ENV* _zz9k_env;

#define ZZ9K_APP_DATASPACE 0x05000000

// from libmemory
void malloc_addblock(void* addr, size_t size);
void zz9k_app_main(uint32_t imageData, uint32_t imageSize, uint8_t* fb, uint32_t screen_w, uint32_t screen_h);

void _putchar(char c) {
  _zz9k_env->putchar(c);
};

void __attribute__ ((section (".binstart"))) main(struct ZZ9K_ENV* env) {
  _zz9k_env = env;

  if (!env) {
    return;
  }
  
  if (env->argc != 4) {
    printf("nanojpeg expects exactly 4 arguments: imageData, imageSize, fb, width.\n");
    return;
  }
  
  // todo get memory block via env function
  // clear any headers of malloc implementation
  memset((void*)ZZ9K_APP_DATASPACE, 0, 1024);
  malloc_addblock((void*)ZZ9K_APP_DATASPACE, 8 * 1024 * 1024);

  int h = 480;
  if (env->argv[3]==1280) h = 720;
  
  zz9k_app_main(env->argv[0], env->argv[1], (uint8_t*)env->argv[2], env->argv[3], h);
}
