/*
 * MNT ZZ9000 Amiga Graphics and ARM Coprocessor SDK
 *            "zz9k" Amiga CLI tool for loading and executing
 *            ARM applications
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/graphics.h>
#include <libraries/expansion.h>
#include <libraries/configvars.h>
#include <clib/alib_protos.h>

#include <proto/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <devices/audio.h>
#include <devices/input.h>
#include <exec/ports.h>
#include <exec/interrupts.h>
#include <exec/memory.h>

#include "zz9000.h"

#define uint8_t unsigned char
#define int16_t signed short
#define uint16_t unsigned short
#define uint32_t unsigned long
#define int32_t signed long

#define ZZ9K_APP_SPACE 0x03000000
#define ZZ9K_APP_DATASPACE 0x05000000
#define ZZ9K_MEM_START 0x00200000

volatile MNTZZ9KRegs* regs;

// lifted from https://stackoverflow.com/a/15171925
unsigned crc8(unsigned crc, unsigned char const *data, size_t len)
{
    if (data == NULL)
        return 0;
    crc = ~crc & 0xff;
    while (len--) {
        crc ^= *data++;
        for (unsigned k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0xb2 : crc >> 1;
    }
    return crc ^ 0xff;
}

struct Screen* zz9k_screen;

void open_screen(int w, int h) {
  uint32_t bmid=BestCModeIDTags(CYBRBIDTG_NominalWidth, w,
                                CYBRBIDTG_NominalHeight, h,
                                CYBRBIDTG_Depth, 24,
                                TAG_DONE);

  //printf("Mode ID: %lx\n", bmid);
  
  zz9k_screen = OpenScreenTags(NULL,
                               SA_Title,"ZZ9000 ARM Application",
                               SA_DisplayID, bmid,
                               SA_Depth, 24,
                               TAG_DONE);

  /*printf("Planes[0]: %p\n", zz9k_screen->BitMap.Planes[0]);
  printf("BytesPerRow: %d\n", zz9k_screen->BitMap.BytesPerRow);
  printf("Rows: %d\n", zz9k_screen->BitMap.Rows);
  printf("Depth: %d\n", zz9k_screen->BitMap.Depth);*/
}

__saveds struct InputEvent *input_handler(__reg("a0") struct InputEvent *ielist, __reg("a1") struct MsgPort *port) {
  struct InputEvent *ie, *prev;

  ie = ielist;
  prev = NULL;

  while(ie) {
    if (ie->ie_Class == IECLASS_RAWKEY) {
      int code = ie->ie_Code;
      //printf("RAWKEY: %d\n", code);
      regs->arm_event_code = code;
    }
    
    ie = ie->ie_NextEvent;
  }
  
  return (ielist);
}

BOOL input_dev_open = FALSE;
struct MsgPort  *input_port = NULL;
struct Interrupt *input_irq_handler = NULL;
struct IOStdReq *input_req = NULL;
UBYTE NameString[]="zz9k input";

int setup_input() {
  int rc = 0;

  if (input_port = CreateMsgPort())
    if (input_irq_handler = AllocVec(sizeof(struct Interrupt),MEMF_CLEAR))
      if (input_req = (struct IOStdReq *) CreateIORequest(input_port,sizeof(struct IOStdReq)))
        if (!OpenDevice("input.device",0,(struct IORequest *)input_req,0))
          input_dev_open = TRUE;

  if (!input_dev_open) {
    printf("Error: Could not open input.device.\n");
  } else {
    ULONG sigs;

    input_irq_handler->is_Code = (APTR)input_handler;
    input_irq_handler->is_Data = (APTR)input_port;
    input_irq_handler->is_Node.ln_Name = "";
    input_irq_handler->is_Node.ln_Pri  = 60;
    
    input_req->io_Data = (APTR)input_irq_handler;
    input_req->io_Command = IND_ADDHANDLER;
    DoIO((struct IORequest *)input_req);

    rc = 1;
  }

  return rc;
}

void cleanup() {
  if (zz9k_screen) {
    CloseScreen(zz9k_screen);
  }
  
  if (input_dev_open) {
    input_req->io_Data = (APTR)input_irq_handler;
    input_req->io_Command = IND_REMHANDLER;
    DoIO((struct IORequest *)input_req);
    
    CloseDevice((struct IORequest *)input_req);
    DeleteIORequest((struct IORequest *)input_req);
    FreeVec(input_irq_handler);
    DeleteMsgPort(input_port);
  }
}

int is_hex(char* str) {
  if (strlen(str)>2 && str[0]=='0' && str[1]=='x') {
    return 1;
  }
  return 0;
}

int main(int argc, char** argv) {
  struct ConfigDev* cd = NULL;
  uint8_t* memory;
  uint16_t fw;
  uint32_t load_offset = 0;
  uint32_t audio_offset = 0;
  char* load_filename = NULL;
  int load_mode = 0;
  int run_mode = 0;
  int stop_mode = 0;
  int screen_mode = 0;
  int console_mode = 0;
  int keyboard_mode = 0;
  int audio_mode = 0;
  int verbose_mode = 0;
  int screen_w = 640;
  int screen_h = 480;
  uint32_t arm_args[4];
  int placeholder_arm_argi[4];
  int placeholder_argi[4];
  int placeholders = 0;
  int arm_argc = 0;

  // zz9k load <arm-data-file> [offset hex or decimal]
  // zz9k run -audio -screen -console 0x1234 45123 0xbeef $screen
  // zz9k stop

  int syntax_ok = 1;
  for (int i=1; i<argc; i++) {
    if (i==1) {
      // command
      if (!strcmp("load", argv[1])) {
        load_mode = 1;
      } else if (!strcmp("run",argv[1])) {
        run_mode = 1;
      } else if (!strcmp("stop",argv[1])) {
        stop_mode = 1;
      } else {
        printf("Unknown command '%s'.\n", argv[1]);
        syntax_ok = 0;
        break;
      }
    } else {
      // option
      if (run_mode) {
        if (!strcmp("-640x480",argv[i])) {
          screen_mode = 1;
          screen_w = 640;
          screen_h = 480;
        } else if (!strcmp("-320x240",argv[i])) {
          screen_mode = 1;
          screen_w = 320;
          screen_h = 240;
        } else if (!strcmp("-audio",argv[i])) {
          audio_mode = 1;
        } else if (!strcmp("-keyboard",argv[i])) {
          keyboard_mode = 1;
        } else if (!strcmp("-console",argv[i])) {
          console_mode = 1;
        } else if (!strcmp("-verbose",argv[i])) {
          verbose_mode = 1;
        } else if (argv[i][0] == '-') {
          printf("Unknown run option '%s'\n", argv[i]);
          syntax_ok = 0;
          break;
        } else {
          if (arm_argc>4) {
            printf("Too many arguments for ARM application (maximum 4).\n");
            syntax_ok = 0;
            break;
          }
          if (is_hex(argv[i])) {
            // hex arg
            arm_args[arm_argc++] = strtoul(&argv[i][2], NULL, 16);
          } else if (argv[i][0] == '!') {
            // special variable
            if (placeholders>4) {
              printf("Too many special variables (maximum 4).\n");
              syntax_ok = 0;
              break;
            }
            placeholder_argi[placeholders]=i;
            placeholder_arm_argi[placeholders]=arm_argc++;
            placeholders++;
          } else {
            // decimal arg
            arm_args[arm_argc++] = strtoul(argv[i], NULL, 10);
          }
        }
      } else if (stop_mode) {
        printf("Surplus arguments for 'stop' command.\n");
        syntax_ok = 0;
        break;
      } else if (load_mode) {
        if (i==2) {
          load_filename = argv[i];
        } else if (i==3) {
          if (is_hex(argv[i])) {
            load_offset = strtoul(&argv[i][2], NULL, 16);
          } else {
            load_offset = strtoul(argv[i], NULL, 10);
          }
        } else {
          printf("Surplus arguments for 'load' command.\n");
          syntax_ok = 0;
          break;
        }
      }
    }
  }

  if (argc<2) {
    syntax_ok = 0;
  }

  if (audio_mode && argc<3) {
    printf("Not enough arguments for 'audio' command.\n");
    syntax_ok = 0;
  }

  if (!syntax_ok) {
    printf("\nUsage: zz9k load <arm-data-file> [offset hex or decimal]\n");
    printf("       zz9k run [mode options] [arg0] ... [arg3]\n");
    printf("       zz9k stop\n\n");
    printf("Mode options (can be combined):\n");
    printf("  -640x480 opens a 640x480x32 screen (bitmap address available in !screen variable)\n");
    printf("  -320x240 opens a 320x240x32 screen (bitmap address available in !screen variable)\n");
    printf("  -console attaches ARM application's input and output event streams to Amiga shell, line based\n");
    printf("  -keyboard sends Amiga keyboard scan codes to ARM application as events\n");
    printf("* -audio (experimental) streams raw audio generated by ARM application. not ready for use yet.\n\n");
    
    printf("arg0 - arg3 are numeric (unsigned 32-bit integer) arguments to the ARM application. For these, you can pass:\n");
    printf("  a decimal number (e.g. 1280)\n");
    printf("  a hexadecimal number prefixed with 0x (e.g. 0xdeadcafe)\n"); 
    printf("  a special variable, one of: !screen !width !height !depth\n\n");
    exit(1);
  }

  // find a ZZ9000
  cd = (struct ConfigDev*)FindConfigDev(cd,0x6d6e,0x4);
  if (!cd) {
    cd = (struct ConfigDev*)FindConfigDev(cd,0x6d6e,0x3);
  }
  if (!cd) {
    printf("Error: MNT ZZ9000 not found.\n");
    exit(1);
  }
  
  memory = (uint8_t*)(cd->cd_BoardAddr)+0x10000;
  regs = (volatile MNTZZ9KRegs*)(cd->cd_BoardAddr);
  
  if (stop_mode) {
    // reset application ARM core
    regs->arm_run_hi = 0;
    regs->arm_run_lo = 0;
    printf("ARM core reset.\n");
    exit(0);
  }

  if (load_mode) {
    // load a file into ZZ9000 memory

    uint8_t* dest = memory + ZZ9K_APP_SPACE - ZZ9K_MEM_START + load_offset;

    if (verbose_mode) {
      printf("Loading '%s' to ARM address 0x%lx (Amiga address 0x%lx)\n", argv[2], ZZ9K_APP_SPACE + load_offset, dest);
    }

    FILE* f = fopen(argv[2],"rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      long fsize = ftell(f);
      fseek(f, 0, SEEK_SET);
      size_t bytes_read = fread(dest, 1, fsize, f);
      fclose(f);
      printf("%lx\n",bytes_read);

      //printf("CRC: %lx\n",crc8(0,dest,bytes_read));
    } else {
      printf("0\n");
    }
    
    exit(0);
  }
  
  if (run_mode) {
    // TODO encapsulate all this audio junk
    char* audio_buf = NULL;
    const int audio_bufsz = 16000;
    uint32_t t = 0;
    uint32_t chipoffset = 0;
    const uint32_t chunksz = 48000*2;
    char audio_devopened;
    struct MsgPort *audio_port;
    struct IOAudio *aio[1];
    volatile int16_t* src;

    if (screen_mode) {
      open_screen(screen_w, screen_h);

      if (!zz9k_screen) {
        printf("Error creating screen.\n");
        exit(2);
      }

      // capture keypresses
      if (keyboard_mode) {
        setup_input();
      }

      // get screen bitmap address
      uint32_t fb = ((uint32_t)zz9k_screen->BitMap.Planes[0])-(uint32_t)memory+(uint32_t)ZZ9K_MEM_START;
      fb+=zz9k_screen->BarHeight*zz9k_screen->BitMap.BytesPerRow; // skip title bar

      // fill in screen special variables
      for (int i=0; i<placeholders; i++) {
        int argi = placeholder_argi[i];
        int arm_argi = placeholder_arm_argi[i];

        if (!strcmp(argv[argi],"!screen")) {
          arm_args[arm_argi] = fb;
        } else if (!strcmp(argv[argi],"!width")) {
          arm_args[arm_argi] = zz9k_screen->Width;
        } else if (!strcmp(argv[argi],"!height")) {
          arm_args[arm_argi] = zz9k_screen->BitMap.Rows;
        } else if (!strcmp(argv[argi],"!depth")) {
          arm_args[arm_argi] = zz9k_screen->BitMap.Depth;
        }
      }
    }

    if (audio_mode) {
      // TODO: this is currently just a sketch. this should loop
      // over a buffer space and request more buffers via events
            
      if (!(audio_port=CreatePort(0,0))) {
        printf("Error: couldn't create Audio message Port.\n");
        exit(5);
      }
      for (int k=0; k<1; k++) {
        if (!(aio[k]=(struct IOAudio *)CreateExtIO(audio_port,sizeof(struct IOAudio)))) {
          printf("Error: couldn't create IOAudio %d\n",k);
          exit(5);
        }
      }

      char channels[] = {1,2,4,8};
      
      // Set up request to Audio port
      aio[0]->ioa_Request.io_Command = ADCMD_ALLOCATE;
      aio[0]->ioa_Request.io_Flags = ADIOF_NOWAIT;
      aio[0]->ioa_AllocKey = 0;
      aio[0]->ioa_Data = channels;
      aio[0]->ioa_Length = sizeof(channels);

      if (!(OpenDevice("audio.device", 0L, (struct IORequest *) aio[0], 0L)))
        audio_devopened = TRUE;
      else {
        printf("Error: couldn't open audio.device\n");
        exit(5);
      }

      // Paula playback buffer in chipmem
      audio_buf = AllocMem(audio_bufsz,MEMF_CHIP);
      src = (volatile int16_t*)(((char*)arm_args[2])-(uint32_t)ZZ9K_MEM_START+(uint32_t)memory);
      
      if (!audio_buf) {
        printf("Error: allocating chipmem failed.\n");
      } else {
        memset(audio_buf,0,audio_bufsz);
      }
    }

    // pass ARM app arguments
    for (int i=0; i<arm_argc; i++) {
      if (verbose_mode) {
        printf("ARM arg%d: %ld (%lx)\n", i, arm_args[i], arm_args[i]);
      }
      
      regs->arm_arg[i*2]   = arm_args[i]>>16;
      regs->arm_arg[i*2+1] = arm_args[i]&0xffff;
    }
    regs->arm_argc   = arm_argc;

    // pass ARM run address
    regs->arm_run_hi = (ZZ9K_APP_SPACE + load_offset)>>16;
    regs->arm_run_lo = (ZZ9K_APP_SPACE + load_offset)&0xffff;

    uint16_t last_ser = 0;
    uint32_t timeout = 0;
      
    // main loop
    while (1) {
      if (console_mode) {
        // send kbd, mouse events and receive events
        // exit on quit event
        int chr = getc(stdin);
        if (chr) {
          regs->arm_event_code = chr;
        }

        if (chr=='\n') {
          char done = 0;
          while (!done) {
            uint16_t armchr = regs->arm_event_code;
            uint16_t armser = regs->arm_event_serial;
            
            if (armser!=last_ser) {
              last_ser = armser;
              putc(armchr, stdout);
              timeout = 0;
            }
            timeout++;
            if (timeout>=10000) {
              done = 1;
              timeout = 0;
            }
          }
        }
      }

      if (audio_mode) {
        aio[0]->ioa_Request.io_Command = CMD_WRITE;
        aio[0]->ioa_Request.io_Flags = ADIOF_PERVOL;
        aio[0]->ioa_Data = audio_buf+chipoffset; // sample
        aio[0]->ioa_Length = audio_bufsz/2;
        aio[0]->ioa_Period = 3546895L/8000;
        aio[0]->ioa_Volume = 0xff;
        aio[0]->ioa_Cycles = 1;
        BeginIO((struct IORequest *)aio[0]);

        if (chipoffset == 0) {
          chipoffset = audio_bufsz/2;
        } else {
          chipoffset = 0;
        }
          
        // downsample 48000 stereo to 8000 mono
        // FIXME lowpass filter
        // FIXME let ARM do this
        uint32_t bi = 0;
        for (uint32_t i=0; i<chunksz; i+=2*6) {
          int32_t srcss=src[i+audio_offset];
          uint32_t srcs=srcss+=65536/2;
          audio_buf[chipoffset+bi] = (((srcs&0xff)<<8|(srcs&0xff00)>>8))>>8;
          bi++;
          if (bi>=audio_bufsz/2) break;
        }
        audio_offset+=chunksz;
        
        WaitIO((struct IORequest *)aio[0]);
        
        // FIXME arbitrary end
        if (audio_offset>500*chunksz) break;
      }

      // end on mouse click
      volatile uint8_t* mreg = (volatile uint8_t*)0xbfe001;
      if (!(*mreg&(1<<6))) break;
    }

    // done running
    // reset application ARM core
    regs->arm_run_hi = 0;
    regs->arm_run_lo = 0;
    if (verbose_mode) {
      printf("ARM core reset.\n");
    }
    
    cleanup();
    if (audio_mode) {
      // FIXME consolidate with cleanup()
      FreeMem(audio_buf,audio_bufsz);
      if (audio_devopened) {
        CloseDevice((struct IORequest *)aio[0]);
      }
      for (int k=0; k<1; k++) {
        if (aio[k]) {
          DeleteExtIO((struct IORequest *)aio[k]);
          aio[k] = NULL;
        }
      }
      if (audio_port) {
        DeletePort(audio_port);
        audio_port = NULL;
      }
    }
  }
}
