/*
 * ZZ9000 OpenSSL provider — combined KEYMGMT algorithm table.
 *
 * KEYMGMT spans several key types (X25519, EC/P-256, and later RSA), each
 * implemented in its own translation unit, so the table that the provider's
 * query-operation callback hands back for OSSL_OP_KEYMGMT is assembled here.
 * The keyexch, cipher and signature tables live with their single
 * implementations.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "zz9k_prov_local.h"

/* The provider only PREFER-shadows (via "?provider=zz9000") algorithms it can
 * own end-to-end in a TLS handshake. For X25519 that is the whole surface
 * (keygen, key exchange, param round-trips), so it is advertised. EC and RSA
 * are deliberately NOT advertised here even though the SDK accelerates ECDSA
 * and RSA *verify*: shadowing those key types would force libssl to fetch our
 * keymgmt/signature for operations we do not implement (ECDH keygen, signing,
 * RSA-PSS) and would drop their TLS groups, breaking the handshake. Their
 * verify stays in AmiSSL's software default provider. Accelerating cert
 * verify through the provider needs a full delegating keymgmt/signature (so
 * keygen/sign/PSS pass through to the default provider) — tracked as future
 * work; the zz9k_ecdsa.c / zz9k_rsa.c implementations remain host-tested for
 * it. The single per-handshake cert verify is a far smaller cost than the key
 * exchange and bulk record crypto, which the provider does accelerate. */
const OSSL_ALGORITHM zz9k_keymgmt_algorithms[] = {
  { "X25519", "provider=zz9000", zz9k_x25519_keymgmt_functions,
    "ZZ9000 X25519 key management" },
#ifdef ZZ9K_PROVIDER_TEST_ALL
  /* EC/RSA keymgmt are exposed only to the host cross-provider verify tests
   * (provider_ecdsa_test / provider_rsa_test). Shipping them in production
   * would force libssl to fetch our incomplete keymgmt for ECDH keygen and
   * signing — see the comment above and the deferred delegating-keymgmt
   * work. */
  { "EC", "provider=zz9000", zz9k_ec_keymgmt_functions,
    "ZZ9000 EC (P-256) key management" },
  { "RSA", "provider=zz9000", zz9k_rsa_keymgmt_functions,
    "ZZ9000 RSA key management" },
#endif
  { NULL, NULL, NULL, NULL }
};

const OSSL_ALGORITHM zz9k_signature_algorithms[] = {
#ifdef ZZ9K_PROVIDER_TEST_ALL
  { "ECDSA", "provider=zz9000", zz9k_ecdsa_signature_functions,
    "ZZ9000 ECDSA verify" },
  { "RSA", "provider=zz9000", zz9k_rsa_signature_functions,
    "ZZ9000 RSA PKCS#1 v1.5 verify" },
#endif
  { NULL, NULL, NULL, NULL }
};
