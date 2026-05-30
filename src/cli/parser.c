#include "cli/parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int is_required_missing(const char *value) {
	return value == NULL || value[0] == '\0';
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
		if (strcmp(arg, "--include-file") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->include_file = value;
			++i;
			continue;
		}
		if (strcmp(arg, "--exclude-file") == 0) {
			if (require_value(arg, value, error_buffer, error_buffer_size) != 0) {
				return -1;
			}
			options->exclude_file = value;
			++i;
			continue;
		}

		return set_error(error_buffer, error_buffer_size, "unknown option");
	}

	return 0;
}
