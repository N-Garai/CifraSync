#include "../integration/test_support.h"

#include "storage/repo.h"
#include "storage/snapshot_store.h"

int cs_unit_test_snapshot_store(void) {
	char base[CS_IT_PATH_CAP];
	char repo_root[CS_IT_PATH_CAP];
	cs_repo_t repo_meta;
	cs_snapshot_store_t *store;
	cs_snapshot_t first_snapshot;
	cs_snapshot_t second_snapshot;
	cs_snapshot_t latest_snapshot;
	cs_snapshot_t *snapshot_list = NULL;
	size_t snapshot_count = 0U;

	if (cs_it_make_temp_root("snapshot_store", base, sizeof(base)) != 0) {
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

	store = cs_snapshot_store_open(repo_root);
	if (store == NULL) {
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot store open failed");
	}
	if (cs_snapshot_store_create(store, "C:/source/one", "first", &first_snapshot) != 0) {
		cs_snapshot_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("first snapshot create failed");
	}
	Sleep(1100);
	if (cs_snapshot_store_create(store, "C:/source/two", "second", &second_snapshot) != 0) {
		cs_snapshot_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("second snapshot create failed");
	}
	if (cs_snapshot_store_count(store) != 2UL) {
		cs_snapshot_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot store count mismatch");
	}
	if (cs_snapshot_store_list(store, &snapshot_list, &snapshot_count) != 0 || snapshot_count != 2U) {
		cs_snapshot_store_close(store);
		free(snapshot_list);
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot list failed");
	}
	free(snapshot_list);
	if (cs_snapshot_store_latest(store, &latest_snapshot) != 0 || strcmp(latest_snapshot.id, second_snapshot.id) != 0) {
		cs_snapshot_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("latest snapshot mismatch");
	}
	if (cs_snapshot_store_get(store, first_snapshot.id, &latest_snapshot) != 0 || strcmp(latest_snapshot.id, first_snapshot.id) != 0) {
		cs_snapshot_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot get failed");
	}
	if (cs_snapshot_store_delete(store, first_snapshot.id) != 0 || cs_snapshot_store_count(store) != 1UL) {
		cs_snapshot_store_close(store);
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot delete failed");
	}

	cs_snapshot_store_close(store);
	cs_it_remove_tree(base);
	return 0;
}
