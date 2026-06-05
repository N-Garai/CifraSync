#include "storage/index_store.h"
#include "common/path.h"
#include "util/io_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CS_INDEX_FILE "chunks.idx"
#define CS_INDEX_DIR "index"
#define CS_INDEX_MAX_ENTRIES 1000000
#define CS_INDEX_HASH_SIZE 64

typedef struct {
	char hash[CS_INDEX_HASH_SIZE];
	char location[256];
} cs_index_entry_t;

typedef struct cs_index_store {
	char repo_path[4096];
	char index_path[4096];
	cs_index_entry_t *entries;
	size_t entry_count;
	size_t capacity;
} cs_index_store_t;

static int expand_capacity(cs_index_store_t *store) {
	cs_index_entry_t *new_entries;
	size_t new_capacity;
	
	if (store->capacity == 0) {
		new_capacity = 1024;
	} else {
		new_capacity = store->capacity * 2;
	}
	
	if (new_capacity > CS_INDEX_MAX_ENTRIES) {
		return -1;
	}
	
	new_entries = (cs_index_entry_t *)realloc(store->entries, 
	                                            new_capacity * sizeof(cs_index_entry_t));
	if (new_entries == NULL) {
		return -1;
	}
	
	store->entries = new_entries;
	store->capacity = new_capacity;
	return 0;
}

cs_index_store_t *cs_index_store_open(const char *repo_path) {
	cs_index_store_t *store;
	char index_dir[4096];
	
	if (repo_path == NULL) {
		return NULL;
	}
	
	store = (cs_index_store_t *)malloc(sizeof(*store));
	if (store == NULL) {
		return NULL;
	}
	
	memset(store, 0, sizeof(*store));
	strncpy(store->repo_path, repo_path, sizeof(store->repo_path) - 1);
	
	if (cs_path_join(index_dir, sizeof(index_dir), repo_path, CS_INDEX_DIR) != 0) {
		free(store);
		return NULL;
	}
	
	if (cs_path_join(store->index_path, sizeof(store->index_path), 
	              index_dir, CS_INDEX_FILE) != 0) {
		free(store);
		return NULL;
	}
	
	if (expand_capacity(store) != 0) {
		free(store);
		return NULL;
	}
	
	if (cs_index_store_reload(store) != 0) {
		free(store->entries);
		free(store);
		return NULL;
	}
	
	return store;
}

void cs_index_store_close(cs_index_store_t *store) {
	if (store == NULL) {
		return;
	}
	
	if (store->entries != NULL) {
		free(store->entries);
	}
	free(store);
}

int cs_index_store_insert(cs_index_store_t *store, const char *hash_hex, 
                          const char *location) {
	if (store == NULL || hash_hex == NULL || location == NULL) {
		return -1;
	}
	
	if (cs_index_store_contains(store, hash_hex)) {
		return 0;
	}
	
	if (store->entry_count >= store->capacity) {
		if (expand_capacity(store) != 0) {
			return -1;
		}
	}
	
	strncpy(store->entries[store->entry_count].hash, hash_hex, 
	        sizeof(store->entries[store->entry_count].hash) - 1);
	strncpy(store->entries[store->entry_count].location, location, 
	        sizeof(store->entries[store->entry_count].location) - 1);
	store->entry_count++;
	
	return 0;
}

int cs_index_store_contains(cs_index_store_t *store, const char *hash_hex) {
	size_t i;
	
	if (store == NULL || hash_hex == NULL) {
		return 0;
	}
	
	for (i = 0; i < store->entry_count; ++i) {
		if (strcmp(store->entries[i].hash, hash_hex) == 0) {
			return 1;
		}
	}
	
	return 0;
}

int cs_index_store_get(cs_index_store_t *store, const char *hash_hex,
                       char *location, size_t location_size) {
	size_t i;
	
	if (store == NULL || hash_hex == NULL || location == NULL) {
		return -1;
	}
	
	for (i = 0; i < store->entry_count; ++i) {
		if (strcmp(store->entries[i].hash, hash_hex) == 0) {
			strncpy(location, store->entries[i].location, location_size - 1);
			return 0;
		}
	}
	
	return -1;
}

int cs_index_store_remove(cs_index_store_t *store, const char *hash_hex) {
	size_t i;
	
	if (store == NULL || hash_hex == NULL) {
		return -1;
	}
	
	for (i = 0; i < store->entry_count; ++i) {
		if (strcmp(store->entries[i].hash, hash_hex) == 0) {
			if (i < store->entry_count - 1) {
				memmove(&store->entries[i], &store->entries[i + 1],
				        (store->entry_count - i - 1) * sizeof(cs_index_entry_t));
			}
			store->entry_count--;
			return 0;
		}
	}
	
	return -1;
}

int cs_index_store_reload(cs_index_store_t *store) {
	FILE *fp;
	char line[512];
	char hash[CS_INDEX_HASH_SIZE];
	char location[256];
	
	if (store == NULL) {
		return -1;
	}
	
	store->entry_count = 0;
	
	fp = fopen(store->index_path, "rb");
	if (fp == NULL) {
		return 0;
	}
	
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}
		
		if (sscanf(line, "%63[^:]:%255s", hash, location) == 2) {
			cs_index_store_insert(store, hash, location);
		}
	}
	
	fclose(fp);
	return 0;
}

int cs_index_store_flush(cs_index_store_t *store) {
	FILE *fp;
	size_t i;
	
	if (store == NULL) {
		return -1;
	}
	
	fp = fopen(store->index_path, "wb");
	if (fp == NULL) {
		return -1;
	}
	
	fprintf(fp, "# Index of stored chunks\n");
	fprintf(fp, "# Format: hash:location\n\n");
	
	for (i = 0; i < store->entry_count; ++i) {
		fprintf(fp, "%s:%s\n", store->entries[i].hash, store->entries[i].location);
	}
	
	fclose(fp);
	return 0;
}

unsigned long cs_index_store_count(cs_index_store_t *store) {
	if (store == NULL) {
		return 0;
	}
	return (unsigned long)store->entry_count;
}
