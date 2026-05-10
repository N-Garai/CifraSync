#include "core/engine.h"

#include "common/log.h"
#include "core/journal.h"
#include "core/planner.h"
#include <stdio.h>
#include <string.h>

int cs_engine_init(void) {
	CS_LOG_INFO("engine: init called");
	return 0;
}

int cs_engine_backup(const char *source_path, const char *repo_path, int dry_run, int compress, int encrypt, const char *label) {
	if (source_path == NULL || repo_path == NULL) {
		CS_LOG_ERROR("engine: backup requires source and repo");
		return -1;
	}

	CS_LOG_INFO("engine: starting backup source=%s repo=%s dry_run=%d compress=%d encrypt=%d label=%s",
				source_path, repo_path, dry_run, compress, encrypt, label ? label : "");

	cs_plan_t plan;
	if (cs_planner_create_plan(source_path, &plan) != 0) {
		CS_LOG_WARN("engine: planner failed for %s", source_path);
	} else {
		CS_LOG_INFO("engine: plan files=%zu bytes=%zu", plan.file_count, plan.total_bytes);
		cs_planner_free(&plan);
	}

	/* example journal append: record start */
	cs_journal_append(repo_path, "BACKUP START");
	cs_journal_flush(repo_path);

	CS_LOG_INFO("engine: backup complete (simulated)");
	return 0;
}

int cs_engine_restore(const char *repo_path, const char *snapshot_id, const char *out_path) {
	if (repo_path == NULL || snapshot_id == NULL || out_path == NULL) {
		CS_LOG_ERROR("engine: restore requires repo, snapshot and out path");
		return -1;
	}
	CS_LOG_INFO("engine: restoring snapshot=%s to %s (repo=%s)", snapshot_id, out_path, repo_path);
	return 0;
}
