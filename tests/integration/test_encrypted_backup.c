#include "test_support.h"

static int cs_it_create_encrypted_source(const char *root) {
	if (cs_it_write_relative_text_file(root, "secret.txt", "this is a secret message\n") != 0) {
		return -1;
	}
	if (cs_it_write_relative_text_file(root, "sub/config.ini", "key=value\n") != 0) {
		return -1;
	}
	return 0;
}

int cs_integration_test_encrypted_backup(void) {
	char base[CS_IT_PATH_CAP];
	char source_root[CS_IT_PATH_CAP];
	char repo_root[CS_IT_PATH_CAP];
	char restore_root[CS_IT_PATH_CAP];
	char snapshot_stem[CS_IT_PATH_CAP];
	cs_repo_t repo_meta;
	const char *passphrase = "test-encryption-passphrase-123";

	if (cs_it_make_temp_root("encrypted_backup", base, sizeof(base)) != 0) {
		return cs_it_fail("unable to create temp root");
	}
	if (cs_it_join_path(source_root, sizeof(source_root), base, "source") != 0 ||
		cs_it_join_path(repo_root, sizeof(repo_root), base, "repo") != 0 ||
		cs_it_join_path(restore_root, sizeof(restore_root), base, "restore") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to build test paths");
	}

	if (cs_it_create_encrypted_source(source_root) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to create backup source tree");
	}
	if (cs_repo_init(repo_root, &repo_meta) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo initialization failed");
	}

	/* Backup with encryption=1 and programmatic passphrase */
	if (cs_engine_backup(source_root, repo_root, 0, 0, 1, "integration-encrypted-backup", NULL, 0U, NULL, 0U, passphrase) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("encrypted backup failed");
	}
	if (cs_repo_validate(repo_root) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repository validation failed after encrypted backup");
	}
	if (cs_it_find_latest_snapshot_stem(repo_root, snapshot_stem, sizeof(snapshot_stem)) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to locate snapshot artifact");
	}

	/* Restore with correct passphrase must succeed */
	if (cs_engine_restore(repo_root, snapshot_stem, restore_root, passphrase) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("restore with correct passphrase failed");
	}

	{
		char original_secret[CS_IT_PATH_CAP];
		char restored_secret[CS_IT_PATH_CAP];
		char original_config[CS_IT_PATH_CAP];
		char restored_config[CS_IT_PATH_CAP];

		if (cs_it_join_path(original_secret, sizeof(original_secret), source_root, "secret.txt") != 0 ||
			cs_it_join_path(restored_secret, sizeof(restored_secret), restore_root, "secret.txt") != 0 ||
			cs_it_join_path(original_config, sizeof(original_config), source_root, "sub/config.ini") != 0 ||
			cs_it_join_path(restored_config, sizeof(restored_config), restore_root, "sub/config.ini") != 0) {
			cs_it_remove_tree(base);
			return cs_it_fail("unable to build compare paths");
		}

		if (cs_it_compare_files(original_secret, restored_secret) != 0 ||
			cs_it_compare_files(original_config, restored_config) != 0) {
			cs_it_remove_tree(base);
			return cs_it_fail("encrypted restore files do not match source");
		}
	}

	/* Verify with correct passphrase must succeed */
	if (cs_engine_verify(repo_root, passphrase) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("verify with correct passphrase failed");
	}

	cs_it_remove_tree(base);
	return 0;
}
