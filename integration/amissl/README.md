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

Inside the m68k AmiSSL build environment (the
`sacredbanana/amiga-compiler:m68k-amigaos` image has `m68k-amigaos-gcc`, `sfdc`,
`clib2`, and `make`):

```sh
# From the repository root, with an AmiSSL checkout beside it at ../amissl:
docker run --rm \
  -v "$PWD:/sdk" -v "$PWD/../amissl:/amissl" \
  sacredbanana/amiga-compiler:m68k-amigaos \
  sh -c 'AMISSL_DIR=/amissl ZZ9000_SDK=/sdk /sdk/integration/amissl/build.sh'
```

Or let the script clone AmiSSL itself (drop `AMISSL_DIR` and the `../amissl`
mount). The built library lands in `work/out/` (gitignored). `OS=os3` builds the
m68k library; the other AmiSSL CPU targets (`os3-68020`, …) build the same way.

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
