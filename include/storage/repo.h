#ifndef CIFRASYNC_STORAGE_REPO_H
#define CIFRASYNC_STORAGE_REPO_H

#include <time.h>

typedef struct {
	char path[4096];
	char version[32];
	time_t created_at;
	char platform[16];
} cs_repo_t;

/* Initialize a repository at the given path */
int cs_repo_init(const char *repo_path, cs_repo_t *repo);

/* Load repository metadata from existing repo */
int cs_repo_load(const char *repo_path, cs_repo_t *repo);

/* Save repository metadata */
int cs_repo_save(const cs_repo_t *repo);

/* Get repository version string */
const char *cs_repo_version(void);

/* Check if a repository already exists at path */
int cs_repo_exists(const char *repo_path);

/* Validate repository structure */
int cs_repo_validate(const char *repo_path);

#endif
