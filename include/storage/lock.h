#ifndef CIFRASYNC_STORAGE_LOCK_H
#define CIFRASYNC_STORAGE_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef enum cs_lock_mode {
	CS_LOCK_SHARED = 0,
	CS_LOCK_EXCLUSIVE = 1
} cs_lock_mode_t;

typedef struct cs_lock cs_lock_t;

int cs_lock_acquire(const char *repo_path, cs_lock_mode_t mode, cs_lock_t **out_lock);
void cs_lock_release(cs_lock_t *lock);
int cs_lock_try_acquire(const char *repo_path, cs_lock_mode_t mode, cs_lock_t **out_lock);

#ifdef __cplusplus
}
#endif

#endif