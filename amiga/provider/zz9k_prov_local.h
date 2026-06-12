/*
 * ZZ9000 OpenSSL provider — internal definitions shared across the provider
 * translation units (provider core + per-operation implementations).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef ZZ9K_PROV_LOCAL_H
#define ZZ9K_PROV_LOCAL_H

#include <openssl/core.h>
#include <openssl/core_dispatch.h>

#include "zz9k/abi.h"   /* ZZ9K_SERVICE_FLAG_CRYPTO_* for the offload gates */

#ifdef __cplusplus
extern "C" {
#endif

/* Provider-side context. On the Amiga `sdk_ctx` holds an open ZZ9000 SDK
 * context (ZZ9KContext *) — but only when the firmware's crypto service
 * responded at provider init — and `service_flags` the crypto service flags it
 * advertised. On the host both stay NULL/0 and everything uses the software
 * reference. */
typedef struct zz9k_prov_ctx_st {
  const OSSL_CORE_HANDLE *handle;
  void *sdk_ctx;            /* ZZ9KContext * when running on hardware, else NULL */
  unsigned int service_flags;
} ZZ9K_PROV_CTX;

/* Offload gates used by the operation implementations. Every algorithm must
 * check its own ZZ9K_SERVICE_FLAG_CRYPTO_* bit: firmware that implements the
 * crypto service but not a given algorithm either rejects the call (a wasted
 * mailbox round trip per operation) or, for AEAD, silently runs the legacy
 * default algorithm instead (see the AEAD flags-nibble note in zz9k/abi.h).
 * ChaCha20-Poly1305 is that legacy default, available whenever the crypto
 * service itself is, so it gates on the service alone. */
#define ZZ9K_PROV_CAN_OFFLOAD_SERVICE(p) \
  ((p) != NULL && (p)->sdk_ctx != NULL)
#define ZZ9K_PROV_CAN_OFFLOAD(p, flag) \
  (ZZ9K_PROV_CAN_OFFLOAD_SERVICE(p) && ((p)->service_flags & (flag)) != 0U)

/* X25519 scalar multiplication used by the key-exchange derive. Routes to the
 * ZZ9000 offload when `provctx` carries a live SDK context with the X25519
 * service flag, otherwise the portable software reference. Returns 1 on
 * success, 0 on failure (e.g. all-zero / small-order result). */
int zz9k_prov_x25519(unsigned char out[32], const unsigned char scalar[32],
                     const unsigned char point[32], ZZ9K_PROV_CTX *provctx);

/* Per-key-type KEYMGMT and per-algorithm SIGNATURE dispatch tables, combined
 * into the keymgmt/signature OSSL_ALGORITHM tables in zz9k_algorithms.c. */
extern const OSSL_DISPATCH zz9k_x25519_keymgmt_functions[];
extern const OSSL_DISPATCH zz9k_ec_keymgmt_functions[];
extern const OSSL_DISPATCH zz9k_rsa_keymgmt_functions[];
extern const OSSL_DISPATCH zz9k_ecdsa_signature_functions[];
extern const OSSL_DISPATCH zz9k_rsa_signature_functions[];

/* Algorithm tables advertised by the provider's query-operation callback. */
extern const OSSL_ALGORITHM zz9k_keymgmt_algorithms[];
extern const OSSL_ALGORITHM zz9k_keyexch_algorithms[];
extern const OSSL_ALGORITHM zz9k_cipher_algorithms[];
extern const OSSL_ALGORITHM zz9k_signature_algorithms[];

#ifdef __cplusplus
}
#endif

#endif /* ZZ9K_PROV_LOCAL_H */
