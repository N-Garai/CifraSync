#ifndef CIFRASYNC_FS_SCANNER_H
#define CIFRASYNC_FS_SCANNER_H

#include <stddef.h>

#include "fs/metadata.h"

typedef struct cs_fs_scan_options {
	int recursive;
	int include_directories;
	const char *const *include_patterns;
	size_t include_count;
	const char *const *exclude_patterns;
	size_t exclude_count;
} cs_fs_scan_options_t;

typedef int (*cs_fs_scan_visit_fn)(const cs_fs_metadata_t *metadata, void *ctx);

void cs_fs_scan_options_default(cs_fs_scan_options_t *options);
int cs_fs_scan(const char *root_path, const cs_fs_scan_options_t *options, cs_fs_scan_visit_fn visit, void *ctx);

#endif
