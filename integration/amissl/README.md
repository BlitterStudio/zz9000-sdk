# ZZ9000 provider baked into AmiSSL (Path A)

This directory builds a drop-in `amissl.library` with the ZZ9000 crypto-offload
provider compiled **inside** it, so that **every** application opening the
library — browsers (AWeb3, IBrowse, Voyager), mail clients, anything using
AmiSSL — transparently gets hardware-accelerated TLS with no changes of its own.

It is the production counterpart to the application-side provider described in
[`../../docs/zz9k-amissl-provider.md`](../../docs/zz9k-amissl-provider.md). That
path only accelerates an application that calls `zz9k_amissl_register()` itself;
this path accelerates the whole system.

## How it works

AmiSSL keeps per-application OpenSSL state, so a provider registered by one
program is invisible to others. To reach prebuilt browsers, the provider must
live in the library and be registered by the library during `InitAmiSSL`.

The integration is deliberately thin:

* **The provider sources stay in this repo** (`amiga/provider/`, `host/src/`,
  `tools/`). They are versioned, reviewed, and host-tested here.
* **`amissl-zz9000.patch`** is the only change to AmiSSL — two hunks:
  1. `Makefile`: compile our objects (referenced by `$(ZZ9000_SDK)` path) and
     add them to `LIBOBJS`.
  2. `src/amissl_library.c`: call `zz9k_provider_register_builtin()` inside
     `InitAmiSSLA`, after OpenSSL is set up, so each application's default
     context prefers the ZZ9000 (`?provider=zz9000`) with software fallback.

Inside the library the provider links directly against the statically-linked
OpenSSL core — no AmiSSL inline redirection, no register-convention boundary.
The board is opened once per OpenSSL context (`zz9k_provider_init`); if it is
absent or a service is missing, operations fall back to AmiSSL software, so the
modified library is safe to ship even to users without a ZZ9000.

`AMISSL_REF` pins the upstream AmiSSL commit the patch is validated against
(currently AmiSSL 5.27 + 1). To track a newer AmiSSL: bump `AMISSL_REF`, re-run,
and only re-touch the patch if those two spots moved.

## Building

### Recommended: GitHub Actions (`.github/workflows/build-amissl-provider.yml`)

AmiSSL + OpenSSL 3.x build from source with an old m68k toolchain (adtools,
gcc 2.95.3). That toolchain is finicky outside the exact environment AmiSSL's
own CI uses, so the supported path is a workflow that mirrors it: an
`ubuntu-24.04` runner, the adtools build env, a clean (LF, ext4) checkout of
both this repo and AmiSSL at the pinned ref, `build.sh`, and an uploaded
`amissl.library` artifact for `os3-68020` and `os3-68060`.

Run it from the Actions tab ("Build AmiSSL + ZZ9000 provider" → Run workflow),
or it runs automatically when the provider/integration files change. Download
the artifact, and that's your drop-in library. This is the basis for cutting
binary releases.

### `build.sh` (used by the workflow; also for local builds)

`build.sh` does the actual work — fetch/locate AmiSSL at `AMISSL_REF`, apply the
patch, `make OS=os3-68020 ZZ9000_SDK=…`. It needs the adtools m68k toolchain on
`PATH` (`/opt/m68k-amigaos/bin` for the cross-gcc, `/usr/local/amiga/bin` for
`bumprev`/utils). `OS=os3-68020` (default) builds the 68020+ library;
`OS=os3-68060` builds the 68060 one. Output lands in `work/out/` (gitignored).

**Local-build caveats** (why CI is recommended): the adtools 2017 gcc is a 32-bit
binary that overflows on Windows bind-mount inodes (`Value too large for defined
data type`) — build on the container's own ext4 (copy sources off the mount).
And it rejects `\`-continuations followed by CR, so a Windows (CRLF) checkout of
AmiSSL must be normalised to LF (`dos2unix`) first. A Linux/ext4 checkout — what
the workflow uses — has neither problem. (The `sacredbanana/amiga-compiler`
image compiles the provider objects fine but is *not* a clean AmiSSL-from-source
environment: its NDK conflicts with AmiSSL's `libcmt` and it lacks `bumprev`.)

## Installing and testing

Copy the built `amissl.library` over the one in `LIBS:` (keep a backup), then run
any TLS application. To confirm offload is active, use the self-test from the
application-side docs, or watch a TLS handshake — anything negotiating X25519,
AES-GCM/ChaCha20-Poly1305, or an ECDSA-P256/RSA certificate is accelerated;
everything else falls back to software.

## Licensing

The ZZ9000 provider sources are GPL-3.0-or-later. AmiSSL and OpenSSL 3.x are
Apache-2.0, which is one-way compatible with GPLv3, so the combination is legal —
but the resulting `amissl.library` binary is then a GPLv3 combined work.
Redistributing it is fine (the source is public in this repo); redistributors
carry the GPLv3 obligations (offer source, keep notices).
