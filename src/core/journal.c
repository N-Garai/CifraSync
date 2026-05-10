#include "core/journal.h"
#include "common/path.h"
#include "common/log.h"
#include <stdio.h>
#include <string.h>

static char journal_path[1024];

static const char *make_journal_path(const char *repo_path) {
    snprintf(journal_path, sizeof(journal_path), "%s/.cifrasync_journal", repo_path);
    return journal_path;
}

int cs_journal_append(const char *repo_path, const char *record) {
    if (!repo_path || !record) return -1;
    const char *p = make_journal_path(repo_path);
    FILE *f = fopen(p, "a");
    if (!f) {
        CS_LOG_ERROR("journal: failed open %s", p);
        return -1;
    }
    fprintf(f, "%s\n", record);
    fclose(f);
    return 0;
}

int cs_journal_flush(const char *repo_path) {
    /* no-op for file-backed append writes */
    (void)repo_path;
    return 0;
}

int cs_journal_replay(const char *repo_path, int (*cb)(const char *line, void *ctx), void *ctx) {
    if (!repo_path || !cb) return -1;
    const char *p = make_journal_path(repo_path);
    FILE *f = fopen(p, "r");
    if (!f) return -1;
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        size_t l = strlen(buf);
        if (l && buf[l-1] == '\n') buf[l-1] = '\0';
        if (cb(buf, ctx) != 0) break;
    }
    fclose(f);
    return 0;
}
