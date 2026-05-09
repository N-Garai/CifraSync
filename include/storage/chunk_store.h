#ifndef CIFRASYNC_STORAGE_CHUNK_STORE_H
#define CIFRASYNC_STORAGE_CHUNK_STORE_H

#include <stddef.h>

typedef struct cs_chunk_store cs_chunk_store_t;

/* Open/create a chunk store in a repository */
cs_chunk_store_t *cs_chunk_store_open(const char *repo_path);

/* Close and free chunk store */
void cs_chunk_store_close(cs_chunk_store_t *store);

/* Store a chunk by its hash */
int cs_chunk_store_put(cs_chunk_store_t *store, const char *hash_hex, 
                       const unsigned char *data, size_t size);

/* Retrieve a chunk by its hash (caller must free returned buffer) */
int cs_chunk_store_get(cs_chunk_store_t *store, const char *hash_hex,
                       unsigned char **data, size_t *size);

/* Check if a chunk exists by hash */
int cs_chunk_store_exists(cs_chunk_store_t *store, const char *hash_hex);

/* Delete a chunk by hash */
int cs_chunk_store_delete(cs_chunk_store_t *store, const char *hash_hex);

/* Get total size of all stored chunks */
unsigned long long cs_chunk_store_total_size(cs_chunk_store_t *store);

/* Get count of stored chunks */
unsigned long cs_chunk_store_count(cs_chunk_store_t *store);

#endif
