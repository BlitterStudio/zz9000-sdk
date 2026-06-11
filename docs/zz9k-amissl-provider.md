# ZZ9000 AmiSSL Provider

This document describes how the ZZ9000 crypto offload plugs into AmiSSL 5.x
(OpenSSL 3.6) as an OpenSSL 3 *provider*, how to build an application that uses
it, and how to verify on real hardware that operations are actually running on
the board.

It complements [zz9k-crypto-acceleration.md](zz9k-crypto-acceleration.md),
which covers the economics and the measured per-operation timings.

## How it fits together

OpenSSL 3 resolves every algorithm through a *provider*. The ZZ9000 provider is
a **built-in** (no-DSO) provider: it is compiled into the application and
registered at run time, rather than loaded from a `.so`/shared object. It
advertises the handful of operations the board accelerates and lets everything
else fall through to AmiSSL's own (default) provider.

```
  application
    │  EVP_* calls  (EVP_PKEY_derive, EVP_EncryptUpdate, EVP_DigestVerify, …)
    ▼
  AmiSSL / OpenSSL 3 core ── property query "?provider=zz9000"
    │                                   │
    ├── zz9000 provider                 └── default provider (software fallback)
    │     KEYEXCH  X25519                       everything not offloaded
    │     CIPHER   AES-128/256-GCM, ChaCha20-Poly1305
    │     SIGNATURE ECDSA-P256, RSA-PKCS1 (2048/3072/4096) verify
    │       │
    │       ▼  zz9k_offload.c
    │     ZZ9000 SDK  (zz9k_crypto_kx / _aead / _verify, shared buffers, mailbox)
    │       │
    │       ▼
    └─────► ZZ9000 firmware (ARM Cortex-A9)
```

The provider operation files are portable OpenSSL 3 code and are shared with the
host unit tests. Two things make them use the hardware:

* **`-DZZ9K_PROVIDER_OFFLOAD`** at compile time enables the offload hooks. Each
  operation (`zz9k_prov_x25519`, `zz9k_prov_aead`, `zz9k_prov_ecdsa_verify`,
  `zz9k_prov_rsa_verify`) first tries `zz9k_offload_*`; if the offload cannot
  run (no board, allocation/mailbox error, or a decrypt tag failure) it falls
  back to the bundled software reference — the same code the firmware was
  validated against. With the macro undefined (the host build) every operation
  uses the software reference, which is why the host unit tests stay meaningful.
* **An open SDK context.** `zz9k_provider_init` calls `zz9k_open()` once for the
  provider's lifetime and records the advertised crypto service flags. If the
  board is absent the provider still loads and every operation transparently
  uses software.

## Source files

| File | Role |
| --- | --- |
| `amiga/provider/zz9k_provider.c` | Provider core: params, operation query, init/teardown (opens the SDK context under `ZZ9K_PROVIDER_OFFLOAD`). |
| `amiga/provider/zz9k_algorithms.c` | Central `OSSL_ALGORITHM` tables. |
| `amiga/provider/zz9k_x25519.c` | X25519 KEYMGMT + KEYEXCH. |
| `amiga/provider/zz9k_aead.c` | AES-128/256-GCM and ChaCha20-Poly1305 ciphers. |
| `amiga/provider/zz9k_ecdsa.c` | EC P-256 KEYMGMT + ECDSA verify. |
| `amiga/provider/zz9k_rsa.c` | RSA KEYMGMT + RSA-PKCS1-SHA256 verify (2048/3072/4096). |
| `amiga/provider/zz9k_offload.c` | SDK bridge: marshals an operation into shared buffers and runs it through the SDK. **Amiga-only.** |
| `amiga/provider/zz9k_amissl.c` | `zz9k_amissl_register()` / `zz9k_amissl_unregister()` — registers the provider with AmiSSL's default library context. **Amiga-only.** |
| `amiga/provider/zz9k_amissl_selftest.c` | Standalone hardware self-test program. **Amiga-only.** |
| `tools/zz9k-crypto-soft.c` | Portable software reference (fallback + host tests). |

## Building an application

Compile the provider objects together with your program and define
`ZZ9K_PROVIDER_OFFLOAD`. Using the `sacredbanana/amiga-compiler:m68k-amigaos`
image and an AmiSSL SDK checkout at `$AMISSL`:

```sh
CC=m68k-amigaos-gcc
INC="-I$AMISSL/include -Iinclude -Ihost/include -Itools -Iamiga/provider"
DEF="-DZZ9K_PROVIDER_OFFLOAD"

# Provider + offload + software reference (compile once, link into the app).
$CC -noixemul -O2 $DEF $INC -c \
    amiga/provider/zz9k_provider.c \
    amiga/provider/zz9k_algorithms.c \
    amiga/provider/zz9k_x25519.c \
    amiga/provider/zz9k_aead.c \
    amiga/provider/zz9k_ecdsa.c \
    amiga/provider/zz9k_rsa.c \
    amiga/provider/zz9k_offload.c \
    amiga/provider/zz9k_amissl.c \
    tools/zz9k-crypto-soft.c
```

Link the resulting objects with your application and the ZZ9000 SDK library
(`amiga/`), AmiSSL, and bsdsocket — exactly as a normal AmiSSL program links.
The provider adds no link dependencies of its own beyond the SDK.

## Registering the provider

Call `zz9k_amissl_register()` once, **after** a successful `InitAmiSSL()` and
**before** any crypto. It registers the built-in provider, loads it alongside
the default provider (kept for fallback), and sets the default property query to
`?provider=zz9000` so ordinary EVP calls prefer the hardware:

```c
#include "zz9k_amissl.h"

/* ... open libraries, OpenAmiSSL(), InitAmiSSL() ... */

if (!zz9k_amissl_register()) {
    /* registration failed — nothing is left registered; you can still run
       with AmiSSL's software crypto. */
}

/* ... use AmiSSL normally; offloadable operations now go to the ZZ9000 ... */

zz9k_amissl_unregister();   /* before CleanupAmiSSL() */
CleanupAmiSSLA(NULL);
```

No other application change is required: existing `EVP_*`, `SSL_*` and
`TLS` code transparently picks up the provider through the default property
query.

## Verifying on hardware

### Self-test program

`zz9k_amissl_selftest.c` is a complete AmigaOS 3 program that initialises
AmiSSL, registers the provider, and round-trips both AEADs:

* it **encrypts** with the default `?provider=zz9000` query (offloaded on the
  board) and prints which provider actually served the cipher, and
* it **decrypts** with a cipher pinned to `provider=default` (AmiSSL's software
  implementation) and checks the recovered plaintext and the authentication tag.

Because the decrypt uses an independent software implementation, a correct round
trip proves the ZZ9000 produced a standards-conformant ciphertext and tag — no
memorised test vector required. Expected output on a working board:

```
ZZ9000 AmiSSL provider self-test
--------------------------------
  AES-256-GCM            encrypt via 'zz9000', decrypt via 'default'
  AES-256-GCM            PASS (keylen=32)
  ChaCha20-Poly1305      encrypt via 'zz9000', decrypt via 'default'
  ChaCha20-Poly1305      PASS (keylen=32)
--------------------------------
Result: ALL PASS
```

The `encrypt via 'zz9000'` line is the proof the offload path was taken; if the
board is absent it reads `encrypt via 'default'` and the round trips still pass
(software on both sides). Build it like the application above, adding
`amiga/provider/zz9k_amissl_selftest.c` and linking against AmiSSL + the SDK.

### Confirming a specific operation is offloaded

For any algorithm, fetch it and read back the provider name:

```c
EVP_CIPHER *c = EVP_CIPHER_fetch(NULL, "AES-256-GCM", NULL);
const OSSL_PROVIDER *p = EVP_CIPHER_get0_provider(c);
printf("AES-256-GCM served by: %s\n", OSSL_PROVIDER_get0_name(p));
```

The equivalent introspection exists for the other operation classes
(`EVP_PKEY_get0_provider`, `EVP_SIGNATURE_get0_provider`,
`EVP_KEYEXCH_get0_provider`).

### Full TLS handshake

To exercise the handshake path (X25519 + ECDSA/RSA verify) end to end, adapt the
AmiSSL `test/https.c` example: add the provider objects to its build, define
`ZZ9K_PROVIDER_OFFLOAD`, and call `zz9k_amissl_register()` right after the
existing `InitAmiSSL()`. A successful HTTPS GET against a server that negotiates
`X25519` + an ECDSA-P256 or RSA certificate confirms the offloaded key exchange
and certificate verification produced a valid handshake.

## Fallback semantics

The provider is conservative: it never breaks an operation it cannot fully
handle.

* Operations not advertised by the provider resolve to the default provider via
  the `?` (optional) property query.
* RSA-PSS, non-SHA-256 digests, and moduli above 4096 bits are declined by the
  RSA operation, so they fall back to software.
* A decrypt whose tag fails to authenticate returns the correct authentication
  failure (the offload reports an error and the software reference then rejects
  the tag).
* If the board is missing or a mailbox call fails, the operation completes in
  software. The only observable difference is timing.
