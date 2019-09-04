#include <stdint.h>
#include "printf/printf.h"
#include "zz9k_env.h"

int _Z10trace_mainPmm(uint32_t* fb, uint32_t stride);

struct ZZ9K_ENV* _zz9k_env;

#define ZZ9K_APP_DATASPACE 0x05000000

void _putchar(char c) {
  _zz9k_env->putchar(c);
};

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

int __attribute__ ((section (".binstart"))) main(struct ZZ9K_ENV* env) {
  _zz9k_env = env;

  if (!env) {
    return 1;
  }
  
  if (env->argc<2) {
    return 1;
  }

  printf("zz9ktrace! framebuffer: %lx stride: %d\n",env->argv[0],env->argv[1]);

  // arg0: framebuffer pointer
  // arg1: screen width
  _Z10trace_mainPmm((uint32_t*)env->argv[0],env->argv[1]);
  
  return 0;
}
