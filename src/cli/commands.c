#include "cli/commands.h"
#include "cli/parser.h"

#include "core/engine.h"
#include "common/path.h"
#include "storage/repo.h"
#include "util/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	CS_OK = 0,
	CS_ERR_USAGE = 1,
	CS_ERR_INVALID = 2,
	CS_ERR_UNSUPPORTED = 3,
	CS_PATH_CAP = 4096
};

static const char *k_version = "0.1.0";

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
	puts("  restore  --repo PATH --snapshot ID --out PATH [--source-file PATH]");
	puts("  verify   --repo PATH");
	puts("  prune    --repo PATH [--keep-last N] [--older-than DAYS]");
	puts("  sync     --repo PATH --remote HOST:PORT");
}

const char *cs_version(void) {
	return k_version;
}



static int handle_init(const cs_cli_options_t *options) {
	cs_repo_t repo;
	if (is_required_missing(options->repo)) {
		fprintf(stderr, "init requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	if (cs_repo_init(options->repo, &repo) != 0) {
		fprintf(stderr, "init failed for '%s'\n", options->repo);
		return CS_ERR_INVALID;
	}
	printf("Initialized repository at %s\n", options->repo);
	return CS_OK;
}

static int handle_list(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo)) {
		fprintf(stderr, "list requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	return cs_engine_list(options->repo);
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
	if (options->source_file != NULL && options->source_file[0] != '\0') {
		if (cs_engine_restore_file(options->repo, options->snapshot, options->source_file, options->output) != 0) {
			fprintf(stderr, "restore single file failed\n");
			return CS_ERR_INVALID;
		}
		printf("Single file restore complete.\n");
	} else {
		if (cs_engine_restore(options->repo, options->snapshot, options->output) != 0) {
			fprintf(stderr, "restore failed\n");
			return CS_ERR_INVALID;
		}
		printf("Restore complete.\n");
	}
	return CS_OK;
}

static int handle_verify(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo)) {
		fprintf(stderr, "verify requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	if (cs_engine_verify(options->repo) != 0) {
		fprintf(stderr, "verify: data integrity check failed\n");
		return CS_ERR_INVALID;
	}
	printf("Verify complete: all chunks pass integrity check.\n");
	return CS_OK;
}

static int handle_prune(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo)) {
		fprintf(stderr, "prune requires --repo PATH\n");
		return CS_ERR_USAGE;
	}
	if (cs_engine_prune(options->repo, options->keep_last, options->older_than) != 0) {
		fprintf(stderr, "prune failed\n");
		return CS_ERR_INVALID;
	}
	return CS_OK;
}

static int handle_sync(const cs_cli_options_t *options) {
	if (is_required_missing(options->repo) || is_required_missing(options->remote)) {
		fprintf(stderr, "sync requires --repo PATH --remote HOST:PORT\n");
		return CS_ERR_USAGE;
	}
	if (cs_engine_sync(options->repo, options->remote) != 0) {
		fprintf(stderr, "sync failed\n");
		return CS_ERR_INVALID;
	}
	printf("Sync complete.\n");
	return CS_OK;
}

int cs_run(int argc, char **argv) {
	cs_cli_options_t options;
	char error_buffer[128];
	cs_config_t config;
	int result;

	cs_config_init(&config);
	{
		FILE *cfg_fp = fopen("cifrasync.conf", "r");
		if (cfg_fp != NULL) {
			fclose(cfg_fp);
			cs_config_load(&config, "cifrasync.conf");
		}
	}

	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		cs_print_help();
		cs_config_free(&config);
		return CS_ERR_USAGE;
	}

	cs_cli_options_init(&options);
	error_buffer[0] = '\0';
	if (cs_parse_cli(argc, argv, &options, error_buffer, sizeof(error_buffer)) != 0) {
		fprintf(stderr, "Error: %s\n", error_buffer[0] ? error_buffer : "invalid arguments");
		fprintf(stderr, "Run 'cifrasync --help' for usage.\n");
		cs_config_free(&config);
		return CS_ERR_USAGE;
	}

	switch (options.command) {
		case CS_CMD_HELP:
			cs_print_help();
			result = CS_OK;
			break;
		case CS_CMD_VERSION:
			printf("cifrasync %s\n", cs_version());
			result = CS_OK;
			break;
		case CS_CMD_INIT:
			result = handle_init(&options);
			break;
		case CS_CMD_LIST:
			result = handle_list(&options);
			break;
		case CS_CMD_BACKUP:
			result = handle_backup(&options);
			break;
		case CS_CMD_RESTORE:
			result = handle_restore(&options);
			break;
		case CS_CMD_VERIFY:
			result = handle_verify(&options);
			break;
		case CS_CMD_PRUNE:
			result = handle_prune(&options);
			break;
		case CS_CMD_SYNC:
			result = handle_sync(&options);
			break;
		default:
			fprintf(stderr, "Unknown command.\n");
			result = CS_ERR_USAGE;
			break;
	}

	cs_config_free(&config);
	return result;
}
