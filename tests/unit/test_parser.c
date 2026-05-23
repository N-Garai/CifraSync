#include "../integration/test_support.h"

#include "cli/parser.h"

int cs_unit_test_parser(void) {
	char *argv[] = {
		"cifrasync",
		"backup",
		"--source",
		"source_dir",
		"--repo",
		"repo_dir",
		"--dry-run",
		"--compress",
		"--encrypt",
		"--label",
		"nightly"
	};
	cs_cli_options_t options;
	char error_buffer[128];

	cs_cli_options_init(&options);
	error_buffer[0] = '\0';
	if (cs_parse_cli((int)(sizeof(argv) / sizeof(argv[0])), argv, &options, error_buffer, sizeof(error_buffer)) != 0) {
		return cs_it_fail("parser rejected a valid backup command");
	}
	if (options.command != CS_CMD_BACKUP || options.dry_run != 1 || options.compress != 1 || options.encrypt != 1) {
		return cs_it_fail("parser did not populate boolean flags correctly");
	}
	if (options.source == NULL || strcmp(options.source, "source_dir") != 0 || options.repo == NULL || strcmp(options.repo, "repo_dir") != 0) {
		return cs_it_fail("parser did not capture required values");
	}
	if (options.label == NULL || strcmp(options.label, "nightly") != 0) {
		return cs_it_fail("parser did not capture label");
	}

	{
		char *bad_argv[] = {"cifrasync", "backup", "--unknown"};
		cs_cli_options_init(&options);
		error_buffer[0] = '\0';
		if (cs_parse_cli((int)(sizeof(bad_argv) / sizeof(bad_argv[0])), bad_argv, &options, error_buffer, sizeof(error_buffer)) == 0) {
			return cs_it_fail("parser accepted an unknown option");
		}
	}

	return 0;
}
