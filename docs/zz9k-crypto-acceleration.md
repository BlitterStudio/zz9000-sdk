# ZZ9000 Crypto / TLS Acceleration

Copyright (C) 2024-2026, Dimitris Panokostas / BlitterStudio

This is the working analysis for accelerating TLS on classic Amiga through the
ZZ9000 ARM coprocessor. It records the economic reasoning behind the
**handshake-first** roadmap and the Phase 0 benchmark plan that will replace the
estimates below with hardware measurements.

> Status: Phase 0 (benchmark + economic model). No firmware crypto additions
> have been made yet. The numbers in the estimate table are first-order
> estimates, explicitly labelled, pending measurement on real hardware with
> SDK-service firmware.

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

## Estimated economics (PENDING HARDWARE MEASUREMENT)

Targets: m68k is a ~25-50 MHz 68030/68040; ARM is the ZZ9000 Zynq-7000
Cortex-A9 (ARMv7, no AES/crypto extensions). Mailbox round trip is taken as a
placeholder ~120 us (to be measured with the existing ping benchmark). These
are order-of-magnitude estimates for sequencing the roadmap, **not** measured
results.

| Operation | Phase | m68k SW (est.) | ARM + mbox (est.) | Speedup (est.) | Per-op saving |
|---|---|---|---|---|---|
| X25519 scalar mult | handshake | ~500 ms | ~3 ms | ~160x | ~497 ms |
| P-256 ECDHE/ECDSA-verify | handshake | ~700 ms | ~5 ms | ~140x | ~695 ms |
| RSA-2048 verify (e=65537) | handshake | ~50 ms | ~0.6 ms | ~80x | ~49 ms |
| RSA-2048 sign (CRT) | handshake | ~1500 ms | ~10 ms | ~150x | ~1490 ms |
| ChaCha20-Poly1305 16 KB record | record | TBD (cryptobench) | TBD | TBD | TBD |

Reading: the asymmetric handshake operations save tens to hundreds of
milliseconds *per connection* at 80-160x, with offload cost dominated by a few
milliseconds of ARM compute and a negligible mailbox round trip. There is no
small-record break-even risk for these — they are risk-free wins. The symmetric
record row is deliberately left as TBD: it is the open question Phase 0
measures, because at small record sizes the round trip may erase the benefit.

This is why the roadmap is **handshake-first**: the largest, most certain wins
for TLS-on-classic-Amiga are the asymmetric primitives, not the bulk cipher.

## Data-collection plan

1. **Symmetric record sweep — `zz9k-cryptobench`** (built, packaged). Sweeps
   record sizes 64 B .. 16 KB, timing the software reference
   (`tools/zz9k-crypto-soft.c`, the m68k baseline) against the ZZ9000
   ChaCha20-Poly1305 offload, and reports the break-even record size. Run on
   hardware with SDK-service firmware:

   ```text
   zz9k-cryptobench            ; default 16 iterations/size
   zz9k-cryptobench 64         ; more iterations for stable small-size timing
   ```

   Feed the measured 16 KB software rate into the table's record row.

2. **Mailbox round-trip baseline — `zz9k-bench`**. Use the ping calls/second
   figure to replace the ~120 us mailbox placeholder in the model.

3. **Asymmetric micro-benchmark — planned**. Once a firmware asymmetric service
   exists (Phase 2+), a micro-benchmark will measure X25519 / P-256 / RSA on
   both the 68k and the ARM and replace the estimate rows above. Until then the
   model in `zz9k-handshake-model.h` lets us plug in any measured pair and read
   the verdict.

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
