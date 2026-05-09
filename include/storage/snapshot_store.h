#ifndef CIFRASYNC_STORAGE_SNAPSHOT_STORE_H
#define CIFRASYNC_STORAGE_SNAPSHOT_STORE_H

#include <time.h>
#include <stddef.h>

typedef struct {
	char id[64];
	time_t timestamp;
	char source_path[4096];
	char label[256];
	unsigned long long size_bytes;
	unsigned long file_count;
} cs_snapshot_t;

typedef struct cs_snapshot_store cs_snapshot_store_t;

/* Open/create a snapshot store in a repository */
cs_snapshot_store_t *cs_snapshot_store_open(const char *repo_path);

/* Close and free snapshot store */
void cs_snapshot_store_close(cs_snapshot_store_t *store);

/* Create a new snapshot */
int cs_snapshot_store_create(cs_snapshot_store_t *store, const char *source_path,
                             const char *label, cs_snapshot_t *snapshot);

/* List all snapshots (caller must free returned array) */
int cs_snapshot_store_list(cs_snapshot_store_t *store, cs_snapshot_t **snapshots, 
                           size_t *count);

/* Get a snapshot by ID */
int cs_snapshot_store_get(cs_snapshot_store_t *store, const char *snapshot_id, 
                          cs_snapshot_t *snapshot);

/* Update snapshot metadata */
int cs_snapshot_store_update(cs_snapshot_store_t *store, const cs_snapshot_t *snapshot);

/* Delete a snapshot by ID */
int cs_snapshot_store_delete(cs_snapshot_store_t *store, const char *snapshot_id);

/* Get count of snapshots */
unsigned long cs_snapshot_store_count(cs_snapshot_store_t *store);

/* Get newest snapshot */
int cs_snapshot_store_latest(cs_snapshot_store_t *store, cs_snapshot_t *snapshot);

#endif
