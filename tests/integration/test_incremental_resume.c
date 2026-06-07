#include "test_support.h"

int cs_integration_test_incremental_resume(void) {
	char base[CS_IT_PATH_CAP];
	char source_root[CS_IT_PATH_CAP];
	char repo_root[CS_IT_PATH_CAP];
	char chunks_root[CS_IT_PATH_CAP];
	char snapshots_root[CS_IT_PATH_CAP];
	char snapshot_stem[CS_IT_PATH_CAP];
	unsigned long chunk_count_before = 0UL;
	unsigned long chunk_count_after = 0UL;
	unsigned long snapshot_count_before = 0UL;
	unsigned long snapshot_count_after = 0UL;
	cs_repo_t repo_meta;

	if (cs_it_make_temp_root("incremental_resume", base, sizeof(base)) != 0) {
		return cs_it_fail("unable to create temp root");
	}
	if (cs_it_join_path(source_root, sizeof(source_root), base, "source") != 0 ||
		cs_it_join_path(repo_root, sizeof(repo_root), base, "repo") != 0 ||
		cs_it_join_path(chunks_root, sizeof(chunks_root), repo_root, "chunks") != 0 ||
		cs_it_join_path(snapshots_root, sizeof(snapshots_root), repo_root, "snapshots") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to build test paths");
	}

	if (cs_it_write_relative_text_file(source_root, "payload.txt", "payload-one\npayload-two\n") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to create source tree");
	}
	if (cs_repo_init(repo_root, &repo_meta) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo initialization failed");
	}
	if (cs_engine_backup(source_root, repo_root, 0, 0, 0, "integration-incremental-1", NULL, 0U, NULL, 0U, NULL) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("first backup failed");
	}
	if (cs_it_count_files_recursive(chunks_root, NULL, &chunk_count_before) != 0 ||
		cs_it_count_files_recursive(snapshots_root, ".snapshot", &snapshot_count_before) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("failed to count backup artifacts after first run");
	}

	Sleep(1101);

	if (cs_engine_backup(source_root, repo_root, 0, 0, 0, "integration-incremental-2", NULL, 0U, NULL, 0U, NULL) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("second backup failed");
	}
	if (cs_it_count_files_recursive(chunks_root, NULL, &chunk_count_after) != 0 ||
		cs_it_count_files_recursive(snapshots_root, ".snapshot", &snapshot_count_after) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("failed to count backup artifacts after second run");
	}

	if (chunk_count_after != chunk_count_before) {
		cs_it_remove_tree(base);
		return cs_it_fail("unchanged backup created duplicate chunk files");
	}
	if (snapshot_count_after != snapshot_count_before + 1UL) {
		cs_it_remove_tree(base);
		return cs_it_fail("second backup did not create exactly one new snapshot");
	}
	if (cs_it_find_latest_snapshot_stem(repo_root, snapshot_stem, sizeof(snapshot_stem)) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to resolve latest snapshot stem");
	}

	cs_it_remove_tree(base);
	(void)snapshot_stem;
	return 0;
}