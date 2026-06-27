# zz9k.library

`zz9k.library` is the AmigaOS 3.x entry point for SDK v2. New Amiga-side
programs should use it instead of talking to the mailbox registers directly.

The current library identity is:

```c
#define ZZ9K_LIBRARY_NAME "zz9k.library"
#define ZZ9K_LIBRARY_VERSION 2
#define ZZ9K_LIBRARY_REVISION 24
```

Open the library with at least version 2:

```c
#include "proto/zz9k.h"
#include <proto/exec.h>

struct Library *ZZ9KBase;

ZZ9KBase = OpenLibrary((CONST_STRPTR)ZZ9K_LIBRARY_NAME,
                       ZZ9K_LIBRARY_VERSION);
```

## Build

Use the same m68k AmigaOS compiler flags as the SDK tools:

```sh
m68k-amigaos-gcc -noixemul -Os -s \
  -Iinclude -Ihost/include -Iamiga/include \
  examples/amiga-library/zz9k-library-demo.c \
  -o build/zz9k-library-demo
```

The repository build scripts also build the demo:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-m68k-amigaos.ps1
```

## Developer Headers

The SDK ships the conventional Amiga library metadata alongside the inline
caller header:

- `amiga/fd/zz9k_lib.fd` lists the public LVOs for stub generation.
- `amiga/include/clib/zz9k_protos.h` declares the public C prototypes.
- `amiga/include/proto/zz9k.h` provides inline m68k calls for programs that
  include `proto/zz9k.h` directly.

New C programs can include `proto/zz9k.h`. Ported code or build systems that
expect Amiga FD/clib metadata can use the FD and clib files instead.

For application-side helper code, `zz9k/sdk.h` is the umbrella include. It
pulls in the SDK v2 ABI, host/request/reply types, and helper headers. AmigaOS
programs still include `proto/zz9k.h` when they need to call the resident
`zz9k.library` LVOs.

Small text helpers live in `zz9k/text.h`. They are header-only so AmigaOS
programs that only call `zz9k.library` do not need to link `zz9k_host.o` just
to print readable status names:

```c
#include "zz9k/text.h"

printf("decode failed: %s (%d)\n", zz9k_status_text(status), status);
printf("surface format: %s\n", zz9k_surface_format_text(surface.format));
printf("service: %s\n", zz9k_service_text(ZZ9K_SERVICE_IMAGE));
```

SDK v2 is paired with the new SDK-service firmware. The pre-service firmware is
not a supported runtime for SDK v2 tools or `zz9k.library`, because the mailbox
service registry did not exist before this release pair.

Use `zz9k_known_service_count()` / `zz9k_known_service_id()` to enumerate the
SDK-known built-in service IDs when checking the firmware service registry.

Capability helpers live in `zz9k/caps.h` and are also header-only:

```c
#include "zz9k/caps.h"

uint32_t required = ZZ9K_CAP_SHARED_ALLOC | ZZ9K_CAP_IMAGE_DECODE;
if (!zz9k_has_capabilities(caps.capability_bits, required)) {
  uint32_t missing = zz9k_missing_capabilities(caps.capability_bits,
                                               required);
  printf("missing SDK caps: 0x%08lx\n", (unsigned long)missing);
}
```

Use `zz9k_has_service_flags()` and `zz9k_missing_service_flags()` the same way
for service descriptor flags returned by `ZZ9KQueryService()`.
`zz9k_capability_name()` and `zz9k_service_flag_name()` return stable
CLI-friendly names for known bits, or `0` for unknown bits:

```c
const char *name = zz9k_capability_name(ZZ9K_CAP_IMAGE_DECODE);
if (name) {
  printf("capability: %s\n", name);
}
```
Use `zz9k_known_capability_count()` / `zz9k_known_capability_bit()` and
`zz9k_known_service_flag_count()` / `zz9k_known_service_flag()` when a tool
needs to enumerate all SDK-known bits without maintaining its own table.
Use `zz9k_service_capability_mask()` and
`zz9k_service_advertised_by_capabilities()` when a diagnostic wants to compare
global firmware capabilities with explicit service descriptors.
The reserved built-in service families now have explicit capability ownership:
core/mailbox, memory, surface, gfx, image, audio, codec, storage, crypto,
diagnostics, and module loading. Firmware should only advertise a service when
the matching capability bit is set and `ZZ9KQueryService()` can return a stable
descriptor for it.

For release-pair hardware validation, run:

```text
zz9k-services --check-release
```

The command requires service discovery, queries the expected SDK v2 services,
checks required capabilities, service flags, service version major, opcode base,
and opcode count, and exits nonzero if the SDK and firmware do not match the
current release contract. A matching firmware ends with:

```text
release check ok
```

For the broader package-level runtime smoke pass, use
[`zz9k-release-smoke.md`](zz9k-release-smoke.md). It groups the shipped tools
by service family and records the expected pass signal and failure routing for
each check.

Compression helpers live in `zz9k/compression.h`. They build the low-level
shared-buffer descriptors used by archive and datatype tools without turning
the SDK into an archive parser.

Surface layout helpers live in `zz9k/surface.h`. They are header-only and are
intended for callers that need to size SDK surfaces before calling
`ZZ9KAllocSurfaceEx()`:

```c
#include "zz9k/surface.h"

uint32_t pitch;
uint32_t length;
uint32_t format = zz9k_surface_native_rtg_format();

if (zz9k_surface_layout(width, height, format, &pitch, &length)) {
  ZZ9KAllocSurfaceEx(width, height, format, 0, pitch, &surface);
}
```

`zz9k_surface_rect_fits()` and `zz9k_surface_rect_fits_bpp()` validate that a
`ZZ9KRect` stays inside a surface's width/height/pitch/length bounds, including
integer overflow checks.
`zz9k_known_surface_flag_count()` / `zz9k_known_surface_flag()` and
`zz9k_surface_flag_name()` provide stable names for surface metadata flags such
as `cpu-visible`, `framebuffer`, and `arm-local`.

The same header also includes descriptor builders for common surface operations:
`zz9k_surface_build_fill_desc()`,
`zz9k_surface_build_framebuffer_fill_desc()`,
`zz9k_surface_build_copy_desc()`, and
`zz9k_surface_build_framebuffer_backup_copy_descs()`.

Color helpers in `zz9k/surface.h` pack fill colors for the firmware semantics:
32-bit formats use logical `0xAARRGGBB`, `RGB565`/`RGB555` use packed 16-bit
values, and indexed surfaces use the palette index.

Image geometry helpers live in `zz9k/image_geometry.h`. They are header-only
and mirror the rectangle math used by the SDK image tools, so datatypes and
viewers can build framebuffer or surface scaler descriptors without depending
on tool-private code:

```c
#include "zz9k/image_geometry.h"

ZZ9KRect area = { left, top, width, height };
ZZ9KRect draw;
ZZ9KScaleImageClippedDesc desc;

if (zz9k_image_choose_draw_rect_in_area(&area, image_w, image_h, &draw) &&
    zz9k_image_build_clipped_scale_desc(&desc, surface.handle,
                                        image_w, image_h,
                                        &draw, &area,
                                        ZZ9K_SCALE_BILINEAR)) {
  ZZ9KScaleImageClipped(&desc);
}
```

Image decode/session helpers live in `zz9k/image.h`. They build one-shot
decode descriptors, image-session begin descriptors for surface/framebuffer/tile
output, and bounded input feed descriptors.

Shared-buffer helpers live in `zz9k/shared.h`. Use them when copying CPU-side
bytes into or out of `ZZ9KSharedBuffer.data`, clearing ranges, or compacting
streaming input within a shared buffer; they bounds-check the mapped buffer and
preserve volatile byte access:

```c
#include "zz9k/shared.h"

if (!zz9k_shared_copy_to(&input, 0, bytes, byte_count)) {
  /* handle bad shared-buffer range */
}
```

Crypto helpers live in `zz9k/crypto.h`. They provide digest-size lookup and
descriptor builders for plain hashes, HMAC, Poly1305, ChaCha20 streams, and
ChaCha20-Poly1305 AEAD:

```c
#include "zz9k/crypto.h"

ZZ9KCryptoStreamDesc stream;

if (zz9k_crypto_build_chacha20_desc(&stream, input.handle, 0,
                                    input.length, output.handle, 0,
                                    key.handle, 0, nonce.handle, 0, 1)) {
  ZZ9KCryptoStream(&stream, &result);
}
```

## Synchronous Calls

Use high-level wrappers such as `ZZ9KQueryCaps()`, `ZZ9KPing()`,
`ZZ9KAllocShared()`, `ZZ9KAllocSurfaceEx()`, `ZZ9KScaleImage()`,
`ZZ9KScaleImageClipped()`, `ZZ9KFillSurface()`, `ZZ9KCopySurface()`,
`ZZ9KDecodeImage()`, `ZZ9KDecodeJpeg()`, `ZZ9KDecodePng()`,
`ZZ9KDecodeGif()`,
`ZZ9KImageSessionBegin()`, `ZZ9KImageSessionFeed()`,
`ZZ9KImageSessionClose()`,
`ZZ9KCryptoHash()`, `ZZ9KCryptoHashBatch()`,
`ZZ9KCryptoStream()`, `ZZ9KCryptoStreamBatch()`, `ZZ9KCryptoAead()`, and
`ZZ9KCryptoAeadBatch()` for normal calls. The resident library serializes
access to the shared mailbox context.

```c
ZZ9KCaps caps;
if (ZZ9KQueryCaps(&caps) != ZZ9K_STATUS_OK) {
  /* handle error */
}
```

For custom service calls, fill a `ZZ9KRequest` and call `ZZ9KCall()`.

Prefer the typed request builders in `zz9k/request.h` over hand-packing
mailbox payload bytes:

```c
#include "zz9k/request.h"

ZZ9KRequest request;
zz9k_request_ping(&request, (const uint8_t *)"ping", 4);
ZZ9KCall(&request, &reply, ZZ9K_DEFAULT_TIMEOUT_TICKS);
```

The builders clear the request, set the opcode/inline-payload flags, validate
basic arguments, and encode multi-byte payload fields in the mailbox byte
order. They are header-only so Amiga-side tools can use them without linking an
extra helper object.

Async callers can pair request builders with typed reply decoders from
`zz9k/reply.h`:

```c
#include "zz9k/reply.h"

ZZ9KCaps caps;
if (zz9k_reply_caps(&async.reply, &caps) == ZZ9K_STATUS_OK) {
  /* use caps */
}
```

The decoders check completion status, expected opcode, payload length, and
mailbox byte order before filling public SDK structs.

## Surface Jobs

SDK v2 exposes zero-copy surface primitives for ARM-side operations on SDK
surfaces, including the mapped RTG framebuffer surface:

```c
#include "zz9k/surface.h"

ZZ9KRect rect = {32, 32, 160, 80};
ZZ9KSurfaceFillDesc fill;

if (zz9k_surface_build_framebuffer_fill_desc(
      &fill, &rect, zz9k_surface_color_rgb(0x20, 0x40, 0xff), 0U)) {
  ZZ9KFillSurface(&fill);
}
```

`ZZ9KFillSurface()` stores the logical `0xAARRGGBB` color in the destination
surface byte order. `ZZ9KCopySurface()` copies a rectangle between same-format
surfaces and supports framebuffer handles as either source or destination.
Callers should query `ZZ9K_CAP_SURFACE_OPS` or the surface service descriptor
before using these operations.

Surface format names describe memory byte order at increasing addresses. The
native 32-bit RTG framebuffer format is `ZZ9K_SURFACE_FORMAT_BGRA8888`, matching
the BGRA mode advertised by the Picasso96 driver. ARGB and RGBA remain valid
offscreen SDK formats, but direct-to-RTG decoders and blits should prefer BGRA
to avoid swizzling. SDK callers can use `zz9k_surface_native_rtg_format()` and
`zz9k_surface_is_native_rtg_format()` instead of hard-coding that choice.

When the system is running in an RTG mode, the active framebuffer is already
card-side DDR3 memory managed by the same firmware used by Picasso96. Graphics
operations should therefore stay ARM-side whenever possible: copy, fill, scale,
decode, and restore through SDK surfaces or the framebuffer handle rather than
round-tripping pixels through m68k-visible host memory.

Large temporary graphics surfaces can be requested with
`ZZ9K_SURFACE_FLAG_ARM_LOCAL`. These surfaces are valid SDK surface handles for
ARM-side copy/fill/scale/decode work, but they are not mapped into the Amiga CPU
address space and `ZZ9KSurface.data` may be `NULL`. Use this for scratch
framebuffer snapshots and other card-local work that does not need direct m68k
pixel access. Firmware must reserve the ARM-local heap separately from RTG
scratch areas, because Picasso96 template and sprite uploads also use card-side
DDR.

The `zz9k-surfaceops` tool is the real-card smoke test. On AmigaOS it opens an
Intuition window and targets the window's inner rectangle for diagnostics. It
saves a guarded framebuffer rectangle into an ARM-local backup surface, fills
an inner framebuffer rectangle, fills an ARM-local source surface, copies that
source to the framebuffer, and restores the guarded rectangle with another
ARM-side surface copy. The `zz9k-surfaceops` diagnostic still uses absolute
framebuffer coordinates after choosing that rectangle, so it is not
layer-aware: it can draw over another window that is moved in front of the test
window. Do not use that drawing model for release viewer or DataType rendering.

The `zz9k-bench` tool measures a 1280x720 BGRA8888 ARM-local surface copy as
`ARM local copy`. This path is intended to track RTG-sized card-local bandwidth:
the benchmark fills and copies SDK surfaces entirely on the ARM/DDR3 side and
does not require those pixels to be mapped into m68k address space.

## Image Scaling

`ZZ9KScaleImage()` scales a same-format source surface into another SDK surface
or the mapped framebuffer. `ZZ9K_SCALE_NEAREST` is the compatibility path.
Firmware builds with the bilinear scaler also accept `ZZ9K_SCALE_BILINEAR` for
32-bit formats such as the native BGRA RTG framebuffer. Bicubic and Lanczos
constants are reserved but still return `UNSUPPORTED`.
Programs can check `ZZ9K_SERVICE_FLAG_IMAGE_SCALE_BILINEAR` on the image
service descriptor before submitting bilinear jobs, or use
`zz9k_image_scale_filter_supported_by_service()`.

`ZZ9KScaleImageClipped()` uses the same full source and destination rectangle
model, but also takes a destination clip rectangle. The ARM scaler intersects
the destination with the clip before writing pixels, while preserving the
mapping that would have been used for the full destination rectangle. This is
the foundation for windowed redraw and damage-region handling where raw ARM
framebuffer writes must stay inside the currently valid visible region.
Programs should check clipped-scale support before using it. The helper
`zz9k_image_service_supports_clipped_scale()` checks both the clipped-scale
opcode count and the filter-specific service flags.

The public `zz9k/image_geometry.h` helper provides `ZZ9KRect`,
`zz9k_image_fit_size_to_area()`, `zz9k_image_choose_draw_rect_in_area()`,
`zz9k_image_build_framebuffer_scale_desc()`,
`zz9k_image_build_surface_scale_desc()`,
`zz9k_image_build_clipped_scale_desc()`,
`zz9k_image_build_surface_clipped_scale_desc()`, and
`zz9k_image_count_scale_slices()`. It also exposes service-check helpers for
filter and clipped-scale support. Use these for datatype/window redraw paths:
they preserve the full image-to-window mapping while optionally restricting
writes to the current valid clip rectangle. The default SDK slice size is
`ZZ9K_IMAGE_SCALE_DEFAULT_SLICE_ROWS`, currently 96 destination rows.

`zz9k-scaletest` can exercise both paths on real hardware:

```sh
zz9k-scaletest --nearest
zz9k-scaletest --bilinear
zz9k-scaletest --clip --bilinear
```

The clipped `zz9k-scaletest` path builds its framebuffer descriptor through the
same public image geometry helper, so it is a small real-hardware smoke test for
the API shape that datatype and viewer code should use.

## Image Decode Jobs

SDK v2 defines a shared-buffer to surface image decode job shape. Current
firmware support uses libjpeg-turbo for JPEG decode into 32-bit SDK surfaces.
PNG support is exposed through streaming image sessions for direct 32-bit
surface/framebuffer output. The one-shot PNG opcode and GIF opcode are reserved
and currently return `UNSUPPORTED`.

```c
ZZ9KImageDecodeDesc desc;
ZZ9KRequest request;
ZZ9KRect target = {0, 0, target_surface.width, target_surface.height};

if (zz9k_image_build_decode_desc(&desc, compressed_buffer.handle, 0,
                                 compressed_size, target_surface.handle,
                                 &target, target_surface.format, 0)) {
  zz9k_request_decode_jpeg(&request, &desc);
}
```

Synchronous callers can use `ZZ9KDecodeJpeg()`, `ZZ9KDecodePng()`,
`ZZ9KDecodeGif()`, or the generic `ZZ9KDecodeImage()` /
`zz9k_decode_image()` entry point. Async callers can build the request with
`zz9k_request_decode_jpeg()` and decode completion payloads with
`zz9k_reply_image_decode_result()`.

Typed decode wrappers were appended in library revision 15. Programs that call
`ZZ9KDecodeJpeg()`, `ZZ9KDecodePng()`, or `ZZ9KDecodeGif()` through
`proto/zz9k.h` should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_TYPED_IMAGE_DECODE) {
  /* use an older path or fail cleanly */
}
```

`examples/amiga-typed-decode/zz9k-typed-decode-demo.c` is the compact
developer example for this API. It loads one JPEG into a shared buffer, selects
`zz9k_surface_native_rtg_format()`, sizes the output with
`zz9k_surface_layout()`, allocates an ARM-local surface, and calls
`ZZ9KDecodeJpeg(&desc, &result)`.

Large JPEG inputs should use image sessions instead of a one-shot full input
buffer plus full output surface. The Amiga side feeds compressed data through a
bounded shared staging buffer and receives partial completions until the image
is complete:

```c
ZZ9KImageSessionBeginDesc begin;
ZZ9KImageSessionFeedDesc feed;
ZZ9KImageSessionResult result;
ZZ9KRect target = {0, 0, framebuffer.width, framebuffer.height};
uint32_t output_format;

output_format = zz9k_surface_native_rtg_format();
if (zz9k_surface_is_native_rtg_format(framebuffer.format) &&
    zz9k_image_build_framebuffer_session_begin_desc(
      &begin, ZZ9K_IMAGE_CODEC_JPEG, &target, output_format, 0) &&
    ZZ9KImageSessionBegin(&begin, &result) == ZZ9K_STATUS_OK) {
  /* Fill staging buffer, call ZZ9KImageSessionFeed() until COMPLETE. */
  ZZ9KImageSessionClose(result.session, 0);
}
```

Programs that require the public session LVOs should check
`ZZ9K_LIBRARY_MIN_REVISION_IMAGE_SESSIONS`.

Direct-to-RTG decode should use `zz9k_surface_native_rtg_format()` when
targeting the mapped framebuffer, then confirm the mapped framebuffer with
`zz9k_surface_is_native_rtg_format()`. That keeps callers aligned with the
native 32-bit RTG byte order without duplicating the format constant. Callers
must still use `ZZ9KQueryCaps()` / `ZZ9KQueryService()` to confirm that
`ZZ9K_CAP_IMAGE_DECODE` is advertised by the firmware they are running.
`zz9k_image_stream_required_service_flags()` builds the service flag mask for a
streaming JPEG or PNG session, including the output-mode-specific framebuffer or
tile requirement when needed.
The image service descriptor uses service-specific flag bits to advertise image
backend capabilities:

- `ZZ9K_SERVICE_FLAG_IMAGE_JPEG_BASELINE`: baseline JPEG decode is available.
- `ZZ9K_SERVICE_FLAG_IMAGE_JPEG_PROGRESSIVE`: progressive JPEG decode is
  available.
- `ZZ9K_SERVICE_FLAG_IMAGE_JPEG_DIRECT_BGRA`: the backend can emit BGRA pixels
  directly instead of decoding to an intermediate RGB buffer first.
- `ZZ9K_SERVICE_FLAG_IMAGE_JPEG_SCALING`: the backend can scale during JPEG
  decode.
- `ZZ9K_SERVICE_FLAG_IMAGE_PNG_DIRECT_BGRA`: streaming PNG decode can write
  direct 32-bit BGRA output to a surface or mapped framebuffer. PNG tile output,
  fit scaling, APNG, and interlaced PNG are intentionally not part of this first
  decoder slice.
- `ZZ9K_SERVICE_FLAG_IMAGE_SCALE_BILINEAR`: the scaler accepts
  `ZZ9K_SCALE_BILINEAR` for 32-bit surfaces.
- `ZZ9K_SERVICE_FLAG_IMAGE_SCALE_CLIPPED`: the scaler accepts clipped
  destination jobs through `ZZ9KScaleImageClipped()`.
- `ZZ9K_SERVICE_FLAG_IMAGE_STREAMING_INPUT`: compressed input can be fed in
  bounded chunks through an image session.
- `ZZ9K_SERVICE_FLAG_IMAGE_TILE_OUTPUT`: image sessions can emit bounded
  BGRA tiles instead of requiring a full output surface.
- `ZZ9K_SERVICE_FLAG_IMAGE_FRAMEBUFFER_OUTPUT`: image sessions can write
  directly to the mapped RTG framebuffer.

The current libjpeg-turbo backend advertises baseline JPEG, progressive JPEG,
direct BGRA output, and bounded fit scaling for image sessions. The fit path
uses libjpeg-turbo DCT scaling first, then a bounded row scaler when the image
is still larger than the target framebuffer.
The current libpng backend advertises direct BGRA output only; use the generic
ARM scaler after decode when PNG fit/upscale behavior is needed.
`zz9k-services` prints these flags as `jpeg-baseline`, `jpeg-progressive`,
`jpeg-direct-bgra`, `jpeg-scaling`, `png-direct-bgra`, `scale-bilinear`,
`scale-clipped`, `streaming-input`, `tile-output`, and `framebuffer-output`.

### Image Tool Smoke Tests

`zz9k-jpeg` runs the built-in 2x2 JPEG vector when no file is supplied. It can
also decode a JPEG file into an offscreen BGRA surface, or show it in an RTG
window with `--fb`:

```sh
zz9k-jpeg
zz9k-jpeg Work:Pictures/test.jpg
zz9k-jpeg --fb Work:Pictures/test.jpg
zz9k-jpeg --fb --fit Work:Pictures/large.jpg
zz9k-jpeg --view Work:Pictures/test.jpg
zz9k-jpeg --fb --resize --hold 500 Work:Pictures/test.jpg
zz9k-jpeg --fb --hold 200 Work:Pictures/test.jpg
zz9k-jpeg --fb --keep Work:Pictures/test.jpg
```

On AmigaOS, `--fb` opens the shared image-window helper in fixed-size mode,
streams JPEG input into a bounded ARM-local BGRA source surface, and asks the
ARM scaler to draw that source into the window inner rectangle. This keeps the
smoke test focused on JPEG decode plus ARM-side scale-to-framebuffer without
copying pixels through the m68k CPU. Large JPEGs are decoded to a
framebuffer-bounded source surface first, then scaled to the fixed window area.
The tool backs up the affected window framebuffer rectangle with an ARM-local
surface copy, shows the image, and restores the rectangle before exiting. The
shared image-window helper requests refresh events even in fixed-size mode, and
treats refresh as a redraw trigger. Visible framebuffer rendering is split into
small clipped-scale bands so large bilinear draws do not monopolize a single
mailbox call. Windowed redraws ask the helper for currently visible clips
before filling or scaling the image, and windowed restore uses the same visible
clip list before copying saved pixels back. `--hold N` changes the display
time. `--view` is the standalone viewer mode: it implies framebuffer windowed
output, enables resizing, and waits for the viewer window to close instead of
using a fixed diagnostic hold time.

`--resize` is an opt-in hardware test for the clipped scaler path. It enables
the window size gadget, handles resize events, and redraws through
`ZZ9KScaleImageClipped()` after checking the image service for `scale-clipped`.
The normal `--fb` path remains fixed-size until the resizable path has more
real-hardware coverage.

`zz9k-png` is the first PNG hardware smoke tool. It streams a PNG file through
the image-session API in bounded input chunks and writes direct BGRA output to
an ARM-local surface by default, or draws it through an Intuition window with
`--fb`:

```sh
zz9k-png Work:Pictures/test.png
zz9k-png --fb Work:Pictures/test.png
zz9k-png --fb --fit Work:Pictures/large.png
zz9k-png --view Work:Pictures/test.png
zz9k-png --fb --resize --hold 500 Work:Pictures/test.png
zz9k-png --fb --hold 200 Work:Pictures/test.png
zz9k-png --fb --keep Work:Pictures/test.png
```

On AmigaOS, `--fb` opens the same shared image-window path used by JPEG. The
PNG decoder writes to an ARM-local BGRA source surface, then the ARM scaler
draws into the window inner rectangle. Refresh and resize redraws query visible
window clips before submitting clipped scale bands, and the affected
framebuffer rectangle is backed up with ARM surface-copy operations unless
`--keep` is supplied. Windowed restore copies only the currently visible clips
from that backup surface. Static non-interlaced PNG files are supported; APNG,
interlaced PNG, and tile output are still rejected.

`zz9k-view` is the standalone ZZ9000 viewer for the SDK v2 image path. It
accepts one or more JPEG or PNG files, opens one resizable Intuition window,
decodes the current image into an ARM-local surface, and redraws the fitted
image through visible layer clips with sliced `ZZ9KScaleImageClipped()` jobs:

```sh
zz9k-view Work:Pictures/test.jpg Work:Pictures/test.png
```

Use Space/Right/Down for next image navigation, Left/Up/Backspace for previous,
`r` to redraw/refit, and `q`, Esc, or the close gadget to exit. The lower-level
`zz9k-jpeg --view` and `zz9k-png --view` commands remain useful codec smoke
paths; `zz9k-view` is the user-facing demo viewer.

These CLI tools are hardware smoke tests. User-facing viewer and DataType output should
enumerate visible layer or damage clip regions and submit clipped scale bands
for those regions only, so refresh, occlusion, and window stacking semantics
match Intuition/Layers instead of the diagnostic framebuffer rectangle model.
The shared image-window helper now includes host-tested rectangle intersection
and damage-clip list construction through
`zz9k_image_window_intersect_rect()` and
`zz9k_image_window_build_damage_clips()`. On AmigaOS,
`zz9k_image_window_visible_clips()` walks the window layer `ClipRect` list,
skips obscured clip rectangles, intersects the remaining visible rectangles
with the requested damage rectangle, and returns the clipped draw list.
The JPEG/PNG smoke tools use that list for visible image fill/scale redraws and
visible image restore. They are now suitable for real-hardware testing of
window stacking behavior, while final DataType/viewer output should use the
same visible-clip rendering model without relying on a diagnostic hold/restore
workflow.

`zz9k-jpeg-stream-demo` is the public `zz9k.library` version of the same
bounded-input path. It is intended as a starting point for datatype classes or
other Amiga-side libraries that must use the stable LVO interface instead of the
SDK host helper internals:

```sh
zz9k-jpeg-stream-demo Work:Pictures/test.jpg
zz9k-jpeg-stream-demo --fb Work:Pictures/test.jpg
```

The tool prints allocator diagnostics before allocating its shared input
buffer. If it reports `no-memory`, check the printed shared-buffer count,
surface count, invalid allocator slot count, and largest free heap block to
distinguish leaked SDK resources from an allocator metadata problem.

## Audio Decode Jobs

The audio service reserves explicit feature flags for this service family:
`mp3-decode`, `pcm-mix`, `resample`, and `pcm16-stereo`. Firmware must only
advertise these after the matching `ZZ9K_OP_DECODE_MP3`, `ZZ9K_OP_MIX_AUDIO`,
or `ZZ9K_OP_RESAMPLE_AUDIO` path is implemented and tested on real hardware.
`mp3-decode` is decode-to-PCM capability, not a promise that ZZ9000AX is
installed. Firmware can expose MP3 decode for all ZZ9000 users and separately
route decoded PCM to ZZ9000AX/card-local output when that optional hardware is
available.

MP3 decode jobs use shared buffers for both input and output:

```c
ZZ9KAudioDecodeDesc desc;
ZZ9KAudioDecodeResult result;

zz9k_audio_build_decode_desc(
    &desc, input.handle, 0, input.length, output.handle, 0, output.length,
    0, 0, ZZ9K_AUDIO_SAMPLE_FORMAT_S16LE, ZZ9K_AUDIO_DECODE_FLAG_EXPECT_END);

if (ZZ9KDecodeMp3(&desc, &result) == ZZ9K_STATUS_OK) {
  /* result.bytes_written bytes of PCM are in output */
}
```

`output_hz` and `output_channels` may be zero to request the source/native MP3
rate and channel count. Callers that need this resident LVO should check
`ZZ9K_LIBRARY_MIN_REVISION_AUDIO_DECODE`.

## Decompression Jobs

SDK v2 reserves a buffer-to-buffer decompression job shape for archive tools:
`ZZ9K_OP_DECOMPRESS` under `ZZ9K_SERVICE_CODEC`. The operation reads compressed
bytes from one shared buffer and writes decompressed bytes to another shared
buffer:

```c
#include "zz9k/compression.h"

ZZ9KDecompressDesc desc;
ZZ9KDecompressResult result;

if (zz9k_compression_build_decompress_desc(
      &desc, ZZ9K_COMPRESSION_GZIP,
      compressed.handle, 0, compressed_length,
      output.handle, 0, output_capacity,
      ZZ9K_DECOMPRESS_FLAG_EXPECT_END)) {
  zz9k_decompress(ctx, &desc, &result);
}
```

The same request shape is used for `ZZ9K_COMPRESSION_LZMA_ALONE` and
`ZZ9K_COMPRESSION_LZMA2` when firmware advertises the matching codec service
flag. LZMA2 payloads use one byte of LZMA2 properties followed by the raw LZMA2
stream, which maps directly to the property byte stored in simple 7z folders.

Firmware that advertises `ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_TEST` also
supports `ZZ9K_OP_DECOMPRESS_TEST` through `zz9k_decompress_test()`. That
streamed test path reads the compressed shared buffer, decodes into an
ARM-local scratch buffer, and returns the decoded byte count plus CRC32 without
allocating a full decoded shared buffer. Archive `t` commands should prefer
this path because it avoids the packed-input plus full-output shared-heap
requirement of `ZZ9K_OP_DECOMPRESS`.

Firmware that advertises `ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_STREAM` adds the
streamed extraction path: `ZZ9K_OP_DECOMPRESS_STREAM_BEGIN`,
`ZZ9K_OP_DECOMPRESS_STREAM_READ`, and `ZZ9K_OP_DECOMPRESS_STREAM_CLOSE`.
Callers start a session with `zz9k_decompress_stream_begin()`, repeatedly ask
the ARM side to fill a reusable shared output buffer with
`zz9k_decompress_stream_read()`, write those chunks to disk, then close the
session. This still keeps archive/container parsing on the Amiga side, but it
removes the requirement to allocate the complete decoded file in the SDK shared
heap. Current firmware supports `ZZ9K_COMPRESSION_LZMA_ALONE` for LZMA-alone
files and simple `.7z` LZMA entries, plus `ZZ9K_COMPRESSION_LZMA2` for simple
`.7z` LZMA2 entries.

Firmware that also advertises `ZZ9K_SERVICE_FLAG_CODEC_DECOMPRESS_FEED` adds
`ZZ9K_OP_DECOMPRESS_STREAM_FEED` and `zz9k_decompress_stream_feed()`. A caller
begins the stream with `ZZ9K_DECOMPRESS_FLAG_FEED_INPUT`, feeds compressed
chunks through a reusable shared input buffer, reads decoded chunks into a
reusable shared output buffer, and feeds again whenever the read result reports
`ZZ9K_DECOMPRESS_RESULT_NEED_INPUT`. This removes the remaining full packed
shared-buffer requirement from streamed extraction. The generic
`decompress-feed` flag means the feed operation exists; zlib-backed algorithms
advertise their supported feed combinations explicitly with `deflate-feed`,
`zlib-feed`, and `gzip-feed`. LZMA-alone and LZMA2 feed support are covered by
their algorithm flags plus the generic `decompress-feed` flag for compatibility
with the first feed-capable firmware.

This is deliberately a decompression primitive, not a ZIP, LHA, or full 7z parser.
CLI tools and datatypes should parse archive/container metadata on the Amiga
side, then offload the expensive raw deflate, zlib, gzip, LZ4 block,
LZMA-alone, or LZMA2 payload work to the ARM side when firmware advertises the
matching `ZZ9K_SERVICE_CODEC` flags. This is the first 7z-relevant layer:
broader `.7z` extraction still needs deeper tool-side parsing for solid blocks,
multi-coder folders, filters, and encrypted/unsupported methods.

Programs that call `zz9k.library` directly can submit the same job through the
typed request layer with `zz9k_request_decompress()` and decode completions
with `zz9k_reply_decompress_result()`. A future resident LVO can wrap the same
descriptor without changing the wire format.

Callers must require `ZZ9K_CAP_COMPRESSION`, query `ZZ9K_SERVICE_CODEC`, and
check `zz9k_compression_required_service_flags(desc.algorithm)` before
submitting work. Unsupported algorithms are normal firmware-version
differences, not fatal SDK errors.

`zz9k-archive` is the first archive frontend built on this split. It supports:

```sh
zz9k-archive l archive.zip
zz9k-archive l archive.7z
zz9k-archive l archive.lha
zz9k-archive t archive.tar.gz
zz9k-archive t payload.lzma
zz9k-archive t --capacity 1048576 stream-with-unknown-size.lzma
zz9k-archive x -o RAM:Extracted archive.zip
zz9k-archive x --overwrite -o RAM:Extracted archive.zip
zz9k-archive x --skip-existing -o RAM:Extracted archive.zip
zz9k-archive x --dry-run -o RAM:Extracted archive.zip
zz9k-archive x --match Prefs/ -o RAM:Extracted archive.lha
zz9k-archive x --strip-components 1 -o RAM:Extracted archive.tar.gz
```

All extractable member names run through the same path-safety checks before
testing or extraction. Absolute paths, parent-directory components, backslashes,
and Amiga-style volume/device names containing `:` such as `SYS:Prefs` are
rejected rather than normalized into an output path.

Extraction refuses to replace existing output files by default and prints
`output exists, use --overwrite:` with the blocked path. Pass `--overwrite` to
allow replacement when that is intentional, or pass `--skip-existing` to leave
existing files untouched and continue extracting later members. Skipped files are
reported with an `s` line. Existing directories may be reused while creating
archive directory entries and parent paths, but file/directory type conflicts
are rejected. If an output file path is already a directory, the tool prints
`output path is a directory:`. If an archive directory entry would be created
where an output file already exists, the tool prints `output path is a file:`.

`--dry-run` applies the same extraction filters, stripped output paths, and
overwrite/skip policy checks without creating directories or writing files.
Entries that would be extracted are reported with a `dry` line.

`--match text` filters archive members by normalized archive path substring for
list, test, and extract commands. The same filter predicate is used for TAR,
tar.gz streaming, ZIP, LHA, and 7z entry loops. Parent directories needed by a
matching file are still created during extraction.

`--strip-components n` applies to extraction output paths and removes `n`
leading path components after archive path normalization and before output path
safety checks. Entries that become empty after stripping are skipped, which
matches the usual behavior for top-level archive directory entries.

The initial tool lists, tests, and extracts gzip, LZMA-alone, tar, tar.gz, and
ZIP entries using store or deflate. ZIP deflate, gzip, known-size LZMA-alone,
and simple 7z Deflate/LZMA/LZMA2 payloads are decompressed by the ARM codec
service. LZMA-alone streams with unknown unpacked size can be tested or
extracted when callers provide `--capacity bytes`. LHA level-0 and level-1
`-lh0-` stored members can be listed, tested, and extracted on the Amiga side,
and `-lhd-` directory entries are created during extraction. Level-1 and level-2
LHA headers are parsed enough to list base names, apply file-name and
directory-name extension headers, and skip extension-header chains. `-lh1-`,
`-lh5-`, `-lh6-`, and `-lh7-` members are tested and extracted through the
embedded LHa for UNIX decoder subset from `jca02266/lha` 1.14i-ac20220213.
Other compressed LHA methods remain unsupported until their decoder paths are
wired in. TAR, LHA, and ZIP container metadata stay on the Amiga side so
path handling, overwrite policy, and future file-system rules remain normal CLI
responsibilities. ZIP listing and stored-file test/extract use a
file-backed central-directory path: the tool reads only the EOCD tail and
central directory, then copies stored entry byte ranges directly from the
archive file. ZIP deflate test also uses the file-backed metadata path and
sends only the compressed entry range through the codec. With firmware that
supports raw-deflate `deflate-feed`, the tool streams the ZIP member from disk
through reusable shared input/output buffers for both `t` and `x`: tests
validate the decoded byte count without allocating either the full compressed
member or the decoded output, and extraction writes decoded chunks directly to
the output file.
Older firmware that only advertises generic `decompress-feed` falls back to the
one-shot `decompress-test` path, which still avoids the decoded output
allocation but requires the compressed member to fit in the shared heap. ZIP
deflate extraction on older firmware falls back to the legacy full-read path,
which is useful for small entries but cannot handle large packed members. The
file-backed ZIP parser accepts empty ZIP archives whose first record is the
end-of-central-directory marker, and accepts a central-directory digital
signature record after the expected entry records,
matching ZIPs produced by tools that include trailing central-directory
metadata. It also accepts Zip64 extra fields that resolve to 32-bit sizes and
local-header offsets in both central-directory and local headers, plus Zip64
EOCD records whose entry counts and central-directory ranges fit the current
32-bit model. This covers small archives emitted by tools that enable Zip64
metadata conservatively. Truly >2 GiB members remain outside the current
Amiga-side archive model.
Empty ZIP archives are accepted with zero entries. For non-empty ZIPs, the
central-directory entry is cross-checked against the local header name, flags,
method, CRC, and sizes before the payload offset is trusted; data descriptor
entries are allowed to omit CRC and sizes in the local header. ZIP member names
that use backslash path separators are normalized to `/`, and current-directory
path components such as leading `./` current-directory prefixes and embedded
`/./` are stripped, while duplicate slash separators are collapsed before the
common archive path-safety checks run. ZIP
archive-root metadata entries such as `./` are skipped rather than being
exposed as unsafe extractable paths. If a
ZIP deflate test still fails, the tool prints the entry name, packed size,
expected unpacked size, data offset, ZIP flags, and ARM test output limit;
extraction failures print the same container-to-codec boundary fields plus the
feed output limit.

Plain `.gz` files also use a file-backed metadata path when firmware advertises
`gzip-feed`: the tool parses the gzip header from the probe buffer, reads only
the four-byte ISIZE footer, and streams the compressed file through reusable
shared buffers for `t` and `x`. This avoids full packed-input and decoded-output
shared-heap allocations for ordinary gzip payloads. Gzip header filenames are
normalized with the same slash and current-directory component rules as archive
member names before list/extract paths use them. Files that look like
`.tar.gz`/`.tgz` use the same gzip feed path plus a streaming tar parser:
decoded tar blocks are consumed as they arrive, so `l`, `t`, and `x` preserve
tar semantics without materializing the whole decoded tar image in memory.
Firmware without explicit `gzip-feed` still falls back to the older full-read
path for compatibility. The tar parser normalizes current-directory path
components such as leading `./` and embedded `/./`, collapses duplicate slash
separators,
skips archive-root metadata entries such as `./` without consuming output entry
slots, applies GNU long-name metadata records to the following entry, and
applies per-file PAX `path=`
records before running the common path-safety checks. GNU long-name and PAX
path metadata that resolves the following entry to archive root is skipped
instead of rejecting the archive. Tar directory entries are
recognized by either directory typeflag or a trailing slash name. Unsupported tar filesystem
types such as symlinks, hardlinks, device nodes, FIFOs, volume headers, and GNU
long-link metadata are skipped rather than being extracted as misleading regular
files. Empty tar archives and metadata-only tar streams are accepted with zero
extractable entries. Raw tar detection accepts both `ustar` archives and
legacy checksum-only tar headers, and it recognizes true empty tar archives
with the required zero blocks. Streaming tar/tar.gz handling allocates PAX
metadata storage from the PAX header size instead of using a 512-byte fixed
buffer, so ordinary extended headers with extra timestamp/comment records can
still feed the following file entry. PAX `size=` records are honored in full-read listing,
pre-counting, and streaming tar/tar.gz extraction, so archives that need PAX
size metadata do not lose synchronization after the following file header.
Other tar metadata layers remain future compatibility work. Tar headers are
checksum-validated in both full-read and streaming paths before the entry
metadata is trusted; both POSIX unsigned and historical signed checksum sums
are accepted. Tar size fields accept normal octal and positive GNU/POSIX
base-256 numeric encoding within the SDK's current 32-bit archive model. ZIP
directory entries are recognized either by a trailing slash in the
central-directory name or by DOS/Unix directory bits in the central directory
external attributes.

Gzip, ZIP, and simple 7z CRC metadata is also preserved across the tool
boundary. Gzip `t`/`x` and tar.gz feed operations compare the ARM-reported
decoded CRC32 against the gzip trailer, while ZIP stored entries are checked on
the Amiga side and ZIP deflate entries compare the ARM result CRC32 against the
central directory before reporting success. Simple 7z Copy entries validate the
file data CRC on the Amiga side using a streamed file-range CRC, and simple 7z
Deflate, LZMA, and LZMA2 entries compare the ARM result CRC32 whenever the
unencoded 7z metadata provides a per-file digest. Gzip headers that set FHCRC
are rejected if the header CRC16 does not match.

When the codec service reports `decompress-stream`, `zz9k-archive x` uses the
streamed extraction path for LZMA-alone payloads and 7z LZMA/LZMA2 entries. It keeps
the packed input in shared memory and reuses a small shared output buffer for
decoded chunks, so large decoded files no longer require a same-sized shared
allocation. When `decompress-feed` is also present, the tool prefers feed-mode
testing and extraction and reuses both the shared input buffer and shared output
buffer, so large packed files no longer require a full packed shared allocation
either. For raw LZMA-alone testing/extraction, the feed path probes only the file
header first and then streams the archive file directly from disk into the
reusable shared input buffer. For simple unencoded 7z LZMA/LZMA2 testing/extraction,
the feed path reads only the 7z metadata header into Amiga memory, then seeks to
the packed member range and streams that range directly from disk with either
the synthetic 13-byte LZMA-alone header or the 1-byte LZMA2 property prefix
prepended in the shared input buffer.
For simple unencoded 7z Deflate testing/extraction, the same disk-streaming
path sends the packed member range directly to the raw-deflate codec with no
synthetic prefix.
For 7z listing and Copy-method extraction, the tool also uses that metadata-only
file path; Copy entries are written by copying the packed file range directly
from the archive to the output file.

`.7z` support currently handles unencoded 7z headers well enough to list, test,
and extract empty-file/empty-directory metadata and simple 7z Copy stored-file
streams without involving the codec service. Copy folders with multiple
substreams are flattened into normal file entries when the folder is stored
contiguously, and true zero-file 7z archives stay on the metadata-only path, so
a basic multi-file Copy archive can be listed, tested, and
extracted without reading the full container into memory. A single-folder 7z Deflate
stream with one file and one pack stream is offloaded through the raw Deflate
codec. A single-folder 7z LZMA stream with one file, one pack stream, and the
normal 5-byte LZMA coder properties is converted to the LZMA-alone stream shape
and offloaded through the existing codec service. With `decompress-feed`,
test/extract sends the synthetic LZMA-alone header separately from the packed
7z bytes instead of allocating a complete wrapped payload; on the disk-streaming
path it also avoids reading the full 7z container before the operation. Copy-encoded 7z headers
are decoded by reading only the encoded header stream range and then parsing
the resulting metadata normally; if that encoded header stream has CRC
metadata, the copied bytes are validated before parsing. Deflate, LZMA, and
LZMA2 encoded 7z headers are decoded through the codec service before parsing,
which covers more archives produced with header compression enabled.
The 7z parser now validates both the start-header CRC and the next-header CRC
before trusting the metadata, and validates simple per-file Copy/LZMA data CRCs
when the archive provides them. Common 7z timestamp and attribute metadata is
skipped deliberately, while unknown file metadata properties are rejected rather
than silently accepted. 7z member names are decoded from UTF-16, backslash
separators are normalized to `/`, current-directory path components such as
leading `./` and embedded `/./` are stripped, and duplicate slash separators
are collapsed before the common path-safety checks run. Simple compressed
multi-substream Deflate/LZMA/LZMA2 folders are listed with their substream
names and sizes; on the file-backed `t`/`x` path, `zz9k-archive` decodes the
folder once with `decompress-feed`, splits the decoded bytes across the listed
entries, and validates CRCs only for selected substreams when the archive
provides them. 7z
archive-root metadata entries such as `./` are skipped and do not consume
output entry slots. A single-folder 7z LZMA2 stream with one file, one pack
stream, and the normal 1-byte LZMA2 property is offloaded through the LZMA2
codec service. If the LZMA2 feed/stream path reports `no-memory` while
allocating the ARM-side dictionary, `zz9k-archive` falls back to a one-shot
LZMA2 decode for entries whose decoded output fits in the SDK shared heap; that
one-shot firmware path uses the decoded output buffer as the dictionary. Larger
entries with large LZMA2 dictionary properties still require enough ARM heap for
the streaming dictionary. Multi-coder/filter chains, encrypted streams, and more
complex 7z solid layouts remain future parser layers. Unsupported 7z folder
layouts now report `7z unsupported layout: ...` with the parser reason, such as
multi-coder/filter-chain folders or multiple input/output streams.

7z LZMA no-memory diagnostics are printed on failed test or extract jobs. The
tool reports the packed size, requested output size, parsed dictionary size, and
the 5-byte LZMA properties, then prints allocator state including Shared heap
free and Largest free block. If the shared heap is healthy but the codec reports
`no-memory`, the failure is likely inside the ARM-side LZMA decoder workspace
allocation, usually due to a large dictionary setting in the archive.

## Crypto Hash Jobs

SDK v2 defines a conservative hash/HMAC/MAC job shape for crypto offload.
Current firmware support covers SHA-1, SHA-256, HMAC-SHA1, HMAC-SHA256, and
Poly1305:

```c
ZZ9KCryptoHashDesc desc;
ZZ9KCryptoResult result;

if (zz9k_crypto_build_hash_desc(&desc, ZZ9K_CRYPTO_HASH_SHA256,
                                input.handle, 0, input_length,
                                output.handle, 0)) {
  ZZ9KCryptoHash(&desc, &result);
}
```

For HMAC, use `zz9k_crypto_build_hmac_desc()` with a key buffer handle, offset,
and length. Async callers can build the request with
`zz9k_request_crypto_hash()` and decode completion payloads with
`zz9k_reply_crypto_result()`.

For Poly1305, use `zz9k_crypto_build_poly1305_desc()` with a 32-byte one-time
key.

This is primitive crypto ABI only. It is not an HTTPS stack, AmiSSL adapter, or
browser integration layer. Callers must query for `ZZ9K_CAP_CRYPTO` and service
availability before using it, and should treat unsupported algorithms as a
normal firmware-version result.

The `zz9k-hash` tool is a real-card smoke test for this path. With no
arguments it hashes the built-in `abc` vector and verifies the returned
SHA-256 digest:

```sh
zz9k-hash
zz9k-hash --alg sha1
zz9k-hash --alg sha256 --hmac key abc
zz9k-hash --alg poly1305
```

`zz9k-crypto-demo` is the public `zz9k.library` example for these primitive
crypto services. It uses only LVO calls: one `ZZ9KCryptoHash()` SHA-256 job,
one `ZZ9KCryptoHashBatch()` with SHA-1 and SHA-256 descriptors, one
`ZZ9KCryptoStream()` ChaCha20 RFC block vector, and one
`ZZ9KCryptoAead()` ChaCha20-Poly1305 RFC record with decrypt roundtrip:

```sh
zz9k-crypto-demo
```

The `zz9k-bench` tool also includes SHA-1, SHA-256, HMAC-SHA1, and
HMAC-SHA256 throughput lines when the crypto capability is advertised by the
firmware. It also measures 16 KiB HMAC-SHA256 and Poly1305 records through the
batch API, which is the closer shape for TLS-style workloads.

For multiple independent records, prefer `zz9k_crypto_hash_batch()` or
`ZZ9KCryptoHashBatch()` over repeated synchronous calls:

```c
ZZ9KCryptoHashDesc descs[16];
ZZ9KCryptoResult results[16];

/* Fill each descriptor with src/dst offsets and HMAC key fields. */
ZZ9KCryptoHashBatch(descs, results, 16, 16, ZZ9K_DEFAULT_TIMEOUT_TICKS);
```

The helper submits up to `max_in_flight` requests, maps completions back to
the matching result slot, and uses the existing mailbox timeout budget. Library
callers should require `ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_HASH_BATCH`.

## Crypto Stream Jobs

SDK v2 also defines a shared-buffer stream-cipher job shape. Current firmware
support covers ChaCha20 with a fixed 32-byte key and 12-byte nonce:

```c
ZZ9KCryptoStreamDesc desc;
ZZ9KCryptoResult result;

if (zz9k_crypto_build_chacha20_desc(&desc, input.handle, 0,
                                    input_length, output.handle, 0,
                                    key_buffer.handle, 0,
                                    nonce_buffer.handle, 0, 1)) {
  ZZ9KCryptoStream(&desc, &result);
}
```

This stream API is still primitive crypto ABI. It is not a TLS/AmiSSL
integration. Callers must query for `ZZ9K_CAP_CRYPTO` and service availability
before using it.

The `zz9k-chacha` tool is the real-card smoke test for this path. It runs an
RFC 8439 ChaCha20 block vector through the ARM service, then decrypts the
result with the same job shape and verifies the round trip:

```sh
zz9k-chacha
```

Library callers should require `ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_STREAM`.

For multiple independent stream chunks, prefer `zz9k_crypto_stream_batch()` or
`ZZ9KCryptoStreamBatch()` over repeated synchronous calls:

```c
ZZ9KCryptoStreamDesc descs[16];
ZZ9KCryptoResult results[16];

/* Fill each descriptor with src/dst offsets, key, nonce, and counter. */
ZZ9KCryptoStreamBatch(descs, results, 16, 16, ZZ9K_DEFAULT_TIMEOUT_TICKS);
```

The `zz9k-bench` tool measures ChaCha20 both as a full-buffer synchronous job
and as pipelined 16 KiB chunks. Library callers should require
`ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_STREAM_BATCH` for the batch helper.

## Crypto AEAD Jobs

SDK v2 exposes ChaCha20-Poly1305 as a single AEAD job for TLS-style records:

```c
ZZ9KCryptoAeadDesc desc;
ZZ9KCryptoResult result;

if (zz9k_crypto_build_chacha20_poly1305_desc(
      &desc, plaintext.handle, 0, plaintext_length,
      ciphertext.handle, 0, aad.handle, 0, aad_length,
      key_buffer.handle, 0, nonce_buffer.handle, 0)) {
  ZZ9KCryptoAead(&desc, &result);
}
```

Encryption writes ciphertext at `dst_offset` and appends the 16-byte tag
immediately after it. Decryption sets `ZZ9K_CRYPTO_AEAD_FLAG_DECRYPT`, reads
ciphertext from `src_offset`, reads the tag immediately after the ciphertext,
and writes plaintext to `dst_offset`. `src_length` is always the plaintext or
ciphertext length, excluding the tag. The nonce buffer is read from offset 0.

The `zz9k-aead` tool runs the RFC 8439 ChaCha20-Poly1305 vector through the
ARM service, verifies ciphertext and tag, then decrypts and verifies the round
trip:

```sh
zz9k-aead
```

For multiple independent AEAD records, prefer `zz9k_crypto_aead_batch()` or
`ZZ9KCryptoAeadBatch()` over repeated synchronous calls:

```c
ZZ9KCryptoAeadDesc descs[16];
ZZ9KCryptoResult results[16];

/* Fill each descriptor with src/dst offsets, AAD, key, and nonce fields. */
ZZ9KCryptoAeadBatch(descs, results, 16, 16, ZZ9K_DEFAULT_TIMEOUT_TICKS);
```

Library callers should require `ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_AEAD` for
the single-call helper and `ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_AEAD_BATCH` for
the batch helper. `zz9k-bench` measures the synchronous full-buffer AEAD path
as `ARM ChaCha20-Poly` and pipelined 16 KiB records as
`ARM ChaCha20-Poly pipe`.

## Async Calls

For AmigaOS programs, prefer event-driven message-port completion. Include
`zz9k/event_wait.h` if you want the small header-only timer helper used by the
packaged example:

```c
#include "zz9k/event_wait.h"

ZZ9KEventTimer timer;
struct MsgPort *port = CreateMsgPort();
ZZ9KAsyncRequest async;
ZZ9KRequest request;
ULONG signals;
int status;

/* Fill request. */
ZZ9KCallAsyncMsg(&async, &request, port);

zz9k_event_timer_open(&timer);
status = zz9k_event_wait_msg(&timer, port, &async, 2,
                             SIGBREAKF_CTRL_C, &signals);
if (status == ZZ9K_STATUS_OK) {
  /* async.reply is valid here */
}
zz9k_event_timer_close(&timer);
```

Batch callers can queue several requests to the same port with
`ZZ9KCallAsyncBatchMsg()`. This is the preferred shape for datatypes, archive
workers, crypto batches, and scaler/decode pipelines that need several jobs in
flight. `zz9k_event_wait_async_port()` waits for one or more dispatcher-posted
async replies on a dedicated SDK reply port, and
`zz9k_event_drain_async_port()` drains replies that were already queued before
the caller reached the wait point:

```c
ZZ9KAsyncRequest *completed[8];
uint32_t drained;
ULONG signals;

status = zz9k_event_wait_async_port(&timer, port, 2,
                                    SIGBREAKF_CTRL_C,
                                    completed, 8, &drained, &signals);
if (status == ZZ9K_STATUS_OK) {
  /* completed[0..drained-1] point at finished ZZ9KAsyncRequest objects. */
}
```

If a program naturally owns several SDK reply ports, use the plural helpers
instead of hand-building an Exec wait mask. `zz9k_event_wait_async_ports()`
waits on every supplied port plus the timeout and cancel signals, while
`zz9k_event_drain_async_ports()` collects already-posted replies across the
same port set:

```c
struct MsgPort *ports[2];
ZZ9KAsyncRequest *completed[8];
uint32_t drained;
ULONG signals;

status = zz9k_event_wait_async_ports(&timer, ports, 2, 2,
                                     SIGBREAKF_CTRL_C,
                                     completed, 8, &drained, &signals);
```

Library revision 3 added `ZZ9KWaitAsync()`. It waits for one queued async
request while still allowing unrelated completions to be dispatched. Library
revision 4 adds `ZZ9KWaitAsyncBatch()`, which waits until every entry in an
async array is complete or the poll budget expires. On timeout it leaves
unfinished requests queued; callers can cancel them explicitly or continue
waiting later.

Library revision 16 makes the resident AmigaOS wait helpers IRQ-aware when the
firmware advertises `ZZ9K_CAP_IRQ_COMPLETION`. `ZZ9KWaitAsync()` and
`ZZ9KWaitAsyncBatch()` temporarily enable SDK completion interrupts, wait for
the shared ZZ9000 interrupt server signal, then drain completions through the
same dispatcher path. If IRQ completion is not available, the helpers keep the
older polling behavior. Callers that need custom scheduling can still drive
completion manually with `ZZ9KPoll()`.

Library revision 17 adds a resident event dispatcher. After a successful
`ZZ9KCallAsync()`, `ZZ9KCallAsyncBatch()`, `ZZ9KCallAsyncMsg()`, or
`ZZ9KCallAsyncBatchMsg()`, the library starts a small dispatcher process when
the firmware advertises `ZZ9K_CAP_IRQ_COMPLETION`. The dispatcher owns the SDK
completion interrupt wait signal, drains completions inside the library, and
posts the usual callback or Exec message-port reply. This lets event-driven
programs wait on their own message ports without explicitly calling
`ZZ9KPoll()`.

`zz9k_event_wait_msg()` is a convenience wrapper around `Wait()` and `GetMsg()`
for one outstanding request. It returns `ZZ9K_STATUS_OK`, the async completion
status, `ZZ9K_STATUS_TIMEOUT`, `ZZ9K_STATUS_CANCELLED` when a caller-supplied
cancel signal such as `SIGBREAKF_CTRL_C` wins, or `ZZ9K_STATUS_INTERNAL_ERROR`
for an unexpected message/state. `zz9k_event_wait_async_port()` is the matching
helper for several outstanding requests on one reply port.
`zz9k_event_wait_async_ports()` and `zz9k_event_drain_async_ports()` handle the
same pattern across several reply ports.
Empty reply-port signals are treated as stale.
The wait helpers keep waiting until a completion, cancel signal, or timeout is
observed.
`zz9k_event_wait_signal()` covers callback-style code that signals the caller's
task rather than posting an Exec message. The older
`ZZ9KWaitAsync()` and `ZZ9KWaitAsyncBatch()` helpers remain useful for
compatibility paths and simple command-line tests, but new applications should
usually wait on Exec message ports directly. That keeps SDK completions
composable with Intuition windows, timers, sockets, and Ctrl-C handling.

Callbacks delivered by the dispatcher run in the dispatcher process. Keep them
short: copy small status fields, signal your own task, and return. Do not call
back into `zz9k.library` from such callbacks; use an Exec message port for
larger event-driven workflows.

Programs that require dispatcher-delivered message-port completion should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_EVENT_DISPATCHER) {
  /* fallback to ZZ9KPoll() / ZZ9KWaitAsync(), or refuse to run */
}
```

Library revision 20 keeps INT6 as the default interrupt line for the resident
library, matching the historical ZZ9000 setup. If a system is configured to
route the SDK completion interrupt through INT2, set `ENV:ZZ9K_INT2` before
the resident library installs its SDK interrupt server. The standalone
`zz9k-irqtest` diagnostic uses the same environment name.

Programs that require the wait helper should check:

```c
if (ZZ9KBase->lib_Revision < ZZ9K_LIBRARY_MIN_REVISION_WAIT_ASYNC) {
  /* fallback or refuse to run */
}
```

Programs that require the batch wait helper should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_WAIT_ASYNC_BATCH) {
  /* fallback or refuse to run */
}
```

Library revision 5 added the convenience acceleration wrappers
`ZZ9KDecodeImage()` and `ZZ9KCryptoHash()`. Programs that require them should
check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_ACCEL_JOB_WRAPPERS) {
  /* fallback or refuse to run */
}
```

Library revision 6 added `ZZ9KCryptoHashBatch()`. Programs that require the
public batch helper should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_HASH_BATCH) {
  /* fallback or refuse to run */
}
```

Library revision 7 added `ZZ9KCryptoStream()`. Programs that require stream
cipher jobs should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_STREAM) {
  /* fallback or refuse to run */
}
```

Library revision 8 added `ZZ9KCryptoStreamBatch()`. Programs that require the
stream batch helper should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_STREAM_BATCH) {
  /* fallback or refuse to run */
}
```

Library revision 9 added `ZZ9KCryptoAead()`. Programs that require AEAD jobs
should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_AEAD) {
  /* fallback or refuse to run */
}
```

Library revision 10 added `ZZ9KCryptoAeadBatch()`. Programs that require the
AEAD batch helper should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_AEAD_BATCH) {
  /* fallback or refuse to run */
}
```

Library revision 11 added `ZZ9KFillSurface()` and `ZZ9KCopySurface()`.
Programs that require ARM-side surface fill/copy should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_SURFACE_OPS) {
  /* fallback or refuse to run */
}
```

Library revision 12 extended `ZZ9KDiagInfo` with `surfaces_used` and
`allocator_invalid_slots`. Programs that read these fields should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_DIAG_SURFACES) {
  /* field unavailable */
}
```

Library revision 13 added public image-session wrappers:
`ZZ9KImageSessionBegin()`, `ZZ9KImageSessionFeed()`, and
`ZZ9KImageSessionClose()`. Programs that require bounded-input image decode
through `zz9k.library` should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_IMAGE_SESSIONS) {
  /* fallback or refuse to run */
}
```

Library revision 14 added `ZZ9KScaleImageClipped()`. Programs that require
clip-aware ARM-side scaling should check:

```c
if (ZZ9KBase->lib_Revision <
    ZZ9K_LIBRARY_MIN_REVISION_SCALE_IMAGE_CLIPPED) {
  /* fallback or refuse to run */
}
```

Library revision 21 added `ZZ9KDecodeMp3()`. Programs that require ARM-side
MP3-to-PCM decode should check:

```c
if (ZZ9KBase->lib_Revision < ZZ9K_LIBRARY_MIN_REVISION_AUDIO_DECODE) {
  /* fallback or refuse to run */
}
```

Library revision 22 added MP3 streaming sessions. Programs that require
bounded-memory MP3 decode should check:

```c
if (ZZ9KBase->lib_Revision < ZZ9K_LIBRARY_MIN_REVISION_AUDIO_STREAM) {
  /* fallback or refuse to run */
}
```

The streaming surface uses `ZZ9KAudioStreamBegin()`, `ZZ9KAudioStreamFeed()`,
`ZZ9KAudioStreamRead()`, and `ZZ9KAudioStreamClose()` so large files do not
need to fit in shared memory at once.

Library revision 23 added X25519 key exchange via `ZZ9KCryptoKeyExchange()`.
Programs that require it should check:

```c
if (ZZ9KBase->lib_Revision < ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_KX) {
  /* fallback or refuse to run */
}
```

The firmware additionally advertises X25519 support through the crypto
service flags; gate hardware use on `ZZ9K_SERVICE_FLAG_CRYPTO_X25519` from
`ZZ9KQueryService()`.

Library revision 24 added P-256 ECDH (the same `ZZ9KCryptoKeyExchange()` with
`ZZ9K_CRYPTO_KX_P256` and a 65-byte uncompressed peer point) and signature
verification via `ZZ9KCryptoVerify()`, which checks ECDSA-P256 or
RSA-PKCS1-2048 signatures over a SHA-256 digest. Gate these on
`ZZ9K_LIBRARY_MIN_REVISION_CRYPTO_VERIFY` and the
`ZZ9K_SERVICE_FLAG_CRYPTO_P256`, `ZZ9K_SERVICE_FLAG_CRYPTO_ECDSA_P256`, and
`ZZ9K_SERVICE_FLAG_CRYPTO_RSA_2048` service flags.
`ZZ9KAudioStreamBeginDesc.low_water_bytes` is reserved for input-ring refill
policy. `high_water_bytes` caps the amount of PCM firmware should produce in a
single mailbox call; pass zero for the firmware default, or a value below the
PCM ring capacity to keep decode work sliced for interactive callers.
`zz9k-mp3 --stats` prints timing counters for file reads, staging-buffer
copies, feed calls, PCM copies, output writes, and read acknowledgements. Run
without `--out` to measure decode/mailbox throughput without pulling PCM back
to DOS, then compare against `--out` to isolate transfer and filesystem cost.

## Async Cancellation

Library revision 2 added `ZZ9KCancelAsync()`. It removes a queued request from
the local pending list, completes it with `ZZ9K_STATUS_CANCELLED`, and posts the
usual callback or message-port completion locally. If the firmware later returns
the already-cancelled request, `ZZ9KPoll()` drains that stale completion without
reporting an internal error.

Programs that require cancellation should check:

```c
if (ZZ9KBase->lib_Revision < ZZ9K_LIBRARY_MIN_REVISION_CANCEL_ASYNC) {
  /* fallback or refuse to run */
}
```

## Register Clobbers

The inline caller header treats `d1`, `a0`, and `a1` as scratch registers across
every LVO call. Do not rely on local variables staying in those registers after
a `ZZ9K*()` call. This is already handled by `amiga/include/proto/zz9k.h`.

## Example

See `examples/amiga-library/zz9k-library-demo.c` for a complete synchronous
and message-port async example.
