#ifndef CIFRASYNC_CLI_PARSER_H
#define CIFRASYNC_CLI_PARSER_H

#include <stddef.h>

typedef enum {
	CS_CMD_NONE = 0,
	CS_CMD_HELP,
	CS_CMD_VERSION,
	CS_CMD_INIT,
	CS_CMD_BACKUP,
	CS_CMD_LIST,
	CS_CMD_RESTORE,
	CS_CMD_VERIFY,
	CS_CMD_PRUNE,
	CS_CMD_SYNC
} cs_command_t;

typedef struct {
	cs_command_t command;
	const char *repo;
	const char *source;
	const char *snapshot;
	const char *output;
	const char *remote;
	const char *label;
	const char *keep_last;
	const char *older_than;
	const char *include_file;
	const char *exclude_file;
	int dry_run;
	int compress;
	int encrypt;
} cs_cli_options_t;

void cs_cli_options_init(cs_cli_options_t *options);
const char *cs_command_to_string(cs_command_t command);
int cs_parse_cli(int argc, char **argv, cs_cli_options_t *options, char *error_buffer, size_t error_buffer_size);

#endif
