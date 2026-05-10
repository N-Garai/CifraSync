#ifndef CIFRASYNC_CORE_JOURNAL_H
#define CIFRASYNC_CORE_JOURNAL_H

#include <stddef.h>

/* Simple append-only journal helpers used by the engine. */
int cs_journal_append(const char *repo_path, const char *record);
int cs_journal_flush(const char *repo_path);
int cs_journal_replay(const char *repo_path, int (*cb)(const char *line, void *ctx), void *ctx);

#endif

