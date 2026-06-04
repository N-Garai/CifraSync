#include "cli/commands.h"
#include "cli/parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define RST "\033[0m"
#define BLD "\033[1m"
#define DIM "\033[2m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YLW "\033[33m"
#define BLU "\033[34m"
#define MAG "\033[35m"
#define CYN "\033[36m"
#define WHT "\033[37m"

static void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

static void print_banner(void) {
    printf(BLD CYN "\n");
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║        " RST BLD WHT "C I F R A S Y N C" RST CYN "          ║\n");
    printf("  ║    " RST DIM "Encrypted Incremental Backup & Sync" RST CYN "   ║\n");
    printf("  ╚══════════════════════════════════════════╝\n");
    printf(RST "\n");
}

static void print_status_bar(const char *repo) {
    printf("  " RST BLD BLU "\u25C9" RST " repo: " GRN "%s" RST "\n\n", repo);
}

static void print_menu(void) {
    printf("  " BLD CYN "Commands:" RST "\n");
    printf("\n");
    printf("    " BLD GRN " 1" RST "  " GRN "\u2191" RST "  Init       " DIM "Initialize a repository" RST "\n");
    printf("    " BLD GRN " 2" RST "  " GRN "\u21E7" RST "  Backup     " DIM "Create an incremental backup" RST "\n");
    printf("    " BLD GRN " 3" RST "  " GRN "\u2630" RST "  List       " DIM "List snapshots" RST "\n");
    printf("    " BLD GRN " 4" RST "  " GRN "\u21E9" RST "  Restore    " DIM "Restore from a snapshot" RST "\n");
    printf("    " BLD GRN " 5" RST "  " GRN "\u2713" RST "  Verify     " DIM "Verify stored data integrity" RST "\n");
    printf("    " BLD GRN " 6" RST "  " GRN "\u2726" RST "  Prune      " DIM "Remove old snapshots" RST "\n");
    printf("    " BLD GRN " 7" RST "  " GRN "\u21C4" RST "  Sync       " DIM "Synchronize with remote" RST "\n");
    printf("    " BLD GRN " 8" RST "  " GRN "?" RST "   Help       " DIM "Show detailed help" RST "\n");
    printf("    " BLD RED " 9" RST "  " RED "\u2716" RST "  Exit       " DIM "Quit CifraSync" RST "\n");
    printf("\n");
}

static void prompt_str(const char *label, const char *def, char *out, size_t out_size) {
    char buf[512];
    if (def && def[0]) {
        printf("  " BLU "\u25B6" RST " %s " DIM "[%s]" RST ": ", label, def);
    } else {
        printf("  " BLU "\u25B6" RST " %s: ", label);
    }
    if (!fgets(buf, sizeof(buf), stdin)) { out[0] = '\0'; return; }
    buf[strcspn(buf, "\n")] = '\0';
    if (buf[0]) {
        strncpy(out, buf, out_size - 1);
        out[out_size - 1] = '\0';
    } else if (def) {
        strncpy(out, def, out_size - 1);
        out[out_size - 1] = '\0';
    } else {
        out[0] = '\0';
    }
}

static int prompt_yesno(const char *label, int def) {
    char buf[16];
    printf("  " BLU "\u25B6" RST " %s " BLD "[%c/%c]" RST ": ", label, def ? 'Y' : 'y', def ? 'n' : 'N');
    if (!fgets(buf, sizeof(buf), stdin)) return def;
    if (buf[0] == 'y' || buf[0] == 'Y') return 1;
    if (buf[0] == 'n' || buf[0] == 'N') return 0;
    return def;
}

int main_interactive(void) {
    char repo[256] = "test_repo";
    int last_exit = 0;

    clear_screen();
    print_banner();
    printf("  " DIM "   Version 0.1.0  |  Type a number to run a command" RST "\n\n");

    prompt_str("Repository path", "test_repo", repo, sizeof(repo));
    clear_screen();

    for (;;) {
        char *argv[32];
        int argc = 0;
        char input[512];
        char source[256] = "", snapshot[256] = "", out[256] = "", remote[256] = "";
        char include_file[256] = "", exclude_file[256] = "";
        char older_than[16] = "", label[64] = "", source_file[256] = "";
        int compress = 0, encrypt = 0, dry_run = 0;

        clear_screen();
        print_banner();
        print_status_bar(repo);

        if (last_exit != 0) {
            printf("  " RED "\u2716" RST " Last command exited with code " RED "%d" RST "\n\n", last_exit);
        } else {
            printf("  " GRN "\u2714" RST " Ready\n\n");
        }

        print_menu();
        printf("  " BLD "Your choice" RST " " DIM "(1-9)" RST ": ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) break;
        int choice = atoi(input);
        last_exit = 0;

        if (choice == 9) {
            clear_screen();
            print_banner();
            printf("\n  " GRN "\u2728" RST " Thank you for using " BLD "CifraSync" RST "!\n");
            printf("  " DIM "Come back anytime." RST "\n\n");
            break;
        }

        switch (choice) {
            case 1:
                printf("\n  " CYN "\u2192" RST " Initializing repository...\n\n");
                argv[argc++] = "cifrasync";
                argv[argc++] = "init";
                argv[argc++] = "--repo"; argv[argc++] = repo;
                break;

            case 2:
                printf("\n");
                prompt_str("Source path", NULL, source, sizeof(source));
                if (!source[0]) { printf("  " RED "\u2716" RST " Source path required.\n"); last_exit = 1; continue; }
                prompt_str("Label", NULL, label, sizeof(label));
                prompt_str("Include file", NULL, include_file, sizeof(include_file));
                prompt_str("Exclude file", NULL, exclude_file, sizeof(exclude_file));
                compress = prompt_yesno("Compress", 0);
                encrypt = prompt_yesno("Encrypt", 0);
                dry_run = prompt_yesno("Dry run", 0);
                printf("\n");
                argv[argc++] = "cifrasync"; argv[argc++] = "backup";
                argv[argc++] = "--source"; argv[argc++] = source;
                argv[argc++] = "--repo";   argv[argc++] = repo;
                if (include_file[0]) { argv[argc++] = "--include-file"; argv[argc++] = include_file; }
                if (exclude_file[0]) { argv[argc++] = "--exclude-file"; argv[argc++] = exclude_file; }
                if (label[0]) { argv[argc++] = "--label"; argv[argc++] = label; }
                if (compress) { argv[argc++] = "--compress"; }
                if (encrypt) { argv[argc++] = "--encrypt"; }
                if (dry_run) { argv[argc++] = "--dry-run"; }
                break;

            case 3:
                printf("\n");
                argv[argc++] = "cifrasync"; argv[argc++] = "list";
                argv[argc++] = "--repo"; argv[argc++] = repo;
                break;

            case 4:
                printf("\n");
                prompt_str("Snapshot ID", NULL, snapshot, sizeof(snapshot));
                if (!snapshot[0]) { printf("  " RED "\u2716" RST " Snapshot ID required.\n"); last_exit = 1; continue; }
                prompt_str("Output path", NULL, out, sizeof(out));
                if (!out[0]) { printf("  " RED "\u2716" RST " Output path required.\n"); last_exit = 1; continue; }
                prompt_str("Source file (single-file)", NULL, source_file, sizeof(source_file));
                printf("\n");
                argv[argc++] = "cifrasync"; argv[argc++] = "restore";
                argv[argc++] = "--repo"; argv[argc++] = repo;
                argv[argc++] = "--snapshot"; argv[argc++] = snapshot;
                argv[argc++] = "--out"; argv[argc++] = out;
                if (source_file[0]) { argv[argc++] = "--source-file"; argv[argc++] = source_file; }
                break;

            case 5:
                printf("\n  " CYN "\u2192" RST " Verifying repository integrity...\n\n");
                argv[argc++] = "cifrasync"; argv[argc++] = "verify";
                argv[argc++] = "--repo"; argv[argc++] = repo;
                break;

            case 6:
                printf("\n");
                prompt_str("Keep last N snapshots", "7", input, sizeof(input));
                prompt_str("Older than N days", NULL, older_than, sizeof(older_than));
                printf("\n");
                argv[argc++] = "cifrasync"; argv[argc++] = "prune";
                argv[argc++] = "--repo"; argv[argc++] = repo;
                if (input[0]) { argv[argc++] = "--keep-last"; argv[argc++] = input; }
                if (older_than[0]) { argv[argc++] = "--older-than"; argv[argc++] = older_than; }
                break;

            case 7:
                printf("\n");
                prompt_str("Remote host:port", NULL, remote, sizeof(remote));
                if (!remote[0]) { printf("  " RED "\u2716" RST " Remote address required.\n"); last_exit = 1; continue; }
                argv[argc++] = "cifrasync"; argv[argc++] = "sync";
                argv[argc++] = "--repo"; argv[argc++] = repo;
                argv[argc++] = "--remote"; argv[argc++] = remote;
                break;

            case 8:
                printf("\n");
                cs_print_help();
                printf("\n  " DIM "Press Enter to continue..." RST);
                fgets(input, sizeof(input), stdin);
                continue;

            default:
                printf("\n  " RED "\u2716" RST " Invalid choice: " BLD "%d" RST "\n", choice);
                printf("  " DIM "Press Enter to continue..." RST);
                fgets(input, sizeof(input), stdin);
                continue;
        }

        argv[argc] = NULL;
        if (argc) {
            printf("\n");
            fflush(stdout);
            last_exit = cs_run(argc, argv);
            printf("\n  " DIM "Command finished (exit: %d)" RST "\n", last_exit);
            printf("  " DIM "Press Enter to continue..." RST);
            fgets(input, sizeof(input), stdin);
        }
    }
    return 0;
}
