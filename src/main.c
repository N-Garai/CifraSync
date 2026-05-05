#include <stdio.h>
#include <string.h>

typedef enum {
	CMD_NONE = 0,
	CMD_HELP,
	CMD_VERSION,
	CMD_INIT,
	CMD_BACKUP,
	CMD_LIST,
	CMD_RESTORE,
	CMD_VERIFY,
	CMD_PRUNE,
	CMD_SYNC
} cs_command_t;

static const char *k_version = "0.1.0";

static void print_banner(void) {
	puts("CifraSync - Encrypted Incremental Backup & Sync");
}

static void print_usage(void) {
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
}

static cs_command_t parse_command(const char *arg) {
	if (arg == NULL) {
		return CMD_NONE;
	}

	if (strcmp(arg, "help") == 0 || strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
		return CMD_HELP;
	}
	if (strcmp(arg, "version") == 0 || strcmp(arg, "--version") == 0 || strcmp(arg, "-V") == 0) {
		return CMD_VERSION;
	}
	if (strcmp(arg, "init") == 0) {
		return CMD_INIT;
	}
	if (strcmp(arg, "backup") == 0) {
		return CMD_BACKUP;
	}
	if (strcmp(arg, "list") == 0) {
		return CMD_LIST;
	}
	if (strcmp(arg, "restore") == 0) {
		return CMD_RESTORE;
	}
	if (strcmp(arg, "verify") == 0) {
		return CMD_VERIFY;
	}
	if (strcmp(arg, "prune") == 0) {
		return CMD_PRUNE;
	}
	if (strcmp(arg, "sync") == 0) {
		return CMD_SYNC;
	}

	return CMD_NONE;
}

static int run_placeholder_command(cs_command_t cmd) {
	switch (cmd) {
		case CMD_INIT:
			puts("[TODO] init command implementation");
			return 0;
		case CMD_BACKUP:
			puts("[TODO] backup command implementation");
			return 0;
		case CMD_LIST:
			puts("[TODO] list command implementation");
			return 0;
		case CMD_RESTORE:
			puts("[TODO] restore command implementation");
			return 0;
		case CMD_VERIFY:
			puts("[TODO] verify command implementation");
			return 0;
		case CMD_PRUNE:
			puts("[TODO] prune command implementation");
			return 0;
		case CMD_SYNC:
			puts("[TODO] sync command implementation");
			return 0;
		default:
			return 2;
	}
}

int main(int argc, char **argv) {
	cs_command_t cmd;

	if (argc < 2) {
		print_usage();
		return 1;
	}

	cmd = parse_command(argv[1]);

	if (cmd == CMD_HELP) {
		print_usage();
		return 0;
	}

	if (cmd == CMD_VERSION) {
		printf("cifrasync %s\n", k_version);
		return 0;
	}

	if (cmd == CMD_NONE) {
		fprintf(stderr, "Unknown command: %s\n", argv[1]);
		fprintf(stderr, "Run 'cifrasync --help' to see available commands.\n");
		return 2;
	}

	return run_placeholder_command(cmd);
}
