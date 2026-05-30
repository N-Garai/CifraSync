#include "core/engine.h"

#include "common/constants.h"
#include "common/log.h"
#include "common/path.h"
#include "core/journal.h"
#include "delta/chunker.h"
#include "delta/manifest.h"
#include "fs/file_reader.h"
#include "fs/metadata.h"
#include "fs/scanner.h"
#include "storage/chunk_store.h"
#include "storage/index_store.h"
#include "storage/repo.h"
#include "storage/snapshot_store.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define cs_engine_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define cs_engine_mkdir(path) mkdir(path, 0755)
#endif

typedef struct cs_engine_backup_ctx {
	const char *source_root;
	cs_manifest_t *manifest;
	cs_chunk_store_t *chunk_store;
	cs_index_store_t *index_store;
	cs_chunker_t chunker;
	int dry_run;
	size_t file_count;
	unsigned long long total_bytes;
	unsigned long long total_chunks;
} cs_engine_backup_ctx_t;

typedef struct cs_engine_chunk_ctx {
	cs_engine_backup_ctx_t *backup;
	cs_manifest_file_t *file;
} cs_engine_chunk_ctx_t;

static int cs_engine_join(char *out, size_t out_size, const char *left, const char *right) {
	return cs_path_join(out, out_size, left, right);
}

static int cs_engine_safe_snapshot_component(const char *snapshot_id, char *out, size_t out_size) {
	size_t src_index;
	size_t dst_index;

	if (snapshot_id == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	for (src_index = 0U, dst_index = 0U; snapshot_id[src_index] != '\0'; ++src_index) {
		unsigned char ch = (unsigned char)snapshot_id[src_index];
		char mapped = (char)ch;

		if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '-' || ch == '_' || ch == '.')) {
			mapped = '-';
		}

		if (dst_index + 1U >= out_size) {
			return -1;
		}
		out[dst_index++] = mapped;
	}

	out[dst_index] = '\0';
	return 0;
}

static int cs_engine_snapshot_artifact_path(const char *repo_path, const char *snapshot_id, const char *suffix, char *out, size_t out_size) {
	char snapshots_dir[CS_PATH_CAP];
	char snapshot_base[CS_PATH_CAP];
	char snapshot_name[CS_PATH_CAP];

	if (cs_engine_safe_snapshot_component(snapshot_id, snapshot_name, sizeof(snapshot_name)) != 0) {
		return -1;
	}
	if (cs_engine_join(snapshots_dir, sizeof(snapshots_dir), repo_path, "snapshots") != 0) {
		return -1;
	}
	if (cs_engine_join(snapshot_base, sizeof(snapshot_base), snapshots_dir, snapshot_name) != 0) {
		return -1;
	}
	if (snprintf(out, out_size, "%s%s", snapshot_base, suffix) < 0 || strlen(out) + 1U > out_size) {
		return -1;
	}
	return 0;
}

static int cs_engine_mkdir_p(const char *path) {
	char normalized[CS_PATH_CAP];
	char current[CS_PATH_CAP];
	size_t length;
	size_t start;
	size_t index;

	if (path == NULL || path[0] == '\0') {
		return -1;
	}

	if (cs_path_normalize_copy(path, normalized, sizeof(normalized)) != 0) {
		return -1;
	}

	length = strlen(normalized);
	if (length == 0U || length >= sizeof(current)) {
		return -1;
	}

	memcpy(current, normalized, length + 1U);

#ifdef _WIN32
	start = (length >= 3U && current[1] == ':') ? 3U : 1U;
#else
	start = 1U;
#endif

	for (index = start; index < length; ++index) {
		if (current[index] == cs_path_separator()) {
			current[index] = '\0';
			if (current[0] != '\0' && strcmp(current, ".") != 0) {
				if (cs_engine_mkdir(current) != 0 && errno != EEXIST) {
					return -1;
				}
			}
			current[index] = cs_path_separator();
		}
	}

	if (current[0] != '\0' && strcmp(current, ".") != 0) {
		if (cs_engine_mkdir(current) != 0 && errno != EEXIST) {
			return -1;
		}
	}

	return 0;
}

static int cs_engine_parent_directory(const char *path, char *out, size_t out_size) {
	char normalized[CS_PATH_CAP];

	if (path == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	if (cs_path_normalize_copy(path, normalized, sizeof(normalized)) != 0) {
		return -1;
	}

	return cs_path_dirname(normalized, out, out_size);
}

static int cs_engine_make_relative_path(const char *source_root, const char *full_path, char *out, size_t out_size) {
	char normalized_root[CS_PATH_CAP];
	char normalized_full[CS_PATH_CAP];
	size_t root_length;

	if (source_root == NULL || full_path == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	if (cs_path_normalize_copy(source_root, normalized_root, sizeof(normalized_root)) != 0) {
		return -1;
	}
	if (cs_path_normalize_copy(full_path, normalized_full, sizeof(normalized_full)) != 0) {
		return -1;
	}

	root_length = strlen(normalized_root);
	if (strcmp(normalized_root, normalized_full) == 0) {
		const char *base = cs_path_basename(normalized_full);
		return cs_path_normalize_copy(base, out, out_size);
	}

	if (root_length > 0U && strncmp(normalized_full, normalized_root, root_length) == 0) {
		char separator = normalized_full[root_length];
		if (separator == cs_path_separator() || separator == '/' || separator == '\\') {
			return cs_path_normalize_copy(normalized_full + root_length + 1U, out, out_size);
		}
	}

	return cs_path_normalize_copy(cs_path_basename(normalized_full), out, out_size);
}

static int cs_engine_chunk_visit(const cs_chunk_slice_t *slice, const unsigned char *chunk_data, void *ctx) {
	cs_engine_chunk_ctx_t *chunk_ctx;
	char chunk_location[CS_PATH_CAP];
	char hash_prefix[3];

	if (slice == NULL || ctx == NULL) {
		return -1;
	}

	chunk_ctx = (cs_engine_chunk_ctx_t *)ctx;
	if (chunk_ctx->backup == NULL || chunk_ctx->file == NULL) {
		return -1;
	}

	if (cs_manifest_add_chunk(chunk_ctx->backup->manifest, chunk_ctx->file, slice->offset, slice->size, slice->hash_hex) != 0) {
		return -1;
	}
	chunk_ctx->backup->total_chunks++;

	if (chunk_ctx->backup->dry_run || slice->size == 0U) {
		return 0;
	}

	hash_prefix[0] = slice->hash_hex[0];
	hash_prefix[1] = slice->hash_hex[1];
	hash_prefix[2] = '\0';
	if (snprintf(chunk_location, sizeof(chunk_location), "chunks%c%s%c%s", cs_path_separator(), hash_prefix, cs_path_separator(), slice->hash_hex) < 0) {
		return -1;
	}

	if (!cs_chunk_store_exists(chunk_ctx->backup->chunk_store, slice->hash_hex)) {
		if (cs_chunk_store_put(chunk_ctx->backup->chunk_store, slice->hash_hex, chunk_data, slice->size) != 0) {
			return -1;
		}
	}

	if (cs_index_store_insert(chunk_ctx->backup->index_store, slice->hash_hex, chunk_location) != 0) {
		return -1;
	}

	return 0;
}

static int cs_engine_scan_visit(const cs_fs_metadata_t *metadata, void *ctx) {
	cs_engine_backup_ctx_t *backup;
	cs_engine_chunk_ctx_t chunk_ctx;
	unsigned char *file_data = NULL;
	size_t file_size = 0U;
	cs_manifest_file_t *file_entry = NULL;
	char relative_path[CS_PATH_CAP];
	int status;

	if (metadata == NULL || ctx == NULL) {
		return -1;
	}

	backup = (cs_engine_backup_ctx_t *)ctx;
	if (!cs_fs_metadata_is_file(metadata)) {
		return 0;
	}

	if (cs_engine_make_relative_path(backup->source_root, metadata->path, relative_path, sizeof(relative_path)) != 0) {
		CS_LOG_WARN("engine: skipping invalid path %s", metadata->path);
		return 0;
	}

	status = cs_file_read_all(metadata->path, &file_data, &file_size);
	if (status != 0) {
		CS_LOG_ERROR("engine: failed to read %s", metadata->path);
		return -1;
	}

	status = cs_manifest_add_file(backup->manifest, relative_path, metadata->modified_time, (unsigned long long)file_size, &file_entry);
	if (status != 0) {
		free(file_data);
		return -1;
	}

	if (cs_chunker_init(&backup->chunker, CS_DELTA_DEFAULT_CHUNK_SIZE) != 0) {
		free(file_data);
		return -1;
	}

	chunk_ctx.backup = backup;
	chunk_ctx.file = file_entry;
	status = cs_chunker_chunk_buffer(&backup->chunker, file_data, file_size, cs_engine_chunk_visit, &chunk_ctx);
	free(file_data);
	if (status != 0) {
		return status;
	}

	backup->file_count++;
	backup->total_bytes += (unsigned long long)file_size;
	return 0;
}

static int cs_engine_prepare_repo(const char *repo_path, cs_repo_t *repo) {
	if (repo_path == NULL || repo == NULL) {
		return -1;
	}

	if (cs_repo_exists(repo_path)) {
		if (cs_repo_load(repo_path, repo) != 0) {
			return -1;
		}
		return cs_repo_validate(repo_path);
	}

	return cs_repo_init(repo_path, repo);
}

static int cs_engine_write_restore_file(const char *out_root, const char *relative_path, const unsigned char *data, size_t size) {
	char destination[CS_PATH_CAP];
	char parent[CS_PATH_CAP];

	if (cs_engine_join(destination, sizeof(destination), out_root, relative_path) != 0) {
		return -1;
	}

	if (cs_engine_parent_directory(destination, parent, sizeof(parent)) != 0) {
		return -1;
	}
	if (cs_engine_mkdir_p(parent) != 0) {
		return -1;
	}

	return cs_file_write_all(destination, data, size);
}

static int cs_engine_restore_file(cs_chunk_store_t *chunk_store, const cs_manifest_file_t *file, const char *out_root) {
	unsigned char *buffer = NULL;
	size_t file_size;
	size_t chunk_index;
	int status = 0;

	if (chunk_store == NULL || file == NULL || out_root == NULL) {
		return -1;
	}

	file_size = (size_t)file->size_bytes;
	if (file_size > 0U) {
		buffer = (unsigned char *)malloc(file_size);
		if (buffer == NULL) {
			return -1;
		}
		memset(buffer, 0, file_size);
	}

	for (chunk_index = 0U; chunk_index < file->chunk_count; ++chunk_index) {
		const cs_manifest_chunk_t *chunk = &file->chunks[chunk_index];
		unsigned char *chunk_data = NULL;
		size_t chunk_size = 0U;

		if (chunk->size == 0U) {
			continue;
		}

		if (cs_chunk_store_get(chunk_store, chunk->hash_hex, &chunk_data, &chunk_size) != 0) {
			status = -1;
			break;
		}

		if (chunk_size != chunk->size || buffer == NULL || chunk->offset + chunk_size > file_size) {
			free(chunk_data);
			status = -1;
			break;
		}

		memcpy(buffer + chunk->offset, chunk_data, chunk_size);
		free(chunk_data);
	}

	if (status == 0) {
		status = cs_engine_write_restore_file(out_root, file->path, buffer != NULL ? buffer : (const unsigned char *)"", file_size);
	}

	free(buffer);
	return status;
}

static int cs_engine_restore_manifest(const char *repo_path, const char *snapshot_id, cs_manifest_t *manifest) {
	char manifest_path[CS_PATH_CAP];

	if (cs_engine_snapshot_artifact_path(repo_path, snapshot_id, ".manifest", manifest_path, sizeof(manifest_path)) != 0) {
		return -1;
	}

	return cs_manifest_load(manifest_path, manifest);
}

int cs_engine_init(void) {
	CS_LOG_INFO("engine: init called");
	return 0;
}

int cs_engine_backup(const char *source_path, const char *repo_path, int dry_run, int compress, int encrypt, const char *label,
					 const char *const *include_patterns, size_t include_count,
					 const char *const *exclude_patterns, size_t exclude_count) {
	cs_manifest_t manifest;
	cs_repo_t repo;
	cs_chunk_store_t *chunk_store = NULL;
	cs_index_store_t *index_store = NULL;
	cs_snapshot_store_t *snapshot_store = NULL;
	cs_snapshot_t snapshot;
	cs_engine_backup_ctx_t backup_ctx;
	cs_fs_scan_options_t scan_opts;
	char manifest_path[CS_PATH_CAP];
	char journal_record[CS_PATH_CAP * 2U];
	int status;

	if (source_path == NULL || repo_path == NULL) {
		CS_LOG_ERROR("engine: backup requires source and repo");
		return -1;
	}

	CS_LOG_INFO("engine: starting backup source=%s repo=%s dry_run=%d compress=%d encrypt=%d label=%s "
				"include_count=%lu exclude_count=%lu",
				 source_path, repo_path, dry_run, compress, encrypt, label ? label : "",
				 (unsigned long)include_count, (unsigned long)exclude_count);

	if (compress != 0) {
		CS_LOG_WARN("engine: compression flag is recorded but not applied by this engine build");
	}
	if (encrypt != 0) {
		CS_LOG_WARN("engine: encryption flag is recorded but no passphrase is available at this boundary");
	}

	if (cs_manifest_init(&manifest) != 0) {
		return -1;
	}

	backup_ctx.source_root = source_path;
	backup_ctx.manifest = &manifest;
	backup_ctx.chunk_store = NULL;
	backup_ctx.index_store = NULL;
	backup_ctx.dry_run = dry_run != 0;
	backup_ctx.file_count = 0U;
	backup_ctx.total_bytes = 0ULL;
	backup_ctx.total_chunks = 0ULL;
	if (cs_chunker_init(&backup_ctx.chunker, CS_DELTA_DEFAULT_CHUNK_SIZE) != 0) {
		cs_manifest_free(&manifest);
		return -1;
	}

	if (!backup_ctx.dry_run) {
		if (cs_engine_prepare_repo(repo_path, &repo) != 0) {
			cs_manifest_free(&manifest);
			return -1;
		}

		chunk_store = cs_chunk_store_open(repo_path);
		index_store = cs_index_store_open(repo_path);
		snapshot_store = cs_snapshot_store_open(repo_path);
		if (chunk_store == NULL || index_store == NULL || snapshot_store == NULL) {
			cs_chunk_store_close(chunk_store);
			cs_index_store_close(index_store);
			cs_snapshot_store_close(snapshot_store);
			cs_manifest_free(&manifest);
			return -1;
		}

		backup_ctx.chunk_store = chunk_store;
		backup_ctx.index_store = index_store;
	}

	cs_fs_scan_options_default(&scan_opts);
	scan_opts.include_patterns = include_patterns;
	scan_opts.include_count = include_count;
	scan_opts.exclude_patterns = exclude_patterns;
	scan_opts.exclude_count = exclude_count;

	status = cs_fs_scan(source_path, &scan_opts, cs_engine_scan_visit, &backup_ctx);
	if (status != 0) {
		cs_chunk_store_close(chunk_store);
		cs_index_store_close(index_store);
		cs_snapshot_store_close(snapshot_store);
		cs_manifest_free(&manifest);
		return -1;
	}

	if (backup_ctx.dry_run) {
		CS_LOG_INFO("engine: dry run complete files=%lu chunks=%lu bytes=%lu",
					(unsigned long)backup_ctx.file_count, (unsigned long)backup_ctx.total_chunks, (unsigned long)backup_ctx.total_bytes);
		cs_manifest_free(&manifest);
		return 0;
	}

	if (cs_snapshot_store_create(snapshot_store, source_path, label, &snapshot) != 0) {
		cs_chunk_store_close(chunk_store);
		cs_index_store_close(index_store);
		cs_snapshot_store_close(snapshot_store);
		cs_manifest_free(&manifest);
		return -1;
	}

	snapshot.file_count = (unsigned long)manifest.file_count;
	snapshot.size_bytes = manifest.total_bytes;
	if (cs_snapshot_store_update(snapshot_store, &snapshot) != 0) {
		cs_chunk_store_close(chunk_store);
		cs_index_store_close(index_store);
		cs_snapshot_store_close(snapshot_store);
		cs_manifest_free(&manifest);
		return -1;
	}

	if (cs_engine_snapshot_artifact_path(repo_path, snapshot.id, ".manifest", manifest_path, sizeof(manifest_path)) != 0) {
		cs_chunk_store_close(chunk_store);
		cs_index_store_close(index_store);
		cs_snapshot_store_close(snapshot_store);
		cs_manifest_free(&manifest);
		return -1;
	}

	if (cs_manifest_write(&manifest, manifest_path) != 0) {
		cs_chunk_store_close(chunk_store);
		cs_index_store_close(index_store);
		cs_snapshot_store_close(snapshot_store);
		cs_manifest_free(&manifest);
		return -1;
	}

	if (snprintf(journal_record, sizeof(journal_record), "BACKUP\tsnapshot=%s\tsource=%s\tfiles=%lu\tbytes=%lu", snapshot.id, source_path, (unsigned long)manifest.file_count, (unsigned long)manifest.total_bytes) > 0) {
		cs_journal_append(repo_path, journal_record);
		cs_journal_flush(repo_path);
	}

	CS_LOG_INFO("engine: backup complete snapshot=%s files=%lu chunks=%lu bytes=%lu",
				 snapshot.id, (unsigned long)manifest.file_count, (unsigned long)manifest.total_chunks, (unsigned long)manifest.total_bytes);

	cs_chunk_store_close(chunk_store);
	cs_index_store_close(index_store);
	cs_snapshot_store_close(snapshot_store);
	cs_manifest_free(&manifest);
	return 0;
}

int cs_engine_restore(const char *repo_path, const char *snapshot_id, const char *out_path) {
	cs_manifest_t manifest;
	cs_chunk_store_t *chunk_store = NULL;
	char restore_journal[CS_PATH_CAP * 2U];
	size_t file_index;
	int status;

	if (repo_path == NULL || snapshot_id == NULL || out_path == NULL) {
		CS_LOG_ERROR("engine: restore requires repo, snapshot and out path");
		return -1;
	}

	CS_LOG_INFO("engine: restoring snapshot=%s to %s (repo=%s)", snapshot_id, out_path, repo_path);

	if (cs_repo_validate(repo_path) != 0) {
		CS_LOG_ERROR("engine: invalid repository %s", repo_path);
		return -1;
	}

	if (cs_engine_mkdir_p(out_path) != 0) {
		CS_LOG_ERROR("engine: failed to create output directory %s", out_path);
		return -1;
	}

	if (cs_manifest_init(&manifest) != 0) {
		return -1;
	}

	if (cs_engine_restore_manifest(repo_path, snapshot_id, &manifest) != 0) {
		cs_manifest_free(&manifest);
		return -1;
	}

	chunk_store = cs_chunk_store_open(repo_path);
	if (chunk_store == NULL) {
		cs_manifest_free(&manifest);
		return -1;
	}

	for (file_index = 0U; file_index < manifest.file_count; ++file_index) {
		if (cs_engine_restore_file(chunk_store, &manifest.files[file_index], out_path) != 0) {
			cs_chunk_store_close(chunk_store);
			cs_manifest_free(&manifest);
			return -1;
		}
	}

	if (snprintf(restore_journal, sizeof(restore_journal), "RESTORE\tsnapshot=%s\tout=%s", snapshot_id, out_path) > 0) {
		cs_journal_append(repo_path, restore_journal);
		cs_journal_flush(repo_path);
	}

	CS_LOG_INFO("engine: restore complete snapshot=%s files=%lu", snapshot_id, (unsigned long)manifest.file_count);
	status = 0;

	cs_chunk_store_close(chunk_store);
	cs_manifest_free(&manifest);
	return status;
}
