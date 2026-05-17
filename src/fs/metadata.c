#include "fs/metadata.h"

#include "common/memory.h"
#include "common/path.h"

#include <string.h>

void cs_fs_metadata_init(cs_fs_metadata_t *metadata) {
	if (metadata == NULL) {
		return;
	}
	cs_memzero(metadata, sizeof(*metadata));
}

int cs_fs_metadata_copy(cs_fs_metadata_t *dst, const cs_fs_metadata_t *src) {
	if (dst == NULL || src == NULL) {
		return -1;
	}
	memcpy(dst, src, sizeof(*dst));
	return 0;
}

int cs_fs_metadata_set_path(cs_fs_metadata_t *metadata, const char *path) {
	if (metadata == NULL || path == NULL) {
		return -1;
	}
	if (cs_path_normalize_copy(path, metadata->path, sizeof(metadata->path)) != 0) {
		return -1;
	}
	return 0;
}

int cs_fs_metadata_from_stat(cs_fs_metadata_t *metadata, const char *path, unsigned long attributes, unsigned long long size_bytes, time_t modified_time, bool is_directory, bool is_regular_file, bool is_symlink) {
	if (metadata == NULL || path == NULL) {
		return -1;
	}
	cs_fs_metadata_init(metadata);
	if (cs_fs_metadata_set_path(metadata, path) != 0) {
		return -1;
	}
	metadata->attributes = attributes;
	metadata->size_bytes = size_bytes;
	metadata->modified_time = modified_time;
	metadata->is_directory = is_directory;
	metadata->is_regular_file = is_regular_file;
	metadata->is_symlink = is_symlink;
	return 0;
}

bool cs_fs_metadata_is_file(const cs_fs_metadata_t *metadata) {
	return metadata != NULL && metadata->is_regular_file;
}

bool cs_fs_metadata_is_directory(const cs_fs_metadata_t *metadata) {
	return metadata != NULL && metadata->is_directory;
}

