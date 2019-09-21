# MNT ZZ9000 ARM SDK

MNT ZZ9000 is a graphics and ARM coprocessor card for Amiga computers equipped with Zorro slots. It is based on a Xilinx ZYNQ Z-7020 chip that combines 7-series FPGA fabric with dual ARM Cortex-A9 CPUs clocked at 666MHz. The current version has 1GB of DDR3 RAM and no eMMC soldered.

This repository contains some example programs and documentation that will help you to get started hacking on ARM software for the ZZ9000.

The mechanism for launching and interacting with ARM programs from AmigaOS is still rudimentary at this point. We're grateful for any constructive feedback and pull requests that will help shape the system.

# Requirements

To build the example applications, you need a version of GCC called arm-none-eabi-gcc, which is available in major Linux distributions. We're exclusively developing on Linux (Debian, specifically) and don't support any other platforms at the moment. You're welcome to contribute build instructions for other platforms. It would be nice to have an ARM compiler/assembler for AmigaOS as well, so that you can develop on the target machine.

# Building

Every example application has a `build-appname.sh` script that calls `arm-none-eabi-gcc` to build and statically link the application with a special linker file, `link.ld`. Every application is per default linked to run at address `0x03000000` and can access arbitrary memory. There is no memory protection or memory management, but you can link in the included `libmemory` for malloc/free as demonstrated by the nanojpeg example (you just give it a fixed memory block on startup). 

ZZ9000OS offers much less infrastructure to applications than traditional operating systems. Currently, only the following functions and arguments are provided by a structure called ZZ9K_ENV passed to your entry function:

```
struct ZZ9K_ENV {
  uint32_t api_version;
  uint32_t argv[8];
  uint32_t argc;

  int      (*putchar)(char);
  void     (*set_output_putchar_to_events)(char);
  void     (*set_output_events_blocking)(char);
  void     (*put_event_code)(uint16_t);
  uint16_t (*get_event_serial)();
  uint16_t (*get_event_code)();
  char     (*output_event_acked)();
};
```

# Loading

In the `zz9k-loader` directory, you can find sources for the `zz9k` CLI tool that runs on AmigaOS (m68k). With `zz9k`, you can load an ARM application into the DDR3 memory of ZZ9000 and run it. The loader supports setting up multiple user interface modalities as a convenience:

- `run` just jumps to your code with no user interface.
- `-640x480` and `-320x240` set up a 640x480@32 or 320x240@32 Intuition screen. If you pass a `!screen` parameter to your application, it will be substituted for the screen's bitmap address for direct access. Pass `!width` as a parameter to get the screen's width in pixels.
- `-keyboard` passes raw Amiga keyboard scan codes to the ARM application's event stream.
- `-console` attaches stdin and stdout of the Shell to your application, demonstrated by the `shell` example.
- `-audio` experimental mode that plays back an audio buffer your application creates until a mouse button is pressed, demonstrated by `minimp3`.

# Launching Example Apps

## Conway

```
zz9k load conway.bin
zz9k run -320x240 !screen !width
```

## Vector

```
zz9k load vector.bin
zz9k run -320x240 !screen !width
```

## Raytrace

```
zz9k load raytrace.bin
zz9k run -320x240 !screen !width
```

# Third Party Code

The SDK contains a collection of third-party libraries/code for ARM bare metal applications:

- Runtime ABI for Cortex-M0 (lib/div), by Jörg Mische <bobbl@gmx.de>
- Tiny printf, sprintf, vsnprintf (lib/printf), by Marco Paland <info@paland.com>
- libmemory memory allocator (lib/memory/libmemory_freelist.a), by Embedded Artistry, sources at: https://github.com/embeddedartistry/libmemory
- memcpy, memset, memmove (lib/memory), Public Domain

Portions of example code is lifted from the following sources:

- "Conway's game of life", Rosetta Code, https://rosettacode.org/wiki/Conway%27s_Game_of_Life#C

# License / Copyright

If not stated otherwise in specific source code files, everything here is:

Copyright (C) 2016-2019, Lukas F. Hartmann <lukas@mntre.com>
MNT Research GmbH, Berlin
https://mntre.com

SPDX-License-Identifier: GPL-3.0-or-later
https://spdx.org/licenses/GPL-3.0-or-later.html
