#include "storage/snapshot_store.h"
#include "common/path.h"
#include "util/io_utils.h"
#include "util/time_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

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

#define CS_SNAPSHOTS_DIR "snapshots"
#define CS_SNAPSHOT_EXT ".snapshot"
#define CS_SNAPSHOT_MAX 4096

typedef struct cs_snapshot_store {
	char repo_path[4096];
	char snapshots_path[4096];
	cs_snapshot_t *snapshots;
	size_t count;
	size_t capacity;
} cs_snapshot_store_t;

static int ensure_directory(const char *path) {
	if (path == NULL || path[0] == '\0') {
		return -1;
	}
	if (cs_mkdir(path) == 0) {
		return 0;
	}
#ifdef _WIN32
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return 0;
	}
#else
	if (errno == EEXIST) {
		return 0;
	}
#endif
	return -1;
}

static int snapshot_filename_component(const char *snapshot_id, char *out, size_t out_size) {
	size_t src_index;
	size_t dst_index;

	if (snapshot_id == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	for (src_index = 0U, dst_index = 0U; snapshot_id[src_index] != '\0'; ++src_index) {
		unsigned char ch = (unsigned char)snapshot_id[src_index];
		char mapped = (char)ch;

		if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_' || ch == '.')) {
			mapped = '-';
		}

		if (dst_index + 1U >= out_size) {
			return -1;
		}
		out[dst_index++] = mapped;
	}

	out[dst_index] = '\0';
	return 0;
}

static int snapshot_artifact_path(const char *repo_path, const char *snapshot_id, const char *suffix, char *out, size_t out_size) {
	char snapshots_path[4096];
	char snapshot_base[4096];
	char snapshot_name[4096];

	if (snapshot_filename_component(snapshot_id, snapshot_name, sizeof(snapshot_name)) != 0) {
		return -1;
	}
	if (cs_path_join(snapshots_path, sizeof(snapshots_path), repo_path, CS_SNAPSHOTS_DIR) != 0) {
		return -1;
	}
	if (cs_path_join(snapshot_base, sizeof(snapshot_base), snapshots_path, snapshot_name) != 0) {
		return -1;
	}
	if (snprintf(out, out_size, "%s%s", snapshot_base, suffix) < 0 || strlen(out) + 1U > out_size) {
		return -1;
	}
	return 0;
}

cs_snapshot_store_t *cs_snapshot_store_open(const char *repo_path) {
	cs_snapshot_store_t *store;
	
	if (repo_path == NULL) {
		return NULL;
	}
	
	store = (cs_snapshot_store_t *)malloc(sizeof(*store));
	if (store == NULL) {
		return NULL;
	}
	
	memset(store, 0, sizeof(*store));
	strncpy(store->repo_path, repo_path, sizeof(store->repo_path) - 1);
	
	if (cs_path_join(store->snapshots_path, sizeof(store->snapshots_path),
	              repo_path, CS_SNAPSHOTS_DIR) != 0) {
		free(store);
		return NULL;
	}
	
	if (ensure_directory(store->snapshots_path) != 0) {
		free(store);
		return NULL;
	}
	
	store->capacity = 128;
	store->snapshots = (cs_snapshot_t *)malloc(store->capacity * sizeof(cs_snapshot_t));
	if (store->snapshots == NULL) {
		free(store);
		return NULL;
	}
	
	return store;
}

void cs_snapshot_store_close(cs_snapshot_store_t *store) {
	if (store == NULL) {
		return;
	}
	
	if (store->snapshots != NULL) {
		free(store->snapshots);
	}
	free(store);
}

int cs_snapshot_store_create(cs_snapshot_store_t *store, const char *source_path,
                             const char *label, cs_snapshot_t *snapshot) {
	char snapshot_path[4096];
	FILE *fp;
	time_t now;
	char time_str[64];
	
	if (store == NULL || source_path == NULL || snapshot == NULL) {
		return -1;
	}
	
	now = cs_time_now_unix();
	if (cs_time_unix_to_iso8601(now, time_str, sizeof(time_str)) != 0) {
		return -1;
	}
	
	memset(snapshot, 0, sizeof(*snapshot));
	strncpy(snapshot->id, time_str, sizeof(snapshot->id) - 1);
	snapshot->timestamp = now;
	strncpy(snapshot->source_path, source_path, sizeof(snapshot->source_path) - 1);
	if (label != NULL) {
		strncpy(snapshot->label, label, sizeof(snapshot->label) - 1);
	}
	snapshot->file_count = 0;
	snapshot->size_bytes = 0;
	
	if (cs_path_join(snapshot_path, sizeof(snapshot_path), store->snapshots_path,
	              snapshot->id) != 0) {
		return -1;
	}

	if (snapshot_artifact_path(store->repo_path, snapshot->id, CS_SNAPSHOT_EXT, snapshot_path, sizeof(snapshot_path)) != 0) {
		return -1;
	}
	
	fp = fopen(snapshot_path, "wb");
	if (fp == NULL) {
		return -1;
	}
	
	fprintf(fp, "id=%s\n", snapshot->id);
	fprintf(fp, "timestamp=%ld\n", (long)snapshot->timestamp);
	fprintf(fp, "source_path=%s\n", snapshot->source_path);
	fprintf(fp, "label=%s\n", snapshot->label);
	fprintf(fp, "file_count=%lu\n", snapshot->file_count);
	fprintf(fp, "size_bytes=%llu\n", snapshot->size_bytes);
	
	fclose(fp);
	
	if (store->count < store->capacity) {
		store->snapshots[store->count++] = *snapshot;
	}
	
	return 0;
}

int cs_snapshot_store_list(cs_snapshot_store_t *store, cs_snapshot_t **snapshots,
                           size_t *count) {
	if (store == NULL || snapshots == NULL || count == NULL) {
		return -1;
	}
	
	*snapshots = (cs_snapshot_t *)malloc(store->count * sizeof(cs_snapshot_t));
	if (*snapshots == NULL && store->count > 0) {
		return -1;
	}
	
	memcpy(*snapshots, store->snapshots, store->count * sizeof(cs_snapshot_t));
	*count = store->count;
	
	return 0;
}

int cs_snapshot_store_get(cs_snapshot_store_t *store, const char *snapshot_id,
                          cs_snapshot_t *snapshot) {
	size_t i;
	
	if (store == NULL || snapshot_id == NULL || snapshot == NULL) {
		return -1;
	}
	
	for (i = 0; i < store->count; ++i) {
		if (strcmp(store->snapshots[i].id, snapshot_id) == 0) {
			memcpy(snapshot, &store->snapshots[i], sizeof(*snapshot));
			return 0;
		}
	}
	
	return -1;
}

int cs_snapshot_store_update(cs_snapshot_store_t *store, const cs_snapshot_t *snapshot) {
	char snapshot_path[4096];
	FILE *fp;
	size_t i;
	
	if (store == NULL || snapshot == NULL) {
		return -1;
	}
	
	for (i = 0; i < store->count; ++i) {
		if (strcmp(store->snapshots[i].id, snapshot->id) == 0) {
			memcpy(&store->snapshots[i], snapshot, sizeof(*snapshot));
			
			if (cs_path_join(snapshot_path, sizeof(snapshot_path), 
			              store->snapshots_path, snapshot->id) != 0) {
				return -1;
			}
			if (snapshot_artifact_path(store->repo_path, snapshot->id, CS_SNAPSHOT_EXT, snapshot_path, sizeof(snapshot_path)) != 0) {
				return -1;
			}
			
			fp = fopen(snapshot_path, "wb");
			if (fp == NULL) {
				return -1;
			}
			
			fprintf(fp, "id=%s\n", snapshot->id);
			fprintf(fp, "timestamp=%ld\n", (long)snapshot->timestamp);
			fprintf(fp, "source_path=%s\n", snapshot->source_path);
			fprintf(fp, "label=%s\n", snapshot->label);
			fprintf(fp, "file_count=%lu\n", snapshot->file_count);
			fprintf(fp, "size_bytes=%llu\n", snapshot->size_bytes);
			
			fclose(fp);
			return 0;
		}
	}
	
	return -1;
}

int cs_snapshot_store_delete(cs_snapshot_store_t *store, const char *snapshot_id) {
	char snapshot_path[4096];
	size_t i;
	
	if (store == NULL || snapshot_id == NULL) {
		return -1;
	}
	
	for (i = 0; i < store->count; ++i) {
		if (strcmp(store->snapshots[i].id, snapshot_id) == 0) {
			if (cs_path_join(snapshot_path, sizeof(snapshot_path),
			              store->snapshots_path, snapshot_id) != 0) {
				return -1;
			}
				if (snapshot_artifact_path(store->repo_path, snapshot_id, CS_SNAPSHOT_EXT, snapshot_path, sizeof(snapshot_path)) != 0) {
					return -1;
				}
			
			if (remove(snapshot_path) != 0) {
				return -1;
			}
			
			if (i < store->count - 1) {
				memmove(&store->snapshots[i], &store->snapshots[i + 1],
				        (store->count - i - 1) * sizeof(cs_snapshot_t));
			}
			store->count--;
			return 0;
		}
	}
	
	return -1;
}

unsigned long cs_snapshot_store_count(cs_snapshot_store_t *store) {
	if (store == NULL) {
		return 0;
	}
	return (unsigned long)store->count;
}

int cs_snapshot_store_latest(cs_snapshot_store_t *store, cs_snapshot_t *snapshot) {
	time_t latest_time = 0;
	int latest_idx = -1;
	size_t i;
	
	if (store == NULL || snapshot == NULL) {
		return -1;
	}
	
	if (store->count == 0) {
		return -1;
	}
	
	for (i = 0; i < store->count; ++i) {
		if (store->snapshots[i].timestamp > latest_time) {
			latest_time = store->snapshots[i].timestamp;
			latest_idx = (int)i;
		}
	}
	
	if (latest_idx < 0) {
		return -1;
	}
	
	memcpy(snapshot, &store->snapshots[latest_idx], sizeof(*snapshot));
	return 0;
}
