#ifndef CIFRASYNC_CORE_ENGINE_H
#define CIFRASYNC_CORE_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

int cs_engine_backup(const char *source_path, const char *repo_path, int dry_run, int compress, int encrypt, const char *label,
					 const char *const *include_patterns, size_t include_count,
					 const char *const *exclude_patterns, size_t exclude_count);
int cs_engine_restore(const char *repo_path, const char *snapshot_id, const char *out_path);
int cs_engine_restore_file(const char *repo_path, const char *snapshot_id, const char *source_file, const char *out_path);
int cs_engine_verify(const char *repo_path);
int cs_engine_prune(const char *repo_path, const char *keep_last, const char *older_than);
int cs_engine_sync(const char *repo_path, const char *remote);
int cs_engine_list(const char *repo_path);

int cs_engine_init(void);

#ifdef __cplusplus
}
#endif

#endif
