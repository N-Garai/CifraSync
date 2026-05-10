#include "crypto/key_cache.h"

#include "common/memory.h"

#include <string.h>

static int cs_key_cache_match(const cs_key_cache_entry_t *entry,
	const unsigned char passphrase_hash[CS_CRYPTO_SHA256_SIZE],
	const unsigned char salt[CS_CRYPTO_SALT_SIZE],
	unsigned int rounds) {
	return entry != NULL && entry->in_use && entry->rounds == rounds && memcmp(entry->passphrase_hash, passphrase_hash, CS_CRYPTO_SHA256_SIZE) == 0 && memcmp(entry->salt, salt, CS_CRYPTO_SALT_SIZE) == 0;
}

static size_t cs_key_cache_pick_victim(const cs_key_cache_t *cache) {
	size_t victim_index = 0U;
	unsigned long long oldest_tick = 0ULL;
	for (size_t index = 0U; index < cache->capacity; ++index) {
		const cs_key_cache_entry_t *entry = &cache->entries[index];
		if (!entry->in_use) {
			return index;
		}
		if (index == 0U || entry->last_used_tick < oldest_tick) {
			oldest_tick = entry->last_used_tick;
			victim_index = index;
		}
	}
	return victim_index;
}

int cs_key_cache_init(cs_key_cache_t *cache, size_t capacity) {
	if (cache == NULL || capacity == 0U) {
		return -1;
	}

	cache->entries = (cs_key_cache_entry_t *)cs_calloc(capacity, sizeof(cs_key_cache_entry_t));
	if (cache->entries == NULL) {
		cache->capacity = 0U;
		cache->next_tick = 0ULL;
		return -1;
	}

	cache->capacity = capacity;
	cache->next_tick = 1ULL;
	return 0;
}

void cs_key_cache_reset(cs_key_cache_t *cache) {
	if (cache == NULL || cache->entries == NULL) {
		return;
	}

	for (size_t index = 0U; index < cache->capacity; ++index) {
		cs_memzero(&cache->entries[index], sizeof(cache->entries[index]));
	}
	cache->next_tick = 1ULL;
}

void cs_key_cache_free(cs_key_cache_t *cache) {
	if (cache == NULL) {
		return;
	}
	if (cache->entries != NULL) {
		for (size_t index = 0U; index < cache->capacity; ++index) {
			cs_memzero(&cache->entries[index], sizeof(cache->entries[index]));
		}
		cs_free(cache->entries);
	}
	cache->entries = NULL;
	cache->capacity = 0U;
	cache->next_tick = 0ULL;
}

int cs_key_cache_derive(cs_key_cache_t *cache,
	const char *passphrase,
	const unsigned char *salt,
	size_t salt_size,
	unsigned int rounds,
	unsigned char *out_key,
	size_t out_key_size) {
	unsigned char passphrase_hash[CS_CRYPTO_SHA256_SIZE];
	cs_key_cache_entry_t *entry = NULL;

	if (cache == NULL || cache->entries == NULL || passphrase == NULL || salt == NULL || out_key == NULL || salt_size != CS_CRYPTO_SALT_SIZE || out_key_size != CS_CRYPTO_SHA256_SIZE || rounds == 0U) {
		return -1;
	}

	if (cs_kdf_sha256(passphrase, strlen(passphrase), passphrase_hash) != 0) {
		return -1;
	}

	for (size_t index = 0U; index < cache->capacity; ++index) {
		if (cs_key_cache_match(&cache->entries[index], passphrase_hash, salt, rounds)) {
			entry = &cache->entries[index];
			break;
		}
	}

	if (entry == NULL) {
		const size_t victim_index = cs_key_cache_pick_victim(cache);
		entry = &cache->entries[victim_index];
		if (cs_kdf_pbkdf2_sha256(passphrase, salt, salt_size, rounds, entry->key, sizeof(entry->key)) != 0) {
			cs_memzero(passphrase_hash, sizeof(passphrase_hash));
			return -1;
		}
		memcpy(entry->salt, salt, salt_size);
		memcpy(entry->passphrase_hash, passphrase_hash, sizeof(passphrase_hash));
		entry->rounds = rounds;
		entry->in_use = 1;
	}

	memcpy(out_key, entry->key, out_key_size);
	entry->last_used_tick = cache->next_tick++;
	cs_memzero(passphrase_hash, sizeof(passphrase_hash));
	return 0;
}

