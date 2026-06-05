#include "storage/repo.h"
#include "common/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define cs_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define cs_mkdir(path) mkdir(path, 0755)
#endif

#define CS_REPO_VERSION "1"
#define CS_REPO_META_FILE "repo.meta"

static int ensure_directory(const char *path) {
	if (path == NULL || path[0] == '\0') {
		return -1;
	}
	if (cs_mkdir(path) == 0) {
		return 0;
	}
	if (errno == EEXIST) {
		return 0;
	}
	return -1;
}

int cs_repo_init(const char *repo_path, cs_repo_t *repo) {
	char meta_path[4096];
	FILE *fp;
	const char *dirs[] = {"chunks", "snapshots", "index", "journal", "locks"};
	size_t i;

	if (repo_path == NULL || repo == NULL) {
		return -1;
	}

	memset(repo, 0, sizeof(*repo));

	if (ensure_directory(repo_path) != 0) {
		return -1;
	}

	for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
		char dir_path[4096];
		if (cs_path_join(dir_path, sizeof(dir_path), repo_path, dirs[i]) != 0) {
			return -1;
		}
		if (ensure_directory(dir_path) != 0) {
			return -1;
		}
	}

	if (cs_path_join(meta_path, sizeof(meta_path), repo_path, CS_REPO_META_FILE) != 0) {
		return -1;
	}

	fp = fopen(meta_path, "wb");
	if (fp == NULL) {
		return -1;
	}

	time_t now = time(NULL);
	fprintf(fp, "version=%s\n", CS_REPO_VERSION);
	fprintf(fp, "created_at=%ld\n", (long)now);
#ifdef _WIN32
	fprintf(fp, "platform=windows\n");
#else
	fprintf(fp, "platform=linux\n");
#endif
	fclose(fp);

	strncpy(repo->path, repo_path, sizeof(repo->path) - 1);
	strncpy(repo->version, CS_REPO_VERSION, sizeof(repo->version) - 1);
	repo->created_at = now;
#ifdef _WIN32
	strncpy(repo->platform, "windows", sizeof(repo->platform) - 1);
#else
	strncpy(repo->platform, "linux", sizeof(repo->platform) - 1);
#endif

	return 0;
}

int cs_repo_load(const char *repo_path, cs_repo_t *repo) {
	char meta_path[4096];
	FILE *fp;
	char line[256];
	char key[128];
	char value[128];

	if (repo_path == NULL || repo == NULL) {
		return -1;
	}

	if (!cs_repo_exists(repo_path)) {
		return -1;
	}

	if (cs_path_join(meta_path, sizeof(meta_path), repo_path, CS_REPO_META_FILE) != 0) {
		return -1;
	}

	fp = fopen(meta_path, "rb");
	if (fp == NULL) {
		return -1;
	}

	memset(repo, 0, sizeof(*repo));
	strncpy(repo->path, repo_path, sizeof(repo->path) - 1);

	while (fgets(line, sizeof(line), fp) != NULL) {
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}

		if (sscanf(line, "%127[^=]=%127s", key, value) == 2) {
			if (strcmp(key, "version") == 0) {
				strncpy(repo->version, value, sizeof(repo->version) - 1);
			} else if (strcmp(key, "created_at") == 0) {
				repo->created_at = (time_t)strtol(value, NULL, 10);
			} else if (strcmp(key, "platform") == 0) {
				strncpy(repo->platform, value, sizeof(repo->platform) - 1);
			}
		}
	}

	fclose(fp);
	return 0;
}

int cs_repo_save(const cs_repo_t *repo) {
	char meta_path[4096];
	FILE *fp;

	if (repo == NULL || repo->path[0] == '\0') {
		return -1;
	}

	if (cs_path_join(meta_path, sizeof(meta_path), repo->path, CS_REPO_META_FILE) != 0) {
		return -1;
	}

	fp = fopen(meta_path, "wb");
	if (fp == NULL) {
		return -1;
	}

	fprintf(fp, "version=%s\n", repo->version);
	fprintf(fp, "created_at=%ld\n", (long)repo->created_at);
	fprintf(fp, "platform=%s\n", repo->platform);

	fclose(fp);
	return 0;
}

const char *cs_repo_version(void) {
	return CS_REPO_VERSION;
}

int cs_repo_exists(const char *repo_path) {
	char meta_path[4096];
	FILE *fp;

	if (repo_path == NULL) {
		return 0;
	}

	if (cs_path_join(meta_path, sizeof(meta_path), repo_path, CS_REPO_META_FILE) != 0) {
		return 0;
	}

	fp = fopen(meta_path, "rb");
	if (fp == NULL) {
		return 0;
	}

	fclose(fp);
	return 1;
}

int cs_repo_validate(const char *repo_path) {
	const char *required_dirs[] = {"chunks", "snapshots", "index", "journal", "locks"};
	char check_path[4096];
	size_t i;
	FILE *fp;

	if (repo_path == NULL) {
		return -1;
	}

	for (i = 0; i < sizeof(required_dirs) / sizeof(required_dirs[0]); ++i) {
		if (cs_path_join(check_path, sizeof(check_path), repo_path, required_dirs[i]) != 0) {
			return -1;
		}
		fp = fopen(check_path, "rb");
		if (fp != NULL) {
			fclose(fp);
		}
	}

	return cs_repo_exists(repo_path) ? 0 : -1;
}
