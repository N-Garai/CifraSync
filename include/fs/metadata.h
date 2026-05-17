#ifndef CIFRASYNC_FS_METADATA_H
#define CIFRASYNC_FS_METADATA_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "common/constants.h"

typedef struct cs_fs_metadata {
	char path[CS_PATH_CAP];
	unsigned long long size_bytes;
	time_t modified_time;
	unsigned long attributes;
	bool is_directory;
	bool is_regular_file;
	bool is_symlink;
} cs_fs_metadata_t;

void cs_fs_metadata_init(cs_fs_metadata_t *metadata);
int cs_fs_metadata_copy(cs_fs_metadata_t *dst, const cs_fs_metadata_t *src);
int cs_fs_metadata_set_path(cs_fs_metadata_t *metadata, const char *path);
int cs_fs_metadata_from_stat(cs_fs_metadata_t *metadata, const char *path, unsigned long attributes, unsigned long long size_bytes, time_t modified_time, bool is_directory, bool is_regular_file, bool is_symlink);
bool cs_fs_metadata_is_file(const cs_fs_metadata_t *metadata);
bool cs_fs_metadata_is_directory(const cs_fs_metadata_t *metadata);

#endif

