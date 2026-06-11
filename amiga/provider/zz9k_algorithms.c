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

const OSSL_ALGORITHM zz9k_keymgmt_algorithms[] = {
  { "X25519", "provider=zz9000", zz9k_x25519_keymgmt_functions,
    "ZZ9000 X25519 key management" },
  { "EC", "provider=zz9000", zz9k_ec_keymgmt_functions,
    "ZZ9000 EC (P-256) key management" },
  { NULL, NULL, NULL, NULL }
};
