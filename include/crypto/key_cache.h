#ifndef CIFRASYNC_CRYPTO_KEY_CACHE_H
#define CIFRASYNC_CRYPTO_KEY_CACHE_H

#include <stddef.h>

#include "crypto/kdf.h"

typedef struct cs_key_cache_entry {
	unsigned char salt[CS_CRYPTO_SALT_SIZE];
	unsigned char passphrase_hash[CS_CRYPTO_SHA256_SIZE];
	unsigned char key[CS_CRYPTO_SHA256_SIZE];
	unsigned int rounds;
	unsigned long long last_used_tick;
	int in_use;
} cs_key_cache_entry_t;

typedef struct cs_key_cache {
	cs_key_cache_entry_t *entries;
	size_t capacity;
	unsigned long long next_tick;
} cs_key_cache_t;

int cs_key_cache_init(cs_key_cache_t *cache, size_t capacity);
void cs_key_cache_reset(cs_key_cache_t *cache);
void cs_key_cache_free(cs_key_cache_t *cache);

int cs_key_cache_derive(cs_key_cache_t *cache,
	const char *passphrase,
	const unsigned char *salt,
	size_t salt_size,
	unsigned int rounds,
	unsigned char *out_key,
	size_t out_key_size);

#endif

