# ZZ9000 AmiSSL Provider

This document describes how the ZZ9000 crypto offload plugs into AmiSSL 5.x
(OpenSSL 3.x) as an OpenSSL 3 *provider*, how to build an application that uses
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
    ├── zz9000 provider                 └── default provider (software)
    │     KEYMGMT  X25519 (full),               everything not offloaded,
    │              EC, RSA                       reached directly OR via the
    │     KEYEXCH  X25519, ECDH (P-256)          zz9000 keymgmt's delegation
    │     SIGNATURE ECDSA, RSA (verify)          (non-P256 curves, RSA-PSS,
    │     CIPHER   AES-128/256-GCM,              client-cert signing, app
    │              ChaCha20-Poly1305             EC/RSA keygen)
    │       │  │
    │       │  └─ delegate ─► default provider (forced provider=default)
    │       ▼  zz9k_offload.c
    │     ZZ9000 SDK  (zz9k_crypto_kx / _aead, shared buffers, mailbox)
    │       │
    │       ▼
    └─────► ZZ9000 firmware (ARM Cortex-A9)
```

A provider that sets the default property query to `?provider=zz9000` is
*preferred* for every algorithm it advertises, so libssl fetches **our**
implementation for the whole of any key type we name — not just the leaf
operation we accelerate. Two kinds of algorithm are advertised, and they are
owned differently:

* **Owned end-to-end.** **X25519** (key generation, key exchange, and all the
  parameter round-trips libssl performs) and the **AEAD record ciphers**
  (AES-GCM and ChaCha20-Poly1305, including the TLS 1.2 record-layer controls)
  are fully implemented here — nothing about them is delegated.
* **Accelerate-part, delegate-the-rest.** The provider also owns the **EC** and
  **RSA** key types so it can accelerate the handshake operations the firmware
  serves — **P-256 ECDHE** (key generation + ECDH derive) and **P-256 ECDSA /
  RSA-2048 PKCS#1-v1.5-SHA-256 certificate verification**. Owning a whole key
  type means *every* EC/RSA operation routes here, including ones the board has
  no primitive for. Those are **delegated**: the keymgmt builds (or holds) a
  "shadow" `EVP_PKEY` against the default provider and forwards the operation to
  it. Delegated operations are non-P256 curves, RSA-PSS (the TLS 1.3 RSA
  signature scheme) and other digests/sizes, **certificate/CertificateVerify
  signing** with a client key, and application-level **EC/RSA key generation**.
  This makes the EC/RSA keymgmt a transparent *accelerating shim*: the hot
  handshake paths run on the board, everything else behaves exactly as the
  default provider would — so owning the key type never regresses an operation.

The delegated fetch always forces `provider=default` (not the inherited
`?provider=zz9000`): otherwise EVP would prefer *our* op for the shadow key too
and bridge it straight back into this provider — unbounded recursion. Pinning
both the operation and the key to the default provider is what makes delegation
safe.

The provider operation files are portable OpenSSL 3 code and are shared with the
host unit tests. Two things make them use the hardware:

* **`-DZZ9K_PROVIDER_OFFLOAD`** at compile time enables the offload hooks. With
  it set, each accelerated operation (`zz9k_prov_x25519`, `zz9k_prov_aead`) is
  **offload-or-fail**: it runs through `zz9k_offload_*` and does *not* fall back
  to the bundled software reference (an in-library software ChaCha tag
  miscomputes under the base-relative library link, and silent wrong crypto is
  worse than a failed record). The provider only *advertises* operations the
  board can serve, so one that would fail is never offered — it resolves to
  AmiSSL's software default instead (see *Fallback semantics*). With the macro
  undefined (the host build) every operation uses the software reference, which
  is why the host unit tests stay meaningful. (`zz9k_prov_ecdsa_verify` /
  `zz9k_prov_rsa_verify` share the same offload-or-fail shape for the
  *accelerated* verify; the non-accelerated EC/RSA operations do not touch the
  board at all — they delegate to AmiSSL's default provider — so they are
  unaffected by this macro.)
* **An open offload context.** `zz9k_provider_init` opens the board once for
  the provider's lifetime (`zz9k_offload_open`), keeps it only when the
  firmware's crypto service responds, and records the advertised service
  flags. Each operation additionally gates its offload on the matching
  `ZZ9K_SERVICE_FLAG_CRYPTO_*` bit, so firmware that lacks one algorithm never
  pays a failing mailbox round trip for it. If the board (or the crypto
  service) is absent the provider still loads and every operation transparently
  uses software. The context also holds persistent shared scratch buffers:
  allocation is a full mailbox round trip, so a warm operation costs exactly
  one round trip — the figure the published benchmarks measured.

## Source files

| File | Role |
| --- | --- |
| `amiga/provider/zz9k_provider.c` | Provider core: params, operation query, init/teardown (opens the SDK context under `ZZ9K_PROVIDER_OFFLOAD`). |
| `amiga/provider/zz9k_algorithms.c` | Central `OSSL_ALGORITHM` tables. |
| `amiga/provider/zz9k_x25519.c` | X25519 KEYMGMT (keygen, import/export, params) + KEYEXCH. The full surface libssl needs for a TLS key share. |
| `amiga/provider/zz9k_aead.c` | AES-128/256-GCM and ChaCha20-Poly1305 ciphers, including the TLS 1.2 record-layer controls (`EVP_CTRL_AEAD_TLS1_AAD` / `SET_IV_FIXED`, and ChaCha's init-delivered fixed IV) libssl drives per record. |
| `amiga/provider/zz9k_ecdsa.c` | EC KEYMGMT + ECDH (P-256) KEYEXCH + ECDSA SIGNATURE. Accelerates P-256 ECDHE keygen/derive and P-256 ECDSA verify; delegates non-P256 curves, ECDSA signing, and non-P256 keygen to a shadow default-provider key. |
| `amiga/provider/zz9k_rsa.c` | RSA KEYMGMT + RSA SIGNATURE. Accelerates RSA-2048 PKCS#1-v1.5-SHA-256 verify; delegates RSA-PSS, other digests/sizes, RSA signing, and RSA keygen to a shadow default-provider key. |
| `amiga/provider/zz9k_offload.c` | SDK bridge: marshals an operation into shared buffers and runs it through the SDK. **Amiga-only.** |
| `amiga/provider/zz9k_amissl.c` | `zz9k_amissl_register()` / `zz9k_amissl_unregister()` — registers the provider with AmiSSL's default library context. **Amiga-only.** |
| `amiga/provider/zz9k_amissl_selftest.c` | Standalone hardware self-test program. **Amiga-only.** |
| `tools/zz9k-crypto-soft.c` | Portable software reference (firmware-validation oracle + host parity tests; the host build runs it, the offload build does not). |

## Building an application

Compile the provider objects together with your program and define
`ZZ9K_PROVIDER_OFFLOAD`. The build splits into two groups:

* **OpenSSL-touching objects** (the provider operation files plus your
  application) must be forced to include `<proto/amissl.h>` so every OpenSSL
  call is redirected to the AmiSSL library. The simplest way is `-include
  proto/amissl.h` on the command line, which avoids editing the portable
  provider sources. AmiSSL must be opened with `AmiSSL_UsesOpenSSLStructs = TRUE`
  at run time because the provider allocates non-opaque OpenSSL structures
  directly (see registration below). No `-lcrypto`/`-lssl` is needed — the
  redirected calls inline to `jsr` through the library base.
* **Pure-SDK objects** (`zz9k_offload.c`, the SDK host implementation
  `host/src/zz9k_host.c`, and the software reference `tools/zz9k-crypto-soft.c`)
  use no OpenSSL and compile without the force-include.

The `sacredbanana/amiga-compiler:m68k-amigaos` image already ships the AmiSSL
SDK (headers and `libamisslauto.a`), so no separate `-I$AMISSL` is required.
From the SDK root:

```sh
CC=m68k-amigaos-gcc
INC="-Iinclude -Ihost/include -Itools -Iamiga/provider"
CF="-noixemul -O2 -DZZ9K_PROVIDER_OFFLOAD"

# Provider + application (OpenSSL): force-include the AmiSSL proto.
for f in zz9k_provider zz9k_algorithms zz9k_x25519 zz9k_aead zz9k_ecdsa \
         zz9k_rsa zz9k_amissl zz9k_amissl_selftest; do
    $CC $CF -include proto/amissl.h $INC -c amiga/provider/$f.c -o $f.o
done

# Pure-SDK objects: no OpenSSL, no force-include.
$CC $CF $INC -c amiga/provider/zz9k_offload.c -o zz9k_offload.o
$CC $CF $INC -c host/src/zz9k_host.c           -o zz9k_host.o
$CC $CF $INC -c tools/zz9k-crypto-soft.c       -o zz9k_crypto_soft.o

# Link (manual AmiSSL model — the bases are opened by the program).
$CC -noixemul -o zz9k_selftest *.o
```

This is exactly how the bundled self-test (`zz9k_amissl_selftest.c`) is built;
running the block above produces a `loadseg()`able AmigaOS executable. For your
own application, swap `zz9k_amissl_selftest.c` for your sources and link them in
the same way.

## Registering the provider

Call `zz9k_amissl_register()` once, **after** AmiSSL is open and initialised and
**before** any crypto. It registers the built-in provider, loads it alongside
the default provider (kept for fallback), and sets the default property query to
`?provider=zz9000` so ordinary EVP calls prefer the hardware.

Open AmiSSL with `AmiSSL_UsesOpenSSLStructs = TRUE` and fetch **both** the main
(`AmiSSLBase`) and extension (`AmiSSLExtBase`, which carries the
`OSSL_PROVIDER_*` API) library bases. The `OpenAmiSSLTags()` call does this in
one step (m68k shown):

```c
#include <proto/amisslmaster.h>
#include "zz9k_amissl.h"

struct Library *AmiSSLBase, *AmiSSLExtBase;   /* externed by proto/amissl.h */

OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
               AmiSSL_UsesOpenSSLStructs, TRUE,
               AmiSSL_GetAmiSSLBase,    (ULONG)&AmiSSLBase,
               AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
               AmiSSL_SocketBase,       (ULONG)SocketBase,
               AmiSSL_ErrNoPtr,         (ULONG)&errno,
               TAG_DONE);

if (!zz9k_amissl_register()) {
    /* registration failed — nothing is left registered; you can still run
       with AmiSSL's software crypto. */
}

/* ... use AmiSSL normally; offloadable operations now go to the ZZ9000 ... */

zz9k_amissl_unregister();   /* before CloseAmiSSL() */
CloseAmiSSL();              /* also calls CleanupAmiSSL() */
```

No other application change is required: existing `EVP_*`, `SSL_*` and
`TLS` code transparently picks up the provider through the default property
query. `zz9k_amissl_selftest.c` is a complete worked example of this sequence.

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

The host test `tests/provider_tls_handshake_test.c` is the authoritative proof
that the provider is a working drop-in: it runs real TLS 1.3 and TLS 1.2
handshakes (client through the zz9000 provider with `?provider=zz9000`, server
through the default provider, over memory BIOs) across X25519 and P-256 key
exchange, ECDSA-P256 and RSA certificates, and AES-GCM and ChaCha20-Poly1305
records, then exchanges application data. It builds whenever an OpenSSL 3
install with libssl is found.

On hardware, adapt the AmiSSL `test/https.c` example: add the provider objects
to its build, define `ZZ9K_PROVIDER_OFFLOAD`, and call `zz9k_amissl_register()`
right after the existing `InitAmiSSL()` (the in-library Path A build needs no
such call). A successful HTTPS GET against a server negotiating `X25519` or
`P-256` key exchange and an ECDSA-P256 or RSA-2048 certificate confirms the
offloaded key exchange, certificate verification, and record crypto together
produced a valid handshake. To confirm the certificate verify specifically ran
on the board, fetch the signature after a handshake and read its provider
(`EVP_SIGNATURE_get0_provider` → `"zz9000"` for a P-256 ECDSA or RSA-2048 chain;
a P-384/P-521 or RSA-PSS-only chain reads `"default"`, i.e. delegated).

## Fallback semantics

The provider is conservative about what it *advertises*: it only claims
operations it can fully offload, so everything else — and everything on a
machine without the board — transparently uses AmiSSL's software.

* The provider advertises only what the board actually serves, each gated on its
  own firmware capability flag: X25519 (key exchange + keymgmt); the AEAD ciphers
  the firmware supports (AES-GCM gated on its flag, otherwise ChaCha20-Poly1305
  only); the **EC** key type + ECDH KEYEXCH + ECDSA SIGNATURE (gated on the
  P-256 keygen / ECDSA-P256 flags); and the **RSA** key type + SIGNATURE (gated
  on the RSA-2048 flag). With no board or no crypto service it advertises
  **nothing**, and the library behaves exactly like stock AmiSSL. Because each
  key type is gated independently, firmware that predates the P-256-keygen or
  cert-verify flags simply leaves EC/RSA to the default provider, unchanged.
* A key type is advertised only when the board can accelerate *some* operation on
  it; the operations the board cannot do (non-P256 curves, RSA-PSS, signing, app
  keygen) are still served — the keymgmt **delegates** them to the default
  provider (forcing `provider=default`). So owning EC/RSA accelerates the
  handshake without removing any operation the default provider offered.
* Advertised operations are **offload-or-fail**: the in-library software crypto
  paths are deliberately not used (they are compiled only into the host build,
  for the parity tests), so a record either offloads to the board or fails —
  silent wrong crypto is worse than a failed record. A decrypt whose tag does
  not authenticate is correctly rejected.
* `ENV:ZZ9K_DISABLE_OFFLOAD` forces the no-board path at registration: the
  provider loads and is preferred but never opens the board, so every operation
  runs in AmiSSL software. A zero-rebuild A/B switch for isolating a regression
  to — or away from — the offload.
* `zz9k_amissl_unregister()` is a no-op when registration never ran, so an
  application's cleanup path is safe even after a failed `InitAmiSSL()`.
