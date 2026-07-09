# ZZ9000 SDK v2 - BlitterStudio fork

> **Fork notice.** This repository is an independent fork and continued
> development of the original MNT ZZ9000 ARM SDK. It is maintained by
> Dimitris Panokostas / **BlitterStudio** and is **not affiliated with,
> endorsed by, or supported by MNT Research GmbH**. The ZZ9000 hardware
> itself is designed and manufactured by MNT Research GmbH - hardware
> questions belong with them; SDK issues and fork-specific discussion
> belong here. The SDK v2 service-runtime implementation lives under
> `include/zz9k/`, `host/include/zz9k/`, `amiga/`, `tools/`, `examples/`,
> and `docs/`.
>
> Upstream (pre-fork): https://source.mnt.re/amiga/zz9000-sdk

The ZZ9000 is a graphics and ARM coprocessor card for Amiga computers equipped
with Zorro slots. It is based on a Xilinx ZYNQ Z-7020 chip that combines
7-series FPGA fabric with dual ARM Cortex-A9 CPUs clocked at 666 MHz. The
current hardware has 1 GB of DDR3 RAM and no soldered eMMC.

This repository is focused on SDK v2: a firmware-integrated mailbox/service ABI
exposed to AmigaOS through `zz9k.library`, with public helper headers under
`include/zz9k/` and Amiga examples under `examples/amiga-*`. The obsolete
fixed-address ARM launcher, standalone ARM examples, and their bare-metal
support libraries have been removed from this branch.

## What This Fork Adds

Compared with the older MNT ARM SDK, this repository now centers on a
stable AmigaOS-facing service runtime rather than standalone ARM sample
programs:

- `zz9k.library` plus SDK v2 headers for async calls, shared buffers,
  surfaces, image decode/scale, audio, compression, and crypto.
- End-user tools for service inspection, benchmarking, image viewing,
  MP3 playback, archive extraction, and release smoke checks.
- `zz9k-picture.datatype` and JPEG/PNG descriptors for optional
  DataTypes integration.
- `mpega.library` and audio-stream helpers used by the current MHI/MP3
  stack.
- LHA/LZH archive decompression offload, including batch verification
  paths that fall back to software when firmware support is absent.
- AmiSSL/OpenSSL 3 provider work for TLS acceleration: X25519, P-256
  ECDHE, P-256 ECDSA verify, RSA-2048 PKCS#1/SHA-256 verify, AES-GCM,
  and ChaCha20-Poly1305 where the board and firmware advertise support.
- Zorro 2-aware allocation flags (`HOST_WINDOW` / `CARD_ONLY`) so small
  audio staging buffers can stay CPU-visible while card-only rings avoid
  the 4 MB aperture limit.
- Docker packaging, host-side tests, ABI drift checks, and release smoke
  documentation.

Start new AmigaOS-side work with [`docs/zz9k-library.md`](docs/zz9k-library.md).
Firmware-side service metadata is described in
[`docs/zz9k-modules.md`](docs/zz9k-modules.md); conventions for third-party and
loadable-module services in the `0x8000+` range are in
[`docs/zz9k-vendor-services.md`](docs/zz9k-vendor-services.md).

Hardware-accelerated TLS — the ZZ9000 crypto-offload OpenSSL provider, shipped
as a drop-in `amissl.library` so every AmiSSL application gets faster
handshakes and bulk record crypto with no changes — is covered in
[`docs/zz9k-amissl-provider.md`](docs/zz9k-amissl-provider.md) and the
roadmap/measurements in
[`docs/zz9k-crypto-acceleration.md`](docs/zz9k-crypto-acceleration.md).

## Quick Start

Build the AmigaOS 3 SDK tools and package with Docker:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-m68k-amigaos.ps1
powershell -ExecutionPolicy Bypass -File scripts\package-m68k-amigaos.ps1
```

or from a POSIX shell:

```sh
./scripts/build-m68k-amigaos.sh
./scripts/package-m68k-amigaos.sh
```

The package is written to `build/package/amigaos3`. It contains:

- `Libs/zz9k.library`
- `Libs/mpega.library` as the runtime drop-in candidate, plus
  `Libs/mpega.library.zz9k` for side-by-side diagnostics
- CLI tools such as `zz9k-info`, `zz9k-services`, `zz9k-bench`,
  `zz9k-surfaceops`, `zz9k-mp3`, `zz9k-mpega-smoke`, `zz9k-jpeg`, `zz9k-png`,
  `zz9k-view`, `zz9k-hash`, `zz9k-chacha`, `zz9k-aead`, and
  archive/decompression tools including `zz9k-archive`
- `Classes/DataTypes/zz9k-picture.datatype` plus JPEG/PNG descriptors packaged
  inactive under `Storage/DataTypes` for explicit opt-in activation
- developer headers under `Developer/Include`
- public docs under `Docs`
- examples under `Examples`
- `MANIFEST.sha256` with SHA-256 checksums for every packaged file

SDK v2 requires matching ZZ9000 SDK-service firmware. Firmware v2.2.0 is the
older baseline for the service ABI, but current matched firmware, SDK payloads,
and drivers are expected for Zorro 2 host-window audio/MP3 allocation,
`ZZ9000.CFG` query support, and the newest service capability flags. After
installing the SDK package and booting that firmware, run this hardware smoke
check:

```text
zz9k-services --check-release
```

The command should end with `release check ok`.
For a broader package-level runtime pass, follow
[`docs/zz9k-release-smoke.md`](docs/zz9k-release-smoke.md).

For most application-side helper code, include `zz9k/sdk.h`; it pulls in the
stable SDK v2 ABI, host/request/reply types, and helper headers. Include
`proto/zz9k.h` as well when calling `zz9k.library` from AmigaOS.

Useful public helper headers for narrow includes:

- `zz9k/sdk.h`: application-facing umbrella header
- `zz9k/caps.h`: capability and service-flag checks plus stable bit names
- `zz9k/surface.h`: surface layout, colors, fill/copy descriptors
- `zz9k/image_geometry.h`: scale and clipped-scale descriptors
- `zz9k/image.h`: one-shot and streaming image decode descriptors
- `zz9k/shared.h`: bounds-checked shared-buffer byte access
- `zz9k/audio.h`: MP3 decode and streaming audio descriptors
- `zz9k/compression.h`: decompression and streaming decompression descriptors
- `zz9k/crypto.h`: hash, HMAC, Poly1305, ChaCha20, and AEAD descriptors
- `zz9k/text.h`: stable status text for user-facing tools

## Requirements

For AmigaOS 3 tools, the supported local path is Docker with the
`sacredbanana/amiga-compiler:m68k-amigaos` image, driven by the scripts above.

## Third-Party Code

The SDK carries third-party code where current SDK v2 tools need it:

- LHa for UNIX decoder subset (tools/lha-unix), from jca02266/lha
  1.14i-ac20220213, retaining the original LHa for UNIX redistribution terms

## License / Copyright

SDK v2, including the firmware-integrated service ABI, `zz9k.library`, AmigaOS
service tools, public SDK v2 headers, docs, examples, and packaging work in
this fork, is:

Copyright (C) 2024-2026, Dimitris Panokostas / BlitterStudio

Older pre-fork material from the original MNT ZZ9000 ARM SDK has been removed
from this branch unless retained in individual files with their own notices.

Unless a file carries a narrower notice or a third-party license, this
repository is distributed under:

SPDX-License-Identifier: GPL-3.0-or-later
https://spdx.org/licenses/GPL-3.0-or-later.html
