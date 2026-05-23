#include "../integration/test_support.h"

#include "storage/index_store.h"
#include "storage/repo.h"

int cs_unit_test_index_store(void) {
	char base[CS_IT_PATH_CAP];
	char repo_root[CS_IT_PATH_CAP];
	cs_repo_t repo_meta;
	cs_index_store_t *store;
	char location[256];

	if (cs_it_make_temp_root("index_store", base, sizeof(base)) != 0) {
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

	store = cs_index_store_open(repo_root);
	if (store == NULL) {
		cs_it_remove_tree(base);
		return cs_it_fail("index store open failed");
	}
	if (cs_index_store_insert(store, "abc123", "chunks/ab/abc123") != 0) {
		cs_index_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("index insert failed");
	}
	if (!cs_index_store_contains(store, "abc123")) {
		cs_index_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("index should contain inserted hash");
	}
	if (cs_index_store_get(store, "abc123", location, sizeof(location)) != 0 || strcmp(location, "chunks/ab/abc123") != 0) {
		cs_index_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("index get failed");
	}
	if (cs_index_store_flush(store) != 0) {
		cs_index_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("index flush failed");
	}
	cs_index_store_close(store);

	store = cs_index_store_open(repo_root);
	if (store == NULL) {
		cs_it_remove_tree(base);
		return cs_it_fail("index store reopen failed");
	}
	if (cs_index_store_count(store) != 1UL) {
		cs_index_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("index count mismatch after reload");
	}
	if (cs_index_store_remove(store, "abc123") != 0 || cs_index_store_contains(store, "abc123")) {
		cs_index_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("index remove failed");
	}

	cs_index_store_close(store);
	cs_it_remove_tree(base);
	return 0;
}
