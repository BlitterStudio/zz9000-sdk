/*
 * Small public wrapper around the embedded LHa for UNIX decoder subset.
 */

#ifndef ZZ9K_LHA_UNIX_H
#define ZZ9K_LHA_UNIX_H

#include <stdint.h>
#include <stdio.h>

int zz9k_lha_unix_decode_lh5(FILE *input,
                             FILE *output,
                             uint32_t original_size,
                             uint32_t packed_size,
                             uint16_t expected_crc,
                             int check_crc,
                             uint16_t *actual_crc);
int zz9k_lha_unix_decode_method(FILE *input,
                                FILE *output,
                                uint32_t original_size,
                                uint32_t packed_size,
                                uint16_t expected_crc,
                                int check_crc,
                                uint16_t *actual_crc,
                                int method);

#endif
