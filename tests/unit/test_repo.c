#include "../integration/test_support.h"

#include "storage/repo.h"

int cs_unit_test_repo(void) {
	char base[CS_IT_PATH_CAP];
	char repo_root[CS_IT_PATH_CAP];
	cs_repo_t repo_meta;
	cs_repo_t loaded;

	if (cs_it_make_temp_root("repo_unit", base, sizeof(base)) != 0) {
		return cs_it_fail("unable to create temp root");
	}
	if (cs_it_join_path(repo_root, sizeof(repo_root), base, "repo") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to build repo path");
	}
	if (cs_repo_init(repo_root, &repo_meta) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo initialization failed");
	}
	if (!cs_repo_exists(repo_root)) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo should exist after init");
	}
	if (cs_repo_validate(repo_root) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo validation failed");
	}
	if (cs_repo_load(repo_root, &loaded) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo load failed");
	}
	if (strcmp(loaded.version, cs_repo_version()) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("loaded repo version mismatch");
	}
	if (cs_repo_save(&loaded) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo save failed");
	}

	cs_it_remove_tree(base);
	return 0;
}
