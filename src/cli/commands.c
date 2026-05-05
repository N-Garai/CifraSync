#include "cli/commands.h"
#include "cli/parser.h"

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
	puts("  list     --repo PATH");
	puts("  restore  --repo PATH --snapshot ID --out PATH [--source-file PATH]");
	puts("  verify   --repo PATH");
	puts("  prune    --repo PATH [--keep-last N] [--older-than DAYS]");
	puts("  sync     --repo PATH --remote HOST:PORT");
}

const char *cs_version(void) {
	return k_version;
}

void cs_cli_options_init(cs_cli_options_t *options) {
	if (options == NULL) {
		return;
	}
	memset(options, 0, sizeof(*options));
	options->command = CS_CMD_NONE;
}

const char *cs_command_to_string(cs_command_t command) {
	switch (command) {
		case CS_CMD_HELP: return "help";
		case CS_CMD_VERSION: return "version";
		case CS_CMD_INIT: return "init";
		case CS_CMD_BACKUP: return "backup";
		case CS_CMD_LIST: return "list";
		case CS_CMD_RESTORE: return "restore";
		case CS_CMD_VERIFY: return "verify";
		case CS_CMD_PRUNE: return "prune";
		case CS_CMD_SYNC: return "sync";
		default: return "unknown";
	}
}

static cs_command_t parse_command_token(const char *token) {
	if (token == NULL) {
		return CS_CMD_NONE;
	}
	if (strcmp(token, "-h") == 0 || strcmp(token, "--help") == 0 || strcmp(token, "help") == 0) {
		return CS_CMD_HELP;
	}
	if (strcmp(token, "-V") == 0 || strcmp(token, "--version") == 0 || strcmp(token, "version") == 0) {
		return CS_CMD_VERSION;
	}
	if (strcmp(token, "init") == 0) return CS_CMD_INIT;
	if (strcmp(token, "backup") == 0) return CS_CMD_BACKUP;
	if (strcmp(token, "list") == 0) return CS_CMD_LIST;
	if (strcmp(token, "restore") == 0) return CS_CMD_RESTORE;
	if (strcmp(token, "verify") == 0) return CS_CMD_VERIFY;
	if (strcmp(token, "prune") == 0) return CS_CMD_PRUNE;
	if (strcmp(token, "sync") == 0) return CS_CMD_SYNC;
	return CS_CMD_NONE;
}

static int set_error(char *error_buffer, size_t error_buffer_size, const char *message) {
	if (error_buffer != NULL && error_buffer_size > 0) {
		snprintf(error_buffer, error_buffer_size, "%s", message);
	}
	return -1;
}

static int require_value(const char *option_name, const char *value, char *error_buffer, size_t error_buffer_size) {
	char message[128];
	if (!is_required_missing(value)) {
		return 0;
	}
	snprintf(message, sizeof(message), "%s requires a value", option_name);
	return set_error(error_buffer, error_buffer_size, message);
}

int cs_parse_cli(int argc, char **argv, cs_cli_options_t *options, char *error_buffer, size_t error_buffer_size) {
	int i;
	if (options == NULL) {
		return set_error(error_buffer, error_buffer_size, "options pointer is NULL");
	}
	cs_cli_options_init(options);

	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		options->command = CS_CMD_HELP;
		return 0;
	}

	options->command = parse_command_token(argv[1]);
	if (options->command == CS_CMD_NONE) {
		return set_error(error_buffer, error_buffer_size, "unknown command");
	}

	for (i = 2; i < argc; ++i) {
		const char *arg = argv[i];
		const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;

		if (arg == NULL) {
			continue;
		}
		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			options->command = CS_CMD_HELP;
			return 0;
		}
		if (strcmp(arg, "--version") == 0 || strcmp(arg, "-V") == 0) {
			options->command = CS_CMD_VERSION;
			return 0;
		}
		if (strcmp(arg, "--dry-run") == 0) {
			options->dry_run = 1;
			continue;
		}
		if (strcmp(arg, "--compress") == 0) {
			options->compress = 1;
			continue;
		}
		if (strcmp(arg, "--encrypt") == 0) {
			options->encrypt = 1;
			continue;
		}

		if (strcmp(arg, "--repo") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->repo = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--source") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->source = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--snapshot") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->snapshot = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--out") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->output = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--remote") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->remote = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--label") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->label = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--keep-last") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->keep_last = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--older-than") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->older_than = value;
			++i;
			continue;
		}

		return set_error(error_buffer, error_buffer_size, "unknown option");
	}

	return 0;
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

static int handle_not_implemented(const cs_cli_options_t *options) {
	const char *name = cs_command_to_string(options->command);

	switch (options->command) {
		case CS_CMD_BACKUP:
			if (is_required_missing(options->repo) || is_required_missing(options->source)) {
				fprintf(stderr, "backup requires --source PATH and --repo PATH\n");
				return CS_ERR_USAGE;
			}
			break;
		case CS_CMD_RESTORE:
			if (is_required_missing(options->repo) || is_required_missing(options->snapshot) || is_required_missing(options->output)) {
				fprintf(stderr, "restore requires --repo PATH --snapshot ID --out PATH\n");
				return CS_ERR_USAGE;
			}
			break;
		case CS_CMD_VERIFY:
		case CS_CMD_PRUNE:
		case CS_CMD_SYNC:
			if (is_required_missing(options->repo)) {
				fprintf(stderr, "%s requires --repo PATH\n", name);
				return CS_ERR_USAGE;
			}
			break;
		default:
			break;
	}

	fprintf(stderr, "%s is parsed correctly but not yet wired to the storage engine.\n", name);
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
		case CS_CMD_RESTORE:
		case CS_CMD_VERIFY:
		case CS_CMD_PRUNE:
		case CS_CMD_SYNC:
			return handle_not_implemented(&options);
		default:
			fprintf(stderr, "Unknown command.\n");
			return CS_ERR_USAGE;
	}
}
