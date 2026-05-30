#include "cli/commands.h"
#include "cli/parser.h"

#include "core/engine.h"
#include "common/path.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define cs_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define cs_mkdir(path) mkdir(path, 0777)
#endif

enum {
	CS_OK = 0,
	CS_ERR_USAGE = 1,
	CS_ERR_INVALID = 2,
	CS_ERR_UNSUPPORTED = 3,
	CS_PATH_CAP = 4096
};

static const char *k_version = "0.1.0";

static int path_join(char *buffer, size_t buffer_size, const char *left, const char *right) {
	int written;
	if (buffer == NULL || left == NULL || right == NULL || buffer_size == 0) {
		return -1;
	}

#ifdef _WIN32
	written = snprintf(buffer, buffer_size, "%s\\%s", left, right);
#else
	written = snprintf(buffer, buffer_size, "%s/%s", left, right);
#endif
	if (written < 0 || (size_t)written >= buffer_size) {
		return -1;
	}
	return 0;
}

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

static int ensure_repo_layout(const char *repo_path) {
	char buffer[CS_PATH_CAP];
	const char *dirs[] = {"chunks", "snapshots", "index", "journal", "locks"};
	size_t i;

	if (ensure_directory(repo_path) != 0) {
		return -1;
	}

	for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); ++i) {
		if (path_join(buffer, sizeof(buffer), repo_path, dirs[i]) != 0) {
			return -1;
		}
		if (ensure_directory(buffer) != 0) {
			return -1;
		}
	}

	if (path_join(buffer, sizeof(buffer), repo_path, "repo.meta") != 0) {
		return -1;
	}

	FILE *meta = fopen(buffer, "wb");
	if (meta == NULL) {
		return -1;
	}
	fprintf(meta, "name=CifraSync\n");
	fprintf(meta, "version=1\n");
	fprintf(meta, "platform=windows\n");
	fclose(meta);
	return 0;
}

static int is_required_missing(const char *value) {
	return value == NULL || value[0] == '\0';
}

static void print_banner(void) {
	puts("CifraSync - Encrypted Incremental Backup & Sync");
}

void cs_print_help(void) {
	print_banner();
	puts("");
	puts("Usage:");
	puts("  cifrasync <command> [options]");
	puts("");
	puts("Commands:");
	puts("  init       Initialize a repository");
	puts("  backup     Create an incremental backup");
	puts("  list       List snapshots");
	puts("  restore    Restore from a snapshot");
	puts("  verify     Verify stored data integrity");
	puts("  prune      Remove old snapshots");
	puts("  sync       Synchronize with remote repository");
	puts("");
	puts("Global options:");
	puts("  -h, --help       Show help");
	puts("  -V, --version    Show version");
	puts("");
	puts("Command options:");
	puts("  init     --repo PATH");
	puts("  backup   --source PATH --repo PATH [--dry-run] [--compress] [--encrypt] [--label TEXT]");
	puts("           [--include-file FILE] [--exclude-file FILE]");
	puts("  list     --repo PATH");
	puts("  restore  --repo PATH --snapshot ID --out PATH");
	puts("  verify   --repo PATH");
	puts("  prune    --repo PATH [--keep-last N] [--older-than DAYS]");
	puts("  sync     --repo PATH --remote HOST:PORT");
}

const char *cs_version(void) {
	return k_version;
}



static int handle_init(const cs_cli_options_t *options) {
	char buffer[CS_PATH_CAP];
	if (is_required_missing(options->repo)) {
		fprintf(stderr, "init requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	if (ensure_repo_layout(options->repo) != 0) {
		perror("init failed");
		return CS_ERR_INVALID;
	}
	if (path_join(buffer, sizeof(buffer), options->repo, "repo.meta") != 0) {
		fprintf(stderr, "init: failed to resolve repo meta path\n");
		return CS_ERR_INVALID;
	}
	printf("Initialized repository at %s\n", options->repo);
	printf("Metadata file: %s\n", buffer);
	return CS_OK;
}

static int handle_list(const cs_cli_options_t *options) {
	char pattern[CS_PATH_CAP];
	char snapshots_dir[CS_PATH_CAP];
	WIN32_FIND_DATAA find_data;
	HANDLE handle;
	int found_any = 0;

	if (is_required_missing(options->repo)) {
		fprintf(stderr, "list requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	if (path_join(snapshots_dir, sizeof(snapshots_dir), options->repo, "snapshots") != 0) {
		fprintf(stderr, "list: failed to build snapshots directory path\n");
		return CS_ERR_INVALID;
	}
	if (snprintf(pattern, sizeof(pattern), "%s\\*", snapshots_dir) < 0) {
		fprintf(stderr, "list: failed to build search pattern\n");
		return CS_ERR_INVALID;
	}

	handle = FindFirstFileA(pattern, &find_data);
	if (handle == INVALID_HANDLE_VALUE) {
		printf("No snapshots found in %s\n", snapshots_dir);
		return CS_OK;
	}

	puts("Snapshots:");
	do {
		if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			continue;
		}
		if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
			continue;
		}
		found_any = 1;
		printf("  %s\n", find_data.cFileName);
	} while (FindNextFileA(handle, &find_data) != 0);
	FindClose(handle);

	if (!found_any) {
		puts("  (none)");
	}
	return CS_OK;
}

static int handle_backup(const cs_cli_options_t *options) {
	char **include_patterns = NULL;
	char **exclude_patterns = NULL;
	size_t include_count = 0U;
	size_t exclude_count = 0U;
	int result;

	if (is_required_missing(options->repo) || is_required_missing(options->source)) {
		fprintf(stderr, "backup requires --source PATH and --repo PATH\n");
		return CS_ERR_USAGE;
	}

	if (options->include_file != NULL) {
		if (cs_path_load_patterns_file(options->include_file, &include_patterns, &include_count) != 0) {
			fprintf(stderr, "warning: failed to load include patterns from %s\n", options->include_file);
		} else {
			printf("Loaded %lu include patterns from %s\n", (unsigned long)include_count, options->include_file);
		}
	}

	if (options->exclude_file != NULL) {
		if (cs_path_load_patterns_file(options->exclude_file, &exclude_patterns, &exclude_count) != 0) {
			fprintf(stderr, "warning: failed to load exclude patterns from %s\n", options->exclude_file);
		} else {
			printf("Loaded %lu exclude patterns from %s\n", (unsigned long)exclude_count, options->exclude_file);
		}
	}

	result = cs_engine_backup(options->source, options->repo,
							  options->dry_run, options->compress, options->encrypt, options->label,
							  (const char *const *)include_patterns, include_count,
							  (const char *const *)exclude_patterns, exclude_count);

	cs_path_free_patterns(include_patterns, include_count);
	cs_path_free_patterns(exclude_patterns, exclude_count);

	if (result != 0) {
		fprintf(stderr, "backup failed\n");
		return CS_ERR_INVALID;
	}
	printf("Backup complete.\n");
	return CS_OK;
}

static int handle_restore(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo) || is_required_missing(options->snapshot) || is_required_missing(options->output)) {
		fprintf(stderr, "restore requires --repo PATH --snapshot ID --out PATH\n");
		return CS_ERR_USAGE;
	}
	if (cs_engine_restore(options->repo, options->snapshot, options->output) != 0) {
		fprintf(stderr, "restore failed\n");
		return CS_ERR_INVALID;
	}
	printf("Restore complete.\n");
	return CS_OK;
}

static int handle_verify(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo)) {
		fprintf(stderr, "verify requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	fprintf(stderr, "verify is parsed correctly but not yet wired to the storage engine.\n");
	return CS_ERR_UNSUPPORTED;
}

static int handle_prune(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo)) {
		fprintf(stderr, "prune requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	fprintf(stderr, "prune is parsed correctly but not yet wired to the storage engine.\n");
	return CS_ERR_UNSUPPORTED;
}

static int handle_sync(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo) || is_required_missing(options->remote)) {
		fprintf(stderr, "sync requires --repo PATH --remote HOST:PORT\n");
		return CS_ERR_USAGE;
	}
	fprintf(stderr, "sync is parsed correctly but not yet wired to the storage engine.\n");
	return CS_ERR_UNSUPPORTED;
}

int cs_run(int argc, char **argv) {
	cs_cli_options_t options;
	char error_buffer[128];

	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		cs_print_help();
		return CS_ERR_USAGE;
	}

	cs_cli_options_init(&options);
	error_buffer[0] = '\0';
	if (cs_parse_cli(argc, argv, &options, error_buffer, sizeof(error_buffer)) != 0) {
		fprintf(stderr, "Error: %s\n", error_buffer[0] ? error_buffer : "invalid arguments");
		fprintf(stderr, "Run 'cifrasync --help' for usage.\n");
		return CS_ERR_USAGE;
	}

	switch (options.command) {
		case CS_CMD_HELP:
			cs_print_help();
			return CS_OK;
		case CS_CMD_VERSION:
			printf("cifrasync %s\n", cs_version());
			return CS_OK;
		case CS_CMD_INIT:
			return handle_init(&options);
		case CS_CMD_LIST:
			return handle_list(&options);
		case CS_CMD_BACKUP:
			return handle_backup(&options);
		case CS_CMD_RESTORE:
			return handle_restore(&options);
		case CS_CMD_VERIFY:
			return handle_verify(&options);
		case CS_CMD_PRUNE:
			return handle_prune(&options);
		case CS_CMD_SYNC:
			return handle_sync(&options);
		default:
			fprintf(stderr, "Unknown command.\n");
			return CS_ERR_USAGE;
	}
}
