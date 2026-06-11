# ZZ9000 Crypto / TLS Acceleration

Copyright (C) 2024-2026, Dimitris Panokostas / BlitterStudio

This is the working analysis for accelerating TLS on classic Amiga through the
ZZ9000 ARM coprocessor. It records the economic reasoning behind the
**handshake-first** roadmap and the Phase 0 benchmark plan that will replace the
estimates below with hardware measurements.

> Status: Phase 0 complete. Symmetric record sweep and mailbox round trip are
> now measured on hardware (SDK ABI 2.0); see Measured results. The asymmetric
> handshake costs remain first-order estimates, explicitly labelled, until a
> firmware asymmetric service exists to benchmark. No firmware crypto additions
> have been made yet.

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
| P-256 ECDHE/ECDSA-verify | handshake | ~700 ms (est.) | ~5 ms + 6.1 ms | ~63x | ~689 ms |
| RSA-2048 verify (e=65537) | handshake | ~50 ms (est.) | ~0.6 ms + 6.1 ms | ~7.5x | ~43 ms |
| RSA-2048 sign (CRT) | handshake | ~1500 ms (est.) | ~10 ms + 6.1 ms | ~93x | ~1484 ms |
| ChaCha20-Poly1305 16 KB record, sync | record | 162 KiB/s | 2539 KiB/s | 15.7x | measured |
| ChaCha20-Poly1305 16 KB record, pipelined | record | 162 KiB/s | ~9539 KiB/s | ~59x | measured |

Reading: even with the measured ~6.1 ms round trip, the asymmetric handshake
operations still save tens to hundreds of milliseconds *per connection* at
7-90x. They are single ops on the connection's critical path (not batchable),
so each pays one round trip — negligible against their compute. They remain
risk-free wins, which is why the roadmap is **handshake-first**.

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

## Data-collection plan

1. **Symmetric record sweep — `zz9k-cryptobench`** — DONE (see Measured
   results). Break-even at 2 KB; synchronous offload is mailbox-latency-bound at
   ~6.3 ms/record.

2. **Mailbox round-trip baseline — `zz9k-bench`** — DONE. Synchronous ping is
   6.1 ms/call (161 calls/s); pipelined 11193 calls/s. This 6.1 ms is now the
   mailbox figure in the model.

3. **Batched-record sweep — follow-up.** `zz9k-cryptobench` currently measures
   only the synchronous offload path. A batched variant (via
   `zz9k_crypto_aead_batch`) would quantify the throughput-bound case directly
   rather than inferring it from the `zz9k-bench` pipe figures. Worth adding
   before the provider work so the provider's batching design is grounded in a
   real curve.

4. **Asymmetric micro-benchmark — X25519 DONE, P-256/RSA pending.** The
   `zz9k-cryptobench` X25519 section is measured (790.75 ms m68k vs 6.36 ms
   offload, 124x). P-256 ECDHE/ECDSA-verify and RSA-2048-verify sections follow
   in Phase 2 and will replace their estimate rows once run on hardware. The
   model in `zz9k-handshake-model.h` still lets us plug in any measured pair and
   read the verdict.

## Roadmap (informed by this analysis)

- **Phase 0 — Benchmark gate (current).** Software baseline + record sweep +
  this economic model. SDK-only, host-tested, ready to drop onto hardware.
- **Phase 1 — X25519** (firmware ARM service + ABI + SDK wrapper). The
  handshake key-exchange win.
- **Phase 2 — P-256 ECDHE/ECDSA + RSA verify.** Broader handshake coverage.
- **Phase 3 — AES-GCM** (ARM software service). Closes the dominant-ciphersuite
  record gap so the provider accelerates the common case; sequenced after the
  `zz9k-cryptobench` result confirms the record-offload economics.
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
