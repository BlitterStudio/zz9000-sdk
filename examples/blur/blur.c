/*
 * MNT ZZ9000 Amiga Graphics and ARM Coprocessor SDK
 *            Code example: "blur"
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
#include <unistd.h>
#include "printf/printf.h"
#include "zz9k_env.h"

static uint32_t* fb=0;
static uint32_t fb_pitch=0;

struct ZZ9K_ENV* _zz9k_env;

#define ZZ9K_APP_DATASPACE 0x05000000

void __aeabi_idiv0(int r) {
  printf("__aeabi_idiv0()!\n");
  while (1) {
  }
}
void __aeabi_ldiv0(int r) {
  printf("__aeabi_idiv0()!\n");
  while (1) {
  }
}

int errno_var = 0;
int* __errno() {
  return &errno_var;
}
void _putchar(char c) {
  _zz9k_env->putchar(c);
};

void set_fb(uint32_t* fb_, uint32_t pitch) {
	fb=fb_;
	fb_pitch=pitch;
}

void blur() {
	for (int y=0; y<480; y++) {
    for (int x=0; x<640; x++) {
      uint32_t s1 = fb[y*fb_pitch+x-1];
      uint32_t s2 = fb[y*fb_pitch+x+1];
      uint32_t s3 = fb[(y-1)*fb_pitch+x];
      uint32_t s4 = fb[(y+1)*fb_pitch+x];

      uint32_t s1r = (s1>>16)&0xff;
      uint32_t s1g = (s1>>8)&0xff;
      uint32_t s1b = s1&0xff;
      uint32_t s2r = (s2>>16)&0xff;
      uint32_t s2g = (s2>>8)&0xff;
      uint32_t s2b = s2&0xff;
      uint32_t s3r = (s3>>16)&0xff;
      uint32_t s3g = (s3>>8)&0xff;
      uint32_t s3b = s3&0xff;
      uint32_t s4r = (s4>>16)&0xff;
      uint32_t s4g = (s4>>8)&0xff;
      uint32_t s4b = s4&0xff;

      uint32_t r = (s1r+s2r+s3r+s4r)/4;
      if (r>0xff) r = 0xff;
      uint32_t g = (s1g+s2g+s3g+s4g)/4;
      if (g>0xff) g = 0xff;
      uint32_t b = (s1b+s2b+s3b+s4b)/4;
      if (b>0xff) b = 0xff;
      
      fb[y*fb_pitch+x] = (r<<16)|(g<<8)|b;
    }
	}
}

int __attribute__ ((section (".binstart"))) main(struct ZZ9K_ENV* env) {
  _zz9k_env = env;

  if (!env) {
    return 1;
  }
  
  if (env->argc<2) {
    return 1;
  }

  // arg0: framebuffer pointer
  // arg1: screen width
  set_fb((uint32_t*)env->argv[0],env->argv[1]);

  while (1) {
    blur();
  }
  
  return 0;
}
