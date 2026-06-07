#include "test_support.h"

static int cs_it_create_backup_source(const char *root) {
	if (cs_it_write_relative_text_file(root, "alpha.txt", "alpha-one\nalpha-two\n") != 0) {
		return -1;
	}
	if (cs_it_write_relative_text_file(root, "nested/beta.txt", "beta-one\nbeta-two\n") != 0) {
		return -1;
	}
	if (cs_it_write_relative_text_file(root, "nested/deeper/gamma.txt", "gamma-one\n") != 0) {
		return -1;
	}
	return 0;
}

int cs_integration_test_backup_restore(void) {
	char base[CS_IT_PATH_CAP];
	char source_root[CS_IT_PATH_CAP];
	char repo_root[CS_IT_PATH_CAP];
	char restore_root[CS_IT_PATH_CAP];
	char snapshot_stem[CS_IT_PATH_CAP];
	cs_repo_t repo_meta;

	if (cs_it_make_temp_root("backup_restore", base, sizeof(base)) != 0) {
		return cs_it_fail("unable to create temp root");
	}
	if (cs_it_join_path(source_root, sizeof(source_root), base, "source") != 0 ||
		cs_it_join_path(repo_root, sizeof(repo_root), base, "repo") != 0 ||
		cs_it_join_path(restore_root, sizeof(restore_root), base, "restore") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to build test paths");
	}

	if (cs_it_create_backup_source(source_root) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to create backup source tree");
	}
	if (cs_repo_init(repo_root, &repo_meta) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo initialization failed");
	}
	if (cs_engine_backup(source_root, repo_root, 0, 0, 0, "integration-backup-restore", NULL, 0U, NULL, 0U) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("backup failed");
	}
	if (cs_repo_validate(repo_root) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repository validation failed after backup");
	}
	if (cs_it_find_latest_snapshot_stem(repo_root, snapshot_stem, sizeof(snapshot_stem)) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to locate snapshot artifact");
	}
	if (cs_engine_restore(repo_root, snapshot_stem, restore_root, "") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("restore failed");
	}

	if (cs_it_compare_files("" , "") == 0) {
		/* keep static analysis from complaining about the helper being unused in the happy path */
	}

	{
		char original_alpha[CS_IT_PATH_CAP];
		char restored_alpha[CS_IT_PATH_CAP];
		char original_beta[CS_IT_PATH_CAP];
		char restored_beta[CS_IT_PATH_CAP];
		char original_gamma[CS_IT_PATH_CAP];
		char restored_gamma[CS_IT_PATH_CAP];

		if (cs_it_join_path(original_alpha, sizeof(original_alpha), source_root, "alpha.txt") != 0 ||
			cs_it_join_path(restored_alpha, sizeof(restored_alpha), restore_root, "alpha.txt") != 0 ||
			cs_it_join_path(original_beta, sizeof(original_beta), source_root, "nested/beta.txt") != 0 ||
			cs_it_join_path(restored_beta, sizeof(restored_beta), restore_root, "nested/beta.txt") != 0 ||
			cs_it_join_path(original_gamma, sizeof(original_gamma), source_root, "nested/deeper/gamma.txt") != 0 ||
			cs_it_join_path(restored_gamma, sizeof(restored_gamma), restore_root, "nested/deeper/gamma.txt") != 0) {
			cs_it_remove_tree(base);
			return cs_it_fail("unable to build compare paths");
		}

		if (cs_it_compare_files(original_alpha, restored_alpha) != 0 ||
			cs_it_compare_files(original_beta, restored_beta) != 0 ||
			cs_it_compare_files(original_gamma, restored_gamma) != 0) {
			cs_it_remove_tree(base);
			return cs_it_fail("restored files do not match the source tree");
		}
	}

	cs_it_remove_tree(base);
	return 0;
}