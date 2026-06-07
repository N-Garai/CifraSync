#include "cli/commands.h"
#include "cli/parser.h"

#include "core/engine.h"
#include "common/path.h"
#include "storage/repo.h"
#include "net/server.h"
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
	puts("CifraSync is a zero-dependency encrypted incremental backup tool.");
	puts("It backs up ANY file type -- text, images, videos, binaries, executables --");
	puts("by reading raw bytes in binary mode, splitting into SHA-256 chunks,");
	puts("and optionally compressing (RLE) and encrypting (HMAC stream cipher).");
	puts("Deduplication ensures identical data across files/snapshots is stored once.");
	puts("");
	puts("Usage:");
	puts("  cifrasync <command> [options]");
	puts("");
	puts("Quick start:");
	puts("  cifrasync init --repo C:\\my_repo");
	puts("  cifrasync backup --source C:\\my_files --repo C:\\my_repo --compress");
	puts("  cifrasync list --repo C:\\my_repo");
	puts("  cifrasync restore --repo C:\\my_repo --snapshot <ID> --out C:\\restored");
	puts("");
	puts("Commands:");
	puts("  init       Create a repository with directory structure");
	puts("             (chunks/, snapshots/, index/, journal/, locks/)");
	puts("  backup     Scan source dir, chunk files (SHA-256, 1 MiB),");
	puts("             deduplicate, optionally compress & encrypt,");
	puts("             append snapshot. Works on any file type.");
	puts("  list       Show all snapshots with ID, timestamp, file count,");
	puts("             human-readable size, and label.");
	puts("  restore    Restore full snapshot or single file (--source-file).");
	puts("             Decompresses and decrypts automatically.");
	puts("  verify     Re-read every chunk, recompute SHA-256 hash,");
	puts("             report missing or corrupt chunks.");
	puts("  prune      Delete old snapshots (--keep-last N, --older-than DAYS)");
	puts("             and garbage-collect orphan chunks.");
	puts("  sync       Push snapshots + missing chunk data to a remote");
	puts("             CifraSync server. Only new chunks are transferred.");
	puts("  serve      Start TCP server to receive sync data from clients.");
	puts("             With --repo PATH, stores received data into repo.");
	puts("");
	puts("Global options:");
	puts("  -h, --help       Show this help");
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
	puts("  serve    --bind HOST:PORT [--repo PATH]");
	puts("");
	puts("Interactive mode:");
	puts("  Run cifrasync with no arguments to launch the TUI.");
	puts("  Choose a number (1-9) to run commands interactively.");
	puts("");
	puts("Key features:");
	puts("  * Any file type (binary mode, no text restrictions)");
	puts("  * SHA-256 chunk-level deduplication (1 MiB fixed chunks)");
	puts("  * RLE compression + HMAC stream cipher per chunk");
	puts("  * Atomic manifest writes (.tmp + rename) for crash safety");
	puts("  * Journal replay auto-resumes interrupted backups");
	puts("  * Cross-platform: Windows (MinGW), Linux, macOS");
	puts("  * Single binary, zero dependencies, ~200 KB");
	puts("");
	puts("Docs: see docs/ directory for full specification.");
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
	char passphrase[256] = "";

	if (is_required_missing(options->repo) || is_required_missing(options->snapshot) || is_required_missing(options->output)) {
		fprintf(stderr, "restore requires --repo PATH --snapshot ID --out PATH\n");
		return CS_ERR_USAGE;
	}

	printf("Enter decryption passphrase (leave blank if not encrypted): ");
	if (fgets(passphrase, sizeof(passphrase), stdin) != NULL) {
		size_t plen = strlen(passphrase);
		while (plen > 0U && (passphrase[plen - 1U] == '\n' || passphrase[plen - 1U] == '\r')) {
			passphrase[--plen] = '\0';
		}
	}

	if (options->source_file != NULL && options->source_file[0] != '\0') {
		if (cs_engine_restore_file(options->repo, options->snapshot, options->source_file, options->output, passphrase) != 0) {
			fprintf(stderr, "restore single file failed\n");
			return CS_ERR_INVALID;
		}
		printf("Single file restore complete.\n");
	} else {
		if (cs_engine_restore(options->repo, options->snapshot, options->output, passphrase) != 0) {
			fprintf(stderr, "restore failed\n");
			return CS_ERR_INVALID;
		}
		printf("Restore complete.\n");
	}
	return CS_OK;
}

static int handle_verify(const cs_cli_options_t *options) {
	char passphrase[256] = "";

	if (is_required_missing(options->repo)) {
		fprintf(stderr, "verify requires --repo PATH\n");
		return CS_ERR_USAGE;
	}

	printf("Enter decryption passphrase (leave blank if not encrypted): ");
	if (fgets(passphrase, sizeof(passphrase), stdin) != NULL) {
		size_t plen = strlen(passphrase);
		while (plen > 0U && (passphrase[plen - 1U] == '\n' || passphrase[plen - 1U] == '\r')) {
			passphrase[--plen] = '\0';
		}
	}

	if (cs_engine_verify(options->repo, passphrase) != 0) {
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

static int handle_serve(const cs_cli_options_t *options) {
	char host[256];
	unsigned short port = 0;
	const char *colon;
	void *ctx = NULL;

	if (is_required_missing(options->bind)) {
		fprintf(stderr, "serve requires --bind HOST:PORT\n");
		return CS_ERR_USAGE;
	}

	colon = strchr(options->bind, ':');
	if (colon == NULL) {
		fprintf(stderr, "serve: --bind must be HOST:PORT format\n");
		return CS_ERR_INVALID;
	}

	{
		size_t host_len = (size_t)(colon - options->bind);
		if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
		memcpy(host, options->bind, host_len);
		host[host_len] = '\0';
	}

	port = (unsigned short)atoi(colon + 1);
	if (port == 0) {
		fprintf(stderr, "serve: invalid port in '%s'\n", options->bind);
		return CS_ERR_INVALID;
	}

	if (options->repo != NULL && options->repo[0] != '\0') {
		ctx = (void *)options->repo;
	}

	printf("Starting CifraSync server on %s:%u (Ctrl+C to stop)\n", host, (unsigned)port);
	if (ctx != NULL) {
		printf("Repository: %s\n", (const char *)ctx);
	}
	fflush(stdout);

	if (cs_net_server_run(host, port, cs_net_server_sync_handler, ctx, 0) != 0) {
		fprintf(stderr, "serve: server failed: %s\n", cs_net_server_last_error());
		return CS_ERR_INVALID;
	}

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
		case CS_CMD_SERVE:
			result = handle_serve(&options);
			break;
		default:
			fprintf(stderr, "Unknown command.\n");
			result = CS_ERR_USAGE;
			break;
	}

	cs_config_free(&config);
	return result;
}
