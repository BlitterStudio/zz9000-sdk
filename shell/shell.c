/*
 * MNT ZZ9000 Amiga Graphics and ARM Coprocessor SDK
 *            Code example: "shell"
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

struct ZZ9K_ENV* _zz9k_env;

void _putchar(char c) {
  _zz9k_env->putchar(c);
};

void __attribute__ ((section (".binstart"))) main(struct ZZ9K_ENV* env) {
  _zz9k_env = env;

  if (!env) {
    return;
  }

  env->set_output_putchar_to_events(1);
  env->set_output_events_blocking(1);

  uint16_t ser = 0;
  char buffer[64];
  uint16_t bufp = 0;
  while (1) {
    uint16_t newser = env->get_event_serial();
    if (newser != ser) {
      ser = newser;
      uint16_t event = env->get_event_code();
      if (event<256) {
        // character
        if (event!='\n') {
          buffer[bufp++] = event;
          buffer[bufp] = 0;
        }
        if (bufp>62) bufp=0;
        
        if (event=='\n') {
          printf("we got: [%s]\r\n",buffer);
          bufp=0;
        }
      } else {
        printf("event: %x\n",event);
      }
    }
  }
}
