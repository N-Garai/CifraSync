#ifndef CIFRASYNC_DELTA_HASH_H
#define CIFRASYNC_DELTA_HASH_H

#include <stdbool.h>
#include <stddef.h>

#include "common/constants.h"

#define CS_HASH_SHA256_BYTES 32U

int cs_hash_bytes_to_hex(const unsigned char *bytes, size_t byte_count, char *out_hex, size_t out_size);
int cs_hash_hex_to_bytes(const char *hex, unsigned char *out_bytes, size_t out_size);
bool cs_hash_is_hex(const char *hex);

int cs_hash_sha256(const void *data, size_t data_size, unsigned char out_hash[CS_HASH_SHA256_BYTES]);
int cs_hash_sha256_hex(const void *data, size_t data_size, char out_hex[CS_HASH_HEX_BUFSZ]);

#endif

