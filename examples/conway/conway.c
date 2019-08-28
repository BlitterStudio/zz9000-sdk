#include <stdlib.h>
#include <unistd.h>
#include "printf/printf.h"
#include "zz9k_env.h"

// matrix width and height
#define CONW_W 320
#define CONW_H (240-16)

unsigned char univ[CONW_H][CONW_W];
unsigned char new[CONW_H][CONW_W];

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

unsigned int lfsr113 (void)
{
   static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
   unsigned int b;
   b  = ((z1 << 6) ^ z1) >> 13;
   z1 = ((z1 & 4294967294U) << 18) ^ b;
   b  = ((z2 << 2) ^ z2) >> 27; 
   z2 = ((z2 & 4294967288U) << 2) ^ b;
   b  = ((z3 << 13) ^ z3) >> 21;
   z3 = ((z3 & 4294967280U) << 7) ^ b;
   b  = ((z4 << 3) ^ z4) >> 12;
   z4 = ((z4 & 4294967168U) << 13) ^ b;
   return (z1 ^ z2 ^ z3 ^ z4);
}

// Lifted from https://rosettacode.org/wiki/Conway%27s_Game_of_Life#C

void show(uint32_t* fb, int fbw, int w, int h)
{ 
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
      fb[y*fbw+x] = (univ[y][x] ? 0xffffff : 0x000000);
    }
  }
}

void evolve(int w, int h)
{ 
	for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int n = 0;
      for (int y1 = y - 1; y1 <= y + 1; y1++) {
        for (int x1 = x - 1; x1 <= x + 1; x1++) {
          if (univ[(y1 + h) % h][(x1 + w) % w])
            n++;
        }
      }
 
      if (univ[y][x]) n--;
      new[y][x] = (n == 3 || (n == 2 && univ[y][x]));
    }
  }
	for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      univ[y][x] = new[y][x];
    }
  }
}

void init(int w, int h) {
	for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      univ[y][x] = lfsr113() < RAND_MAX / 10 ? 1 : 0;
    }
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

  if (env->argc<2) {
    return;
  }

  init(CONW_W, CONW_H);
  
  while (1) {
    evolve(CONW_W, CONW_H);
    // arg0: framebuffer pointer (32bpp)
    // arg1: framebuffer width in pixels
    show((uint32_t*)env->argv[0], (uint32_t)env->argv[1], CONW_W, CONW_H);
  }
}
