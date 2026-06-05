#include "storage/chunk_store.h"
#include "common/path.h"
#include "util/io_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define cs_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#define cs_mkdir(path) mkdir(path, 0755)
#endif

#define CS_CHUNKS_DIR "chunks"
#define CS_CHUNK_PATH_MAX 4096

typedef struct cs_chunk_store {
	char repo_path[4096];
	char chunks_path[4096];
} cs_chunk_store_t;

static int ensure_directory(const char *path) {
	if (path == NULL || path[0] == '\0') {
		return -1;
	}
	if (cs_mkdir(path) == 0) {
		return 0;
	}
	if (errno == EEXIST) {
		return 0;
	}
	return -1;
}

/* Get chunk path using hash prefix (first 2 chars) for fanout */
static int get_chunk_path(const char *chunks_dir, const char *hash_hex, 
                           char *chunk_path, size_t chunk_path_size) {
	char prefix_dir[CS_CHUNK_PATH_MAX];
	char prefix[8];
	
	if (chunks_dir == NULL || hash_hex == NULL || chunk_path == NULL) {
		return -1;
	}
	
	if (strlen(hash_hex) < 2) {
		return -1;
	}
	
	strncpy(prefix, hash_hex, 2);
	prefix[2] = '\0';
	
	if (cs_path_join(prefix_dir, sizeof(prefix_dir), chunks_dir, prefix) != 0) {
		return -1;
	}
	
	if (ensure_directory(prefix_dir) != 0) {
		return -1;
	}
	
	if (cs_path_join(chunk_path, chunk_path_size, prefix_dir, hash_hex) != 0) {
		return -1;
	}
	
	return 0;
}

cs_chunk_store_t *cs_chunk_store_open(const char *repo_path) {
	cs_chunk_store_t *store;
	
	if (repo_path == NULL) {
		return NULL;
	}
	
	store = (cs_chunk_store_t *)malloc(sizeof(*store));
	if (store == NULL) {
		return NULL;
	}
	
	strncpy(store->repo_path, repo_path, sizeof(store->repo_path) - 1);
	
	if (cs_path_join(store->chunks_path, sizeof(store->chunks_path), 
	              repo_path, CS_CHUNKS_DIR) != 0) {
		free(store);
		return NULL;
	}
	
	if (ensure_directory(store->chunks_path) != 0) {
		free(store);
		return NULL;
	}
	
	return store;
}

void cs_chunk_store_close(cs_chunk_store_t *store) {
	if (store != NULL) {
		free(store);
	}
}

int cs_chunk_store_put(cs_chunk_store_t *store, const char *hash_hex,
                       const unsigned char *data, size_t size) {
	char chunk_path[CS_CHUNK_PATH_MAX];
	
	if (store == NULL || hash_hex == NULL || data == NULL || size == 0) {
		return -1;
	}
	
	if (get_chunk_path(store->chunks_path, hash_hex, chunk_path, sizeof(chunk_path)) != 0) {
		return -1;
	}
	
	if (cs_io_write_file(chunk_path, data, size) != 0) {
		return -1;
	}
	
	return 0;
}

int cs_chunk_store_get(cs_chunk_store_t *store, const char *hash_hex,
                       unsigned char **data, size_t *size) {
	char chunk_path[CS_CHUNK_PATH_MAX];
	
	if (store == NULL || hash_hex == NULL || data == NULL || size == NULL) {
		return -1;
	}
	
	if (get_chunk_path(store->chunks_path, hash_hex, chunk_path, sizeof(chunk_path)) != 0) {
		return -1;
	}
	
	if (cs_io_read_file(chunk_path, data, size) != 0) {
		return -1;
	}
	
	return 0;
}

int cs_chunk_store_exists(cs_chunk_store_t *store, const char *hash_hex) {
	char chunk_path[CS_CHUNK_PATH_MAX];
	FILE *fp;
	
	if (store == NULL || hash_hex == NULL) {
		return 0;
	}
	
	if (get_chunk_path(store->chunks_path, hash_hex, chunk_path, sizeof(chunk_path)) != 0) {
		return 0;
	}
	
	fp = fopen(chunk_path, "rb");
	if (fp == NULL) {
		return 0;
	}
	
	fclose(fp);
	return 1;
}

int cs_chunk_store_delete(cs_chunk_store_t *store, const char *hash_hex) {
	char chunk_path[CS_CHUNK_PATH_MAX];
	
	if (store == NULL || hash_hex == NULL) {
		return -1;
	}
	
	if (get_chunk_path(store->chunks_path, hash_hex, chunk_path, sizeof(chunk_path)) != 0) {
		return -1;
	}
	
	if (remove(chunk_path) != 0) {
		return -1;
	}
	
	return 0;
}

unsigned long long cs_chunk_store_total_size(cs_chunk_store_t *store) {
	unsigned long long total = 0;
	
	if (store == NULL) {
		return 0;
	}
	
	return total;
}

unsigned long cs_chunk_store_count(cs_chunk_store_t *store) {
	unsigned long count = 0;
	
	if (store == NULL) {
		return 0;
	}
	
	return count;
}
