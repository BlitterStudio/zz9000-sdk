# ZZ9000 Crypto / TLS Acceleration

Copyright (C) 2024-2026, Dimitris Panokostas / BlitterStudio

This is the working analysis for accelerating TLS on classic Amiga through the
ZZ9000 ARM coprocessor. It records the economic reasoning behind the
**handshake-first** roadmap and the Phase 0 benchmark plan that will replace the
estimates below with hardware measurements.

> Status: Phases 0-2 complete and measured on hardware (SDK ABI 2.0). The
> symmetric record sweep, mailbox round trip, and the asymmetric handshake
> primitives (X25519, P-256 ECDH, ECDSA-P256 verify, RSA-2048 verify) are all
> measured; see Measured results. The firmware crypto service (BearSSL on the
> ARM) advertises these via capability flags. Phase 3 (AES-GCM) and Phase 4
> (OpenSSL provider) remain.

## Why offload, and where it pays

A ZZ9000 crypto offload replaces an m68k software operation with: submit a
request over the mailbox, the ARM computes, read the result back. Whether that
is a net win depends entirely on the ratio of *compute* to *I/O*:

- **Record-phase symmetric crypto** (ChaCha20-Poly1305, AES-GCM) is
  throughput-bound and per-byte. TLS records are small (<=16 KB, often far
  less). The mailbox round trip is a fixed cost paid per record; for small
  records it can dominate, so offload may be *slower* than m68k software. This
  is the uncertain case, and the one `zz9k-cryptobench` measures directly.
- **Handshake-phase asymmetric crypto** (X25519, P-256 ECDHE/ECDSA, RSA) is
  latency-bound and per-connection: tiny input, large compute. The mailbox
  round trip is negligible next to the computation, so offload essentially
  always wins, and wins big. This is also where classic Amiga hurts most — a
  68k doing an elliptic-curve scalar multiply or an RSA operation can stall a
  connection for hundreds of milliseconds.

The model is captured in `tools/zz9k-handshake-model.h`:
`offload_micros = arm_micros + mailbox_micros`, and offload is only counted as
a win when it is strictly faster than the m68k software cost — the handshake
analog of the symmetric small-record break-even.

## Current SDK crypto surface

Available today (firmware crypto service, `ZZ9K_SERVICE_CRYPTO`):

- Hash / HMAC: SHA-1, SHA-256, SHA-384, SHA-512, BLAKE2S
- MAC: Poly1305
- Stream: ChaCha20
- AEAD: ChaCha20-Poly1305

Missing for a complete TLS story (see roadmap below): AES-GCM (record), and the
asymmetric handshake primitives X25519, P-256 ECDHE/ECDSA, and RSA verify.

## Economics (handshake rows estimated, mailbox + record rows MEASURED)

Targets: m68k is a ~25-50 MHz 68030/68040; ARM is the ZZ9000 Zynq-7000
Cortex-A9 (ARMv7, no AES/crypto extensions). The mailbox round trip is now
**measured at ~6.1 ms** for a synchronous call (see Measured results below),
which replaces the earlier ~120 us placeholder. The asymmetric m68k/ARM costs
remain order-of-magnitude estimates for sequencing the roadmap until a firmware
asymmetric service exists to benchmark.

| Operation | Phase | m68k SW | ARM + mbox | Speedup | Per-op saving |
|---|---|---|---|---|---|
| X25519 scalar mult | handshake | 790.75 ms (meas.) | 6.36 ms (meas.) | 124x | 784 ms |
| P-256 ECDH | handshake | 9593 ms (meas.) | 12.25 ms (meas.) | 783x | 9581 ms |
| ECDSA-P256 verify | handshake | 19123 ms (meas.) | 18.78 ms (meas.) | 1018x | 19104 ms |
| RSA-2048 verify (e=65537) | handshake | 1379 ms (meas.) | 11.29 ms (meas.) | 122x | 1368 ms |
| RSA-2048 sign (CRT) | handshake | ~1500 ms (est.) | ~10 ms + 6.1 ms | ~93x | ~1484 ms |
| ChaCha20-Poly1305 16 KB record, sync | record | 162 KiB/s | 2539 KiB/s | 15.7x | measured |
| ChaCha20-Poly1305 16 KB record, pipelined | record | 162 KiB/s | ~9539 KiB/s | ~59x | measured |
| AES-128-GCM 16 KB record, sync | record | 10 KiB/s | 2487 KiB/s | 249x | measured |
| AES-128-GCM 16 KB record, batched (16) | record | 10 KiB/s | 3584 KiB/s | 358x | measured |

Reading: even with the measured ~6-19 ms offload round trip, the asymmetric
handshake operations save **hundreds of milliseconds to tens of seconds** *per
connection* at 122-1018x (measured). They are single ops on the connection's
critical path (not batchable), so each pays one round trip — negligible against
their compute. They are the clearest risk-free wins, which is why the roadmap is
**handshake-first**.

## Measured results (hardware, SDK ABI 2.0, EClock 709 kHz)

`zz9k-cryptobench` (ChaCha20-Poly1305, software m68k vs synchronous ZZ9000
offload, 16 iterations/size):

| Record | soft KiB/s | offload KiB/s | per-record offload |
|---|---|---|---|
| 64 B | 96 | 9 | 6.94 ms |
| 256 B | 146 | 39 | 6.41 ms |
| 1 KB | 161 | 158 | 6.33 ms |
| 4 KB | 158 | 628 | 6.37 ms |
| 16 KB | 162 | 2539 | 6.30 ms |

Break-even: synchronous offload overtakes software at **2 KB**.

The headline finding: **synchronous offload is mailbox-latency-bound, not
compute-bound.** Per-record offload time is ~6.3 ms across every size from 64 B
to 16 KB, matching the measured synchronous ping latency exactly
(`zz9k-bench`: `SDK ping` 161 calls/s = 6.1 ms/call). The ARM's actual crypto
work stays hidden beneath that fixed latency even at 16 KB. Pipelining collapses
it: `SDK ping pipe` reaches 11193 calls/s (~70x), and the batched AEAD
(`zz9k-bench` `ARM ChaCha20-Poly pipe`) hits ~9539 KiB/s at 16 KB versus the
synchronous sweep's 2539 KiB/s.

Design implication for the provider: **batch records whenever possible.** A bulk
`SSL_write` is split by the TLS layer into multiple ~16 KB records that can be
submitted together via the existing batch AEAD path, turning the latency-bound
case into the throughput-bound one. Small interactive records stay on the sync
path, but they are network-latency-dominated and crypto cost is irrelevant
there. Software m68k ChaCha20-Poly1305 plateaus at ~160 KiB/s, so batched
offload is ~59x faster for bulk transfer.

### X25519 key exchange (Phase 1, measured)

`zz9k-cryptobench` X25519 section (16 iterations, software m68k vs synchronous
ZZ9000 offload):

| | per-op | speedup |
|---|---|---|
| Software (m68k) | 790.75 ms/op | — |
| Offload (ZZ9000) | 6.36 ms/op | **124x** |

This confirms the handshake-first thesis directly: X25519 is the single most
expensive handshake primitive on m68k (~0.8 s of pure compute), and the offload
collapses it to one ~6.4 ms mailbox round trip — the ARM's scalar multiplication
disappears beneath the same fixed latency seen in the symmetric sweep. Unlike
records, this op is on the connection's critical path and pays exactly one round
trip, so the full 124x is realised per connection. The earlier ~500 ms estimate
under-counted the m68k cost; the measured saving is **784 ms per handshake**.

### P-256 / ECDSA / RSA verify (Phase 2, measured)

`zz9k-cryptobench` asymmetric sections (16 iterations, software m68k vs
synchronous ZZ9000 offload), firmware crypto service with BearSSL on the ARM:

| Primitive | Software (m68k) | Offload (ZZ9000) | Speedup |
|---|---|---|---|
| P-256 ECDH | 9593 ms/op | 12.25 ms/op | **783x** |
| ECDSA-P256 verify | 19123 ms/op | 18.78 ms/op | **1018x** |
| RSA-2048 verify (e=65537) | 1379 ms/op | 11.29 ms/op | **122x** |

The offload column collapses to ~11-19 ms — one mailbox round trip plus the
ARM's compute, which stays small. The headline is ECDSA-P256 verify at **1018x**:
the m68k software path is *19 seconds* per verify, offloaded to 19 ms. P-256 ECDH
is 9.6 s → 12 ms. These dwarf even X25519 and make the asymmetric handshake the
single highest-value offload target, exactly as the handshake-first thesis
predicted.

The software (m68k) column is the SDK's **portable correctness-first reference**
(the `zz9k-soft` fallback), not a hand-optimised m68k crypto library, so these
speedups are the wins a client of *this* SDK sees on its own fallback path. They
also re-baseline the earlier estimates, which badly under-counted m68k bignum and
EC cost (P-256 estimated ~700 ms, measured 9593 ms). The conclusion is unchanged
and stronger: asymmetric offload is a risk-free win of **1.4-19 s per handshake
operation**.

### AES-128-GCM record cipher (Phase 3, measured)

`zz9k-cryptobench` AES-128-GCM section (16 iterations/size; software m68k vs
synchronous offload vs batched offload at depth 16):

| Record | soft KiB/s | sync KiB/s | batched(16) KiB/s |
|---|---|---|---|
| 64 B | 7 | 9 | 619 |
| 256 B | 9 | 39 | 510 |
| 1 KB | 10 | 157 | 1989 |
| 4 KB | 10 | 635 | 3132 |
| 16 KB | 10 | 2487 | 3584 |

Two findings, both stronger than the ChaCha20-Poly1305 case:

1. **Software AES-GCM on m68k is unusably slow — ~10 KiB/s**, about 16x slower
   than software ChaCha20-Poly1305 (162 KiB/s). The 68k has no AES instructions,
   and a constant-time byte-oriented AES plus GHASH is expensive. Consequently
   even the *synchronous* offload beats software at **every** record size (9 vs 7
   KiB/s already at 64 B) — there is no small-record regime where software wins,
   unlike ChaCha (which had a ~1-2 KB break-even). For AES-GCM the offload is
   always the right choice.

2. **Batching is the throughput win.** The synchronous path is mailbox-latency
   bound (the familiar ~6 ms/record curve: 9 KiB/s at 64 B rising to 2487 KiB/s
   at 16 KB). Batching 16 records per submission collapses that latency: **619
   KiB/s at 64 B (≈68x the synchronous path) and 3584 KiB/s at 16 KB.** The small
   end gains the most, because that is where per-record latency dominates; at
   16 KB the ARM's compute starts to dominate so the batch margin narrows to
   ~1.4x. This is exactly the curve the provider's batching design (Phase 4)
   needs: accumulate TLS records and submit them via `zz9k_crypto_aead_batch`.

## Data-collection plan

1. **Symmetric record sweep — `zz9k-cryptobench`** — DONE (see Measured
   results). Break-even at 2 KB; synchronous offload is mailbox-latency-bound at
   ~6.3 ms/record.

2. **Mailbox round-trip baseline — `zz9k-bench`** — DONE. Synchronous ping is
   6.1 ms/call (161 calls/s); pipelined 11193 calls/s. This 6.1 ms is now the
   mailbox figure in the model.

3. **Batched-record sweep — DONE (Phase 3).** The AES-128-GCM section measures
   software, synchronous, and batched (`zz9k_crypto_aead_batch`, depth 16) paths.
   Batching reaches 3584 KiB/s at 16 KB and beats the synchronous path ~68x at
   64 B, grounding the provider's batching design in a real curve.

4. **Asymmetric micro-benchmark — DONE.** All four `zz9k-cryptobench`
   handshake sections are measured on hardware: X25519 (790.75 ms vs 6.36 ms,
   124x), P-256 ECDH (9593 ms vs 12.25 ms, 783x), ECDSA-P256 verify (19123 ms vs
   18.78 ms, 1018x), and RSA-2048 verify (1379 ms vs 11.29 ms, 122x). The
   estimate rows have been replaced with these figures.

## Roadmap (informed by this analysis)

- **Phase 0 — Benchmark gate. DONE.** Software baseline + record sweep + this
  economic model, all measured on hardware.
- **Phase 1 — X25519. DONE (measured, 124x).** Firmware ARM service + ABI + SDK
  wrapper. The handshake key-exchange win.
- **Phase 2 — P-256 ECDH/ECDSA + RSA verify. DONE (measured, 122-1018x).**
  Broader handshake coverage, BearSSL on the ARM.
- **Phase 3 — AES-GCM. DONE (measured).** ARM software service (BearSSL
  aes_ct/gcm) on the existing AEAD op. Software m68k AES-GCM is ~10 KiB/s, so
  offload wins at every record size; batched offload reaches 3584 KiB/s at 16 KB.
  Closes the dominant-ciphersuite record gap.
- **Phase 4 — OpenSSL 3.x provider.** Wire the primitives into AmiSSL via a
  built-in, no-DSO provider (`OSSL_PROVIDER_add_builtin`), routing
  `EVP_CIPHER` / `EVP_MD` / key-exchange to the ZZ9000 service. AmiSSL 5.x
  (OpenSSL 3.6) exports the full provider-registration API through
  `amisslext_lib.fd`, so this path is confirmed feasible.
- **Phase 5 (optional, far) — FPGA AES core**, only if software AES-GCM on the
  ARMv7 A9 proves throughput-limited.

## Files

- `tools/zz9k-crypto-soft.{h,c}` — portable software ChaCha20-Poly1305 (m68k
  baseline + verification oracle), RFC 8439 KATs in `tests/crypto_soft_test.c`.
- `tools/zz9k-cryptobench.c` — record-size break-even benchmark, helper logic in
  `tests/cryptobench_logic_test.c`.
- `tools/zz9k-handshake-model.h` — handshake offload calculator, checks in
  `tests/handshake_model_test.c`.
