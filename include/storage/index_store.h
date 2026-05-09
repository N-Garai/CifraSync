#ifndef CIFRASYNC_STORAGE_INDEX_STORE_H
#define CIFRASYNC_STORAGE_INDEX_STORE_H

#include <stddef.h>

typedef struct cs_index_store cs_index_store_t;

/* Open/create an index store in a repository */
cs_index_store_t *cs_index_store_open(const char *repo_path);

/* Close and free index store */
void cs_index_store_close(cs_index_store_t *store);

/* Add a hash entry to the index */
int cs_index_store_insert(cs_index_store_t *store, const char *hash_hex, const char *location);

/* Check if hash exists in index */
int cs_index_store_contains(cs_index_store_t *store, const char *hash_hex);

/* Get location of hash from index */
int cs_index_store_get(cs_index_store_t *store, const char *hash_hex, 
                       char *location, size_t location_size);

/* Remove hash from index */
int cs_index_store_remove(cs_index_store_t *store, const char *hash_hex);

/* Reload index from disk */
int cs_index_store_reload(cs_index_store_t *store);

/* Flush index to disk */
int cs_index_store_flush(cs_index_store_t *store);

/* Get number of entries in index */
unsigned long cs_index_store_count(cs_index_store_t *store);

#endif
