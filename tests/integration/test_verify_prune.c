#include "test_support.h"

static int cs_it_verify_manifest_chunks(const char *repo_root, const char *snapshot_stem) {
	char manifest_path[CS_IT_PATH_CAP];
	cs_manifest_t manifest;
	cs_chunk_store_t *chunk_store;
	int status;

	if (cs_it_snapshot_path_from_stem(repo_root, snapshot_stem, ".manifest", manifest_path, sizeof(manifest_path)) != 0) {
		return -1;
	}
	if (cs_manifest_load(manifest_path, &manifest) != 0) {
		return -1;
	}

	chunk_store = cs_chunk_store_open(repo_root);
	if (chunk_store == NULL) {
		cs_manifest_free(&manifest);
		return -1;
	}

	status = 0;
	for (size_t file_index = 0U; file_index < manifest.file_count && status == 0; ++file_index) {
		const cs_manifest_file_t *file = &manifest.files[file_index];
		for (size_t chunk_index = 0U; chunk_index < file->chunk_count && status == 0; ++chunk_index) {
			const cs_manifest_chunk_t *chunk = &file->chunks[chunk_index];
			unsigned char *chunk_data = NULL;
			size_t chunk_size = 0U;
			char hash_hex[CS_HASH_HEX_BUFSZ];

			if (cs_chunk_store_exists(chunk_store, chunk->hash_hex) == 0) {
				status = -1;
				break;
			}
			if (cs_chunk_store_get(chunk_store, chunk->hash_hex, &chunk_data, &chunk_size) != 0) {
				status = -1;
				break;
			}
			if (chunk_size != chunk->size || cs_hash_sha256_hex(chunk_data, chunk_size, hash_hex) != 0 || strcmp(hash_hex, chunk->hash_hex) != 0) {
				free(chunk_data);
				status = -1;
				break;
			}
			free(chunk_data);
		}
	}

	cs_chunk_store_close(chunk_store);
	cs_manifest_free(&manifest);
	return status;
}

int cs_integration_test_verify_prune(void) {
	char base[CS_IT_PATH_CAP];
	char source_root[CS_IT_PATH_CAP];
	char repo_root[CS_IT_PATH_CAP];
	char snapshot_stem[CS_IT_PATH_CAP];
	cs_repo_t repo_meta;
	cs_snapshot_store_t *snapshot_store;
	cs_snapshot_t first_snapshot;
	cs_snapshot_t latest_snapshot;

	if (cs_it_make_temp_root("verify_prune", base, sizeof(base)) != 0) {
		return cs_it_fail("unable to create temp root");
	}
	if (cs_it_join_path(source_root, sizeof(source_root), base, "source") != 0 ||
		cs_it_join_path(repo_root, sizeof(repo_root), base, "repo") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to build test paths");
	}

	if (cs_it_write_relative_text_file(source_root, "payload.txt", "verify-one\nverify-two\n") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to create source tree");
	}
	if (cs_repo_init(repo_root, &repo_meta) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("repo initialization failed");
	}
	if (cs_engine_backup(source_root, repo_root, 0, 0, 0, "integration-verify-prune", NULL, 0U, NULL, 0U) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("backup failed");
	}
	if (cs_it_find_latest_snapshot_stem(repo_root, snapshot_stem, sizeof(snapshot_stem)) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to locate snapshot stem");
	}
	if (cs_it_verify_manifest_chunks(repo_root, snapshot_stem) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("manifest verification failed");
	}

	snapshot_store = cs_snapshot_store_open(repo_root);
	if (snapshot_store == NULL) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to open snapshot store");
	}
	if (cs_snapshot_store_create(snapshot_store, source_root, "first", &first_snapshot) != 0) {
		cs_snapshot_store_close(snapshot_store);
		cs_it_remove_tree(base);
		return cs_it_fail("first snapshot creation failed");
	}
	Sleep(1100);
	if (cs_snapshot_store_create(snapshot_store, source_root, "second", &latest_snapshot) != 0) {
		cs_snapshot_store_close(snapshot_store);
		cs_it_remove_tree(base);
		return cs_it_fail("second snapshot creation failed");
	}

	if (cs_snapshot_store_count(snapshot_store) != 2UL) {
		cs_snapshot_store_close(snapshot_store);
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot store count should be two before prune");
	}
	if (cs_snapshot_store_latest(snapshot_store, &latest_snapshot) != 0) {
		cs_snapshot_store_close(snapshot_store);
		cs_it_remove_tree(base);
		return cs_it_fail("latest snapshot lookup failed");
	}
	if (strcmp(latest_snapshot.id, first_snapshot.id) == 0) {
		cs_snapshot_store_close(snapshot_store);
		cs_it_remove_tree(base);
		return cs_it_fail("latest snapshot should differ from the first snapshot");
	}
	if (cs_snapshot_store_delete(snapshot_store, first_snapshot.id) != 0) {
		cs_snapshot_store_close(snapshot_store);
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot prune delete failed");
	}
	if (cs_snapshot_store_count(snapshot_store) != 1UL) {
		cs_snapshot_store_close(snapshot_store);
		cs_it_remove_tree(base);
		return cs_it_fail("snapshot store count should be one after prune");
	}

	cs_snapshot_store_close(snapshot_store);
	cs_it_remove_tree(base);
	return 0;
}