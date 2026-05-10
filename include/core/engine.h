#ifndef CIFRASYNC_CORE_ENGINE_H
#define CIFRASYNC_CORE_ENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* High-level engine API for backup and restore orchestration. */
int cs_engine_backup(const char *source_path, const char *repo_path, int dry_run, int compress, int encrypt, const char *label);
int cs_engine_restore(const char *repo_path, const char *snapshot_id, const char *out_path);

int cs_engine_init(void);

#ifdef __cplusplus
}
#endif

#endif

