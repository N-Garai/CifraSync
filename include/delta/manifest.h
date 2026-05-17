#ifndef CIFRASYNC_DELTA_MANIFEST_H
#define CIFRASYNC_DELTA_MANIFEST_H

#include <stddef.h>
#include <time.h>

#include "common/constants.h"
#include "delta/chunker.h"

typedef struct cs_manifest_chunk {
	size_t offset;
	size_t size;
	char hash_hex[CS_HASH_HEX_BUFSZ];
} cs_manifest_chunk_t;

typedef struct cs_manifest_file {
	char path[CS_PATH_CAP];
	time_t modified_time;
	unsigned long long size_bytes;
	cs_manifest_chunk_t *chunks;
	size_t chunk_count;
	size_t chunk_capacity;
} cs_manifest_file_t;

typedef struct cs_manifest {
	cs_manifest_file_t *files;
	size_t file_count;
	size_t file_capacity;
	unsigned long long total_bytes;
	size_t total_chunks;
} cs_manifest_t;

int cs_manifest_init(cs_manifest_t *manifest);
void cs_manifest_reset(cs_manifest_t *manifest);
void cs_manifest_free(cs_manifest_t *manifest);

int cs_manifest_add_file(cs_manifest_t *manifest,
	const char *relative_path,
	time_t modified_time,
	unsigned long long size_bytes,
	cs_manifest_file_t **out_file);
int cs_manifest_add_chunk(cs_manifest_t *manifest,
	cs_manifest_file_t *file,
	size_t offset,
	size_t size,
	const char *hash_hex);

int cs_manifest_append_file_from_buffer(cs_manifest_t *manifest,
	const char *relative_path,
	const unsigned char *data,
	size_t data_size,
	size_t chunk_size,
	time_t modified_time,
	cs_manifest_file_t **out_file);

int cs_manifest_write(const cs_manifest_t *manifest, const char *path);
int cs_manifest_load(const char *path, cs_manifest_t *manifest);

#endif

