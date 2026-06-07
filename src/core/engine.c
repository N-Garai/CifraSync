#include "core/engine.h"

#include "common/constants.h"
#include "common/log.h"
#include "common/path.h"
#include "compress/codec.h"
#include "core/journal.h"
#include "crypto/cipher.h"
#include "delta/chunker.h"
#include "delta/hash.h"
#include "delta/manifest.h"
#include "fs/file_reader.h"
#include "fs/metadata.h"
#include "fs/scanner.h"
#include "net/client.h"
#include "net/protocol.h"
#include "storage/chunk_store.h"
#include "storage/index_store.h"
#include "storage/lock.h"
#include "storage/repo.h"
#include "storage/snapshot_store.h"
#include "util/config.h"
#include "util/io_utils.h"
#include "util/time_utils.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define cs_engine_mkdir(path) _mkdir(path)
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define cs_engine_mkdir(path) mkdir(path, 0755)
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

typedef struct cs_engine_backup_ctx {
	const char *source_root;
	cs_manifest_t *manifest;
	cs_chunk_store_t *chunk_store;
	cs_index_store_t *index_store;
	cs_chunker_t chunker;
	int dry_run;
	int compress;
	int encrypt;
	const char *passphrase;
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
	unsigned char *store_data = NULL;
	size_t store_size = 0U;
	int free_store_data = 0;

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

	if (chunk_ctx->backup->compress) {
		unsigned char *compressed = NULL;
		size_t compressed_size = 0U;
		if (cs_codec_compress_alloc(CS_CODEC_RLE, chunk_data, slice->size, &compressed, &compressed_size) != 0) {
			return -1;
		}
		store_data = compressed;
		store_size = compressed_size;
		free_store_data = 1;
	} else {
		store_data = (unsigned char *)chunk_data;
		store_size = slice->size;
	}

	if (chunk_ctx->backup->encrypt && chunk_ctx->backup->passphrase != NULL) {
		unsigned char *encrypted = NULL;
		size_t encrypted_size = 0U;
		if (cs_cipher_seal_alloc(chunk_ctx->backup->passphrase, store_data, store_size, &encrypted, &encrypted_size) != 0) {
			if (free_store_data) free(store_data);
			return -1;
		}
		if (free_store_data) free(store_data);
		store_data = encrypted;
		store_size = encrypted_size;
		free_store_data = 1;
	}

	hash_prefix[0] = slice->hash_hex[0];
	hash_prefix[1] = slice->hash_hex[1];
	hash_prefix[2] = '\0';
	if (snprintf(chunk_location, sizeof(chunk_location), "chunks%c%s%c%s", cs_path_separator(), hash_prefix, cs_path_separator(), slice->hash_hex) < 0) {
		if (free_store_data) free(store_data);
		return -1;
	}

	if (!cs_chunk_store_exists(chunk_ctx->backup->chunk_store, slice->hash_hex)) {
		if (cs_chunk_store_put(chunk_ctx->backup->chunk_store, slice->hash_hex, store_data, store_size) != 0) {
			if (free_store_data) free(store_data);
			return -1;
		}
	}

	if (free_store_data) free(store_data);

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

static int cs_engine_restore_file_internal(cs_chunk_store_t *chunk_store, const cs_manifest_file_t *file, const char *out_root, const char *passphrase) {
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

		{
			unsigned char *raw_data = chunk_data;
			size_t raw_size = chunk_size;

			if (passphrase != NULL && passphrase[0] != '\0') {
				unsigned char *decrypted = NULL;
				size_t decrypted_size = 0U;
				if (cs_cipher_open_alloc(passphrase, raw_data, raw_size, &decrypted, &decrypted_size) == 0) {
					free(raw_data);
					raw_data = decrypted;
					raw_size = decrypted_size;
				}
			}

			{
				unsigned char *decompressed = NULL;
				size_t decompressed_size = 0U;
				if (cs_codec_decompress_alloc(CS_CODEC_RLE, raw_data, raw_size, &decompressed, &decompressed_size) == 0) {
					if (decompressed_size == chunk->size) {
						free(raw_data);
						raw_data = decompressed;
						raw_size = decompressed_size;
					} else {
						free(decompressed);
					}
				}
			}

			if (raw_size != chunk->size || buffer == NULL || chunk->offset + raw_size > file_size) {
				free(raw_data);
				status = -1;
				break;
			}

			memcpy(buffer + chunk->offset, raw_data, raw_size);
			free(raw_data);
		}
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

#ifdef _WIN32
static int cs_engine_orphan_gc_win32(const char *repo_path, cs_snapshot_t *snapshots, size_t snapshot_count,
									 const char *chunks_dir, unsigned long *total_scanned, unsigned long *total_removed) {
	char pattern[CS_PATH_CAP];
	WIN32_FIND_DATAA find_data;
	HANDLE handle;

	if (snprintf(pattern, sizeof(pattern), "%s\\*", chunks_dir) < 0) return -1;
	handle = FindFirstFileA(pattern, &find_data);
	if (handle == INVALID_HANDLE_VALUE) return 0;

	do {
		char prefix_dir[CS_PATH_CAP];
		char chunk_pattern[CS_PATH_CAP];
		WIN32_FIND_DATAA chunk_data;
		HANDLE chunk_handle;

		if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U) continue;
		if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;
		if (strlen(find_data.cFileName) != 2) continue;

		cs_engine_join(prefix_dir, sizeof(prefix_dir), chunks_dir, find_data.cFileName);
		if (snprintf(chunk_pattern, sizeof(chunk_pattern), "%s\\*", prefix_dir) < 0) continue;

		chunk_handle = FindFirstFileA(chunk_pattern, &chunk_data);
		if (chunk_handle == INVALID_HANDLE_VALUE) continue;

		do {
			char chunk_path[CS_PATH_CAP];
			char hash_hex[CS_HASH_HEX_BUFSZ];
			int referenced = 0;

			if ((chunk_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) continue;
			if (strcmp(chunk_data.cFileName, ".") == 0 || strcmp(chunk_data.cFileName, "..") == 0) continue;

			(*total_scanned)++;
			cs_engine_join(chunk_path, sizeof(chunk_path), prefix_dir, chunk_data.cFileName);
			strncpy(hash_hex, chunk_data.cFileName, sizeof(hash_hex) - 1);
			hash_hex[sizeof(hash_hex) - 1] = '\0';

			for (size_t si = 0U; si < snapshot_count && !referenced; ++si) {
				char manifest_path[CS_PATH_CAP];
				cs_manifest_t manifest;
				if (cs_manifest_init(&manifest) != 0) continue;
				if (cs_engine_snapshot_artifact_path(repo_path, snapshots[si].id, ".manifest", manifest_path, sizeof(manifest_path)) == 0 &&
					cs_manifest_load(manifest_path, &manifest) == 0) {
					for (size_t fi = 0U; fi < manifest.file_count && !referenced; ++fi)
						for (size_t ci = 0U; ci < manifest.files[fi].chunk_count && !referenced; ++ci)
							if (strcmp(manifest.files[fi].chunks[ci].hash_hex, hash_hex) == 0)
								referenced = 1;
				}
				cs_manifest_free(&manifest);
			}

			if (!referenced && remove(chunk_path) == 0) (*total_removed)++;
		} while (FindNextFileA(chunk_handle, &chunk_data) != 0);
		FindClose(chunk_handle);
	} while (FindNextFileA(handle, &find_data) != 0);
	FindClose(handle);
	return 0;
}
#else
static int cs_engine_orphan_gc_posix(const char *repo_path, cs_snapshot_t *snapshots, size_t snapshot_count,
									 const char *chunks_dir, unsigned long *total_scanned, unsigned long *total_removed) {
	DIR *dir = opendir(chunks_dir);
	if (dir == NULL) return 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
		if (strlen(entry->d_name) != 2) continue;

		char prefix_dir[CS_PATH_CAP];
		cs_engine_join(prefix_dir, sizeof(prefix_dir), chunks_dir, entry->d_name);

		DIR *cdir = opendir(prefix_dir);
		if (cdir == NULL) continue;

		struct dirent *centry;
		while ((centry = readdir(cdir)) != NULL) {
			char chunk_path[CS_PATH_CAP];
			char hash_hex[CS_HASH_HEX_BUFSZ];
			int referenced = 0;

			if (strcmp(centry->d_name, ".") == 0 || strcmp(centry->d_name, "..") == 0) continue;

			(*total_scanned)++;
			cs_engine_join(chunk_path, sizeof(chunk_path), prefix_dir, centry->d_name);
			strncpy(hash_hex, centry->d_name, sizeof(hash_hex) - 1);
			hash_hex[sizeof(hash_hex) - 1] = '\0';

			for (size_t si = 0U; si < snapshot_count && !referenced; ++si) {
				char manifest_path[CS_PATH_CAP];
				cs_manifest_t manifest;
				if (cs_manifest_init(&manifest) != 0) continue;
				if (cs_engine_snapshot_artifact_path(repo_path, snapshots[si].id, ".manifest", manifest_path, sizeof(manifest_path)) == 0 &&
					cs_manifest_load(manifest_path, &manifest) == 0) {
					for (size_t fi = 0U; fi < manifest.file_count && !referenced; ++fi)
						for (size_t ci = 0U; ci < manifest.files[fi].chunk_count && !referenced; ++ci)
							if (strcmp(manifest.files[fi].chunks[ci].hash_hex, hash_hex) == 0)
								referenced = 1;
				}
				cs_manifest_free(&manifest);
			}

			if (!referenced && remove(chunk_path) == 0) (*total_removed)++;
		}
		closedir(cdir);
	}
	closedir(dir);
	return 0;
}
#endif

static int cs_engine_orphan_gc(const char *repo_path) {
	char chunks_dir[CS_PATH_CAP];
	cs_snapshot_store_t *snapshot_store = NULL;
	cs_snapshot_t *snapshots = NULL;
	size_t snapshot_count = 0U;
	unsigned long total_removed = 0UL;
	unsigned long total_scanned = 0UL;

	if (cs_engine_join(chunks_dir, sizeof(chunks_dir), repo_path, "chunks") != 0) {
		return -1;
	}

	snapshot_store = cs_snapshot_store_open(repo_path);
	if (snapshot_store == NULL) return -1;

	cs_snapshot_store_list(snapshot_store, &snapshots, &snapshot_count);

#ifdef _WIN32
	cs_engine_orphan_gc_win32(repo_path, snapshots, snapshot_count, chunks_dir, &total_scanned, &total_removed);
#else
	cs_engine_orphan_gc_posix(repo_path, snapshots, snapshot_count, chunks_dir, &total_scanned, &total_removed);
#endif

	free(snapshots);
	cs_snapshot_store_close(snapshot_store);

	CS_LOG_INFO("engine: orphan GC scanned=%lu removed=%lu", (unsigned long)total_scanned, (unsigned long)total_removed);
	return 0;
}

static void cs_engine_verify_snapshot(const cs_manifest_t *manifest, const char *snapshot_id, cs_chunk_store_t *chunk_store,
									  unsigned long *total_files, unsigned long *total_chunks,
									  unsigned long *corrupt_chunks, unsigned long *missing_chunks,
									  const char *passphrase);

#ifdef _WIN32
static int cs_engine_do_verify_win32(const char *snapshots_dir, cs_chunk_store_t *chunk_store, unsigned long *total_snapshots, unsigned long *total_files, unsigned long *total_chunks, unsigned long *corrupt_chunks, unsigned long *missing_chunks, const char *passphrase) {
	char pattern[CS_PATH_CAP];
	WIN32_FIND_DATAA find_data;

	if (snprintf(pattern, sizeof(pattern), "%s\\*.manifest", snapshots_dir) < 0) return -1;
	HANDLE handle = FindFirstFileA(pattern, &find_data);
	if (handle == INVALID_HANDLE_VALUE) return 0;

	do {
		cs_manifest_t manifest;
		char manifest_path[CS_PATH_CAP];
		char snapshot_id[CS_PATH_CAP];

		if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) continue;
		cs_engine_join(manifest_path, sizeof(manifest_path), snapshots_dir, find_data.cFileName);

		if (cs_manifest_init(&manifest) != 0) continue;
		if (cs_manifest_load(manifest_path, &manifest) != 0) { cs_manifest_free(&manifest); continue; }

		(*total_snapshots)++;

		{
			const char *dot = strstr(find_data.cFileName, ".manifest");
			if (dot != NULL) {
				size_t len = (size_t)(dot - find_data.cFileName);
				if (len >= sizeof(snapshot_id)) len = sizeof(snapshot_id) - 1;
				memcpy(snapshot_id, find_data.cFileName, len);
				snapshot_id[len] = '\0';
			} else {
				strncpy(snapshot_id, find_data.cFileName, sizeof(snapshot_id) - 1);
			}
		}

		cs_engine_verify_snapshot(&manifest, snapshot_id, chunk_store, total_files, total_chunks, corrupt_chunks, missing_chunks, passphrase);
		cs_manifest_free(&manifest);
	} while (FindNextFileA(handle, &find_data) != 0);
	FindClose(handle);
	return 0;
}
#else
static int cs_engine_do_verify_posix(const char *snapshots_dir, cs_chunk_store_t *chunk_store, unsigned long *total_snapshots, unsigned long *total_files, unsigned long *total_chunks, unsigned long *corrupt_chunks, unsigned long *missing_chunks, const char *passphrase) {
	DIR *dir = opendir(snapshots_dir);
	if (dir == NULL) return 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		const char *dot = strstr(entry->d_name, ".manifest");
		if (dot == NULL) continue;

		cs_manifest_t manifest;
		char manifest_path[CS_PATH_CAP];
		char snapshot_id[CS_PATH_CAP];

		cs_engine_join(manifest_path, sizeof(manifest_path), snapshots_dir, entry->d_name);
		if (cs_manifest_init(&manifest) != 0) continue;
		if (cs_manifest_load(manifest_path, &manifest) != 0) { cs_manifest_free(&manifest); continue; }

		(*total_snapshots)++;
		{
			size_t len = (size_t)(dot - entry->d_name);
			if (len >= sizeof(snapshot_id)) len = sizeof(snapshot_id) - 1;
			memcpy(snapshot_id, entry->d_name, len);
			snapshot_id[len] = '\0';
		}

		cs_engine_verify_snapshot(&manifest, snapshot_id, chunk_store, total_files, total_chunks, corrupt_chunks, missing_chunks, passphrase);
		cs_manifest_free(&manifest);
	}
	closedir(dir);
	return 0;
}
#endif

static void cs_engine_verify_snapshot(const cs_manifest_t *manifest, const char *snapshot_id, cs_chunk_store_t *chunk_store,
									  unsigned long *total_files, unsigned long *total_chunks,
									  unsigned long *corrupt_chunks, unsigned long *missing_chunks,
									  const char *passphrase) {
	printf("Verifying snapshot %s (%lu files, %llu bytes)...\n", snapshot_id, (unsigned long)manifest->file_count, manifest->total_bytes);

	for (size_t fi = 0U; fi < manifest->file_count; ++fi) {
		const cs_manifest_file_t *file = &manifest->files[fi];
		(*total_files)++;

		for (size_t ci = 0U; ci < file->chunk_count; ++ci) {
			const cs_manifest_chunk_t *chunk = &file->chunks[ci];
			unsigned char *chunk_data = NULL;
			size_t chunk_size = 0U;
			char hash_hex[CS_HASH_HEX_BUFSZ];

			(*total_chunks)++;

			if (!cs_chunk_store_exists(chunk_store, chunk->hash_hex)) {
				printf("  MISSING chunk %s (file: %s)\n", chunk->hash_hex, file->path);
				(*missing_chunks)++;
				continue;
			}

			if (cs_chunk_store_get(chunk_store, chunk->hash_hex, &chunk_data, &chunk_size) != 0) {
				printf("  ERROR reading chunk %s (file: %s)\n", chunk->hash_hex, file->path);
				(*missing_chunks)++;
				continue;
			}

			{
				unsigned char *verify_data = chunk_data;
				size_t verify_size = chunk_size;

				if (passphrase != NULL && passphrase[0] != '\0') {
					unsigned char *decrypted = NULL;
					size_t decrypted_size = 0U;
					if (cs_cipher_open_alloc(passphrase, verify_data, verify_size, &decrypted, &decrypted_size) == 0) {
						free(verify_data);
						verify_data = decrypted;
						verify_size = decrypted_size;
					}
				}

				{
					unsigned char *decompressed = NULL;
					size_t decompressed_size = 0U;
					if (cs_codec_decompress_alloc(CS_CODEC_RLE, verify_data, verify_size, &decompressed, &decompressed_size) == 0) {
						if (decompressed_size == chunk->size) {
							free(verify_data);
							verify_data = decompressed;
							verify_size = decompressed_size;
						} else {
							free(decompressed);
						}
					}
				}

				if (cs_hash_sha256_hex(verify_data, verify_size, hash_hex) != 0) {
					free(verify_data);
					printf("  ERROR hashing chunk %s (file: %s)\n", chunk->hash_hex, file->path);
					(*corrupt_chunks)++;
					continue;
				}

				if (strcmp(hash_hex, chunk->hash_hex) != 0) {
					printf("  CORRUPT chunk %s (file: %s) expected=%s actual=%s\n", chunk->hash_hex, file->path, chunk->hash_hex, hash_hex);
					(*corrupt_chunks)++;
				}

				free(verify_data);
			}
		}
	}
}

static int cs_engine_do_verify(const char *repo_path, const char *passphrase) {
	char snapshots_dir[CS_PATH_CAP];
	cs_chunk_store_t *chunk_store = NULL;
	unsigned long total_snapshots = 0UL;
	unsigned long total_files = 0UL;
	unsigned long total_chunks = 0UL;
	unsigned long corrupt_chunks = 0UL;
	unsigned long missing_chunks = 0UL;
	cs_lock_t *verify_lock = NULL;

	CS_LOG_INFO("engine: verify starting repo=%s", repo_path);

	if (cs_lock_acquire(repo_path, CS_LOCK_SHARED, &verify_lock) != 0) {
		CS_LOG_ERROR("engine: verify failed to acquire lock on %s", repo_path);
		return -1;
	}

	if (cs_engine_join(snapshots_dir, sizeof(snapshots_dir), repo_path, "snapshots") != 0) {
		cs_lock_release(verify_lock);
		return -1;
	}

	chunk_store = cs_chunk_store_open(repo_path);
	if (chunk_store == NULL) {
		CS_LOG_ERROR("engine: verify failed to open chunk store");
		cs_lock_release(verify_lock);
		return -1;
	}

#ifdef _WIN32
	cs_engine_do_verify_win32(snapshots_dir, chunk_store, &total_snapshots, &total_files, &total_chunks, &corrupt_chunks, &missing_chunks, passphrase);
#else
	cs_engine_do_verify_posix(snapshots_dir, chunk_store, &total_snapshots, &total_files, &total_chunks, &corrupt_chunks, &missing_chunks, passphrase);
#endif

	cs_chunk_store_close(chunk_store);

	printf("\nVerify summary:\n");
	printf("  Snapshots checked: %lu\n", total_snapshots);
	printf("  Files checked: %lu\n", total_files);
	printf("  Chunks checked: %lu\n", total_chunks);
	printf("  Missing chunks: %lu\n", missing_chunks);
	printf("  Corrupt chunks: %lu\n", corrupt_chunks);

	if (missing_chunks > 0UL || corrupt_chunks > 0UL) {
		CS_LOG_ERROR("engine: verify found %lu missing, %lu corrupt chunks", missing_chunks, corrupt_chunks);
		cs_lock_release(verify_lock);
		return -1;
	}

	CS_LOG_INFO("engine: verify complete - all chunks OK");
	cs_lock_release(verify_lock);
	return 0;
}

static int cs_engine_do_prune(const char *repo_path, const char *keep_last_str, const char *older_than_str) {
	cs_snapshot_store_t *store = NULL;
	size_t keep_count = 0U;
	time_t older_than_time = 0;
	int status = -1;
	cs_lock_t *prune_lock = NULL;

	CS_LOG_INFO("engine: prune starting repo=%s", repo_path);

	if (cs_lock_acquire(repo_path, CS_LOCK_EXCLUSIVE, &prune_lock) != 0) {
		CS_LOG_ERROR("engine: prune failed to acquire lock on %s", repo_path);
		return -1;
	}

	store = cs_snapshot_store_open(repo_path);
	if (store == NULL) {
		CS_LOG_ERROR("engine: prune failed to open snapshot store");
		cs_lock_release(prune_lock);
		return -1;
	}

	if (keep_last_str != NULL && keep_last_str[0] != '\0') {
		keep_count = (size_t)atoi(keep_last_str);
	}
	if (older_than_str != NULL && older_than_str[0] != '\0') {
		long days = atol(older_than_str);
		if (days > 0L) {
			older_than_time = cs_time_now_unix() - (time_t)(days * 86400L);
		}
	}

	if (keep_count == 0U && older_than_time == 0) {
		keep_count = 7U;
	}

	{
		cs_snapshot_t *snapshots = NULL;
		size_t snapshot_count = 0U;
		size_t deleted_count = 0U;

		if (cs_snapshot_store_list(store, &snapshots, &snapshot_count) != 0) {
			goto prune_cleanup;
		}

		if (snapshot_count > keep_count) {
			size_t sort_i, sort_j;
			for (sort_i = 0U; sort_i < snapshot_count; ++sort_i) {
				for (sort_j = sort_i + 1U; sort_j < snapshot_count; ++sort_j) {
					if (snapshots[sort_j].timestamp < snapshots[sort_i].timestamp) {
						cs_snapshot_t tmp = snapshots[sort_i];
						snapshots[sort_i] = snapshots[sort_j];
						snapshots[sort_j] = tmp;
					}
				}
			}

			for (size_t i = 0U; i < snapshot_count - keep_count; ++i) {
				int should_delete = 0;

				if (older_than_time > 0 && snapshots[i].timestamp < older_than_time) {
					should_delete = 1;
				} else if (older_than_time == 0) {
					should_delete = 1;
				}

				if (should_delete) {
					char manifest_path[CS_PATH_CAP];
					if (cs_snapshot_store_delete(store, snapshots[i].id) == 0) {
						deleted_count++;
						printf("Deleted snapshot: %s\n", snapshots[i].id);

						if (cs_engine_snapshot_artifact_path(repo_path, snapshots[i].id, ".manifest", manifest_path, sizeof(manifest_path)) == 0) {
							remove(manifest_path);
						}
					}
				}
			}
		}

		free(snapshots);
		printf("Prune complete: %lu snapshot(s) deleted\n", (unsigned long)deleted_count);
	}

	cs_engine_orphan_gc(repo_path);

	status = 0;

prune_cleanup:
	cs_snapshot_store_close(store);
	cs_lock_release(prune_lock);
	return status;
}

static int cs_engine_sync_send_chunk(cs_net_client_t *client, const char *hash_hex, const unsigned char *data, size_t size) {
	cs_net_frame_t chunk_frame;
	cs_net_owned_frame_t reply;
	unsigned char *payload;
	int status;

	payload = (unsigned char *)malloc(CS_HASH_HEX_LEN + size);
	if (payload == NULL) {
		return -1;
	}

	memcpy(payload, hash_hex, CS_HASH_HEX_LEN);
	memcpy(payload + CS_HASH_HEX_LEN, data, size);

	chunk_frame.type = CS_NET_MESSAGE_CHUNK;
	chunk_frame.payload = payload;
	chunk_frame.payload_size = CS_HASH_HEX_LEN + size;

	status = cs_net_client_send_frame(client, &chunk_frame);
	free(payload);
	if (status != 0) {
		return status;
	}

	status = cs_net_client_receive_frame(client, &reply);
	if (status != 0) {
		return status;
	}

	status = 0;
	cs_net_owned_frame_reset(&reply);
	return status;
}

static int cs_engine_do_sync(const char *repo_path, const char *remote) {
	char host[256];
	unsigned short port = 0;
	const char *colon;
	cs_net_client_t client;
	cs_net_owned_frame_t reply;
	cs_snapshot_store_t *store = NULL;
	cs_chunk_store_t *chunk_store = NULL;
	cs_snapshot_t *snapshots = NULL;
	size_t snapshot_count = 0U;
	int status = -1;
	int sync_ok = 0;
	cs_lock_t *sync_lock = NULL;

	CS_LOG_INFO("engine: sync starting repo=%s remote=%s", repo_path, remote);

	if (remote == NULL || remote[0] == '\0') {
		CS_LOG_ERROR("engine: sync requires --remote HOST:PORT");
		return -1;
	}

	colon = strchr(remote, ':');
	if (colon == NULL) {
		CS_LOG_ERROR("engine: sync remote must be HOST:PORT format");
		return -1;
	}

	{
		size_t host_len = (size_t)(colon - remote);
		if (host_len >= sizeof(host)) host_len = sizeof(host) - 1;
		memcpy(host, remote, host_len);
		host[host_len] = '\0';
	}

	port = (unsigned short)atoi(colon + 1);
	if (port == 0) {
		CS_LOG_ERROR("engine: sync invalid port in %s", remote);
		return -1;
	}

	cs_net_client_init(&client);
	if (cs_net_client_connect(&client, host, port) != 0) {
		CS_LOG_ERROR("engine: sync failed to connect to %s:%u", host, (unsigned)port);
		return -1;
	}

	printf("Connected to remote %s:%u\n", host, (unsigned)port);

	/* Send HELLO */
	if (cs_net_client_exchange_text(&client, CS_NET_MESSAGE_HELLO, repo_path, strlen(repo_path), &reply) != 0) {
		CS_LOG_ERROR("engine: sync HELLO failed");
		goto cleanup;
	}
	printf("HELLO response: %.*s\n", (int)reply.frame.payload_size, (const char *)reply.frame.payload);
	cs_net_owned_frame_reset(&reply);

	/* Lock and open local repo */
	if (cs_lock_acquire(repo_path, CS_LOCK_SHARED, &sync_lock) != 0) {
		CS_LOG_ERROR("engine: sync failed to acquire lock on %s", repo_path);
		goto cleanup;
	}

	store = cs_snapshot_store_open(repo_path);
	if (store == NULL) {
		CS_LOG_ERROR("engine: sync failed to open snapshot store");
		goto cleanup;
	}

	chunk_store = cs_chunk_store_open(repo_path);
	if (chunk_store == NULL) {
		CS_LOG_ERROR("engine: sync failed to open chunk store");
		goto cleanup;
	}

	if (cs_snapshot_store_list(store, &snapshots, &snapshot_count) != 0) {
		CS_LOG_ERROR("engine: sync failed to list snapshots");
		goto cleanup;
	}

	printf("Syncing %lu snapshot(s)...\n", (unsigned long)snapshot_count);

	for (size_t si = 0U; si < snapshot_count; ++si) {
		char manifest_path[CS_PATH_CAP];
		char *manifest_content = NULL;
		long manifest_size;
		char *manifest_payload;
		size_t payload_size;
		FILE *fp;

		printf("  Snapshot %s...\n", snapshots[si].id);

		if (cs_engine_snapshot_artifact_path(repo_path, snapshots[si].id, ".manifest", manifest_path, sizeof(manifest_path)) != 0) {
			CS_LOG_ERROR("engine: sync failed to build manifest path for %s", snapshots[si].id);
			goto cleanup;
		}

		fp = fopen(manifest_path, "rb");
		if (fp == NULL) {
			CS_LOG_WARN("engine: sync manifest not found for %s, skipping", snapshots[si].id);
			continue;
		}

		{
			fseek(fp, 0, SEEK_END);
			manifest_size = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			if (manifest_size <= 0) {
				fclose(fp);
				continue;
			}

			manifest_content = (char *)malloc((size_t)manifest_size + 1U);
			if (manifest_content == NULL) {
				fclose(fp);
				goto cleanup;
			}

			if (fread(manifest_content, 1, (size_t)manifest_size, fp) != (size_t)manifest_size) {
				fclose(fp);
				free(manifest_content);
				goto cleanup;
			}
			manifest_content[manifest_size] = '\0';
			fclose(fp);
		}

		/* Build manifest payload: header line + full manifest content */
		{
			char header_buf[512];
			int header_len = snprintf(header_buf, sizeof(header_buf),
				"SNAPSHOT\tid=%s\tsource=%s\tts=%ld\tfiles=%lu\tsize=%lu",
				snapshots[si].id, snapshots[si].source_path,
				(long)snapshots[si].timestamp,
				(unsigned long)snapshots[si].file_count,
				(unsigned long)snapshots[si].size_bytes);
			if (header_len < 0 || (size_t)header_len >= sizeof(header_buf)) {
				free(manifest_content);
				goto cleanup;
			}

			payload_size = (size_t)header_len + 1U + (size_t)manifest_size;
			manifest_payload = (char *)malloc(payload_size + 1U);
			if (manifest_payload == NULL) {
				free(manifest_content);
				goto cleanup;
			}

			memcpy(manifest_payload, header_buf, (size_t)header_len);
			manifest_payload[header_len] = '\n';
			memcpy(manifest_payload + header_len + 1U, manifest_content, (size_t)manifest_size);
			manifest_payload[payload_size] = '\0';

			free(manifest_content);
		}

		/* Send MANIFEST */
		{
			cs_net_frame_t manifest_frame;
			cs_net_owned_frame_t manifest_reply;

			manifest_frame.type = CS_NET_MESSAGE_MANIFEST;
			manifest_frame.payload = (unsigned char *)manifest_payload;
			manifest_frame.payload_size = payload_size;

			if (cs_net_client_send_frame(&client, &manifest_frame) != 0) {
				free(manifest_payload);
				CS_LOG_ERROR("engine: sync failed to send manifest for %s", snapshots[si].id);
				goto cleanup;
			}
			free(manifest_payload);

			if (cs_net_client_receive_frame(&client, &manifest_reply) != 0) {
				CS_LOG_ERROR("engine: sync failed to receive manifest ack for %s", snapshots[si].id);
				goto cleanup;
			}

			printf("    Manifest response: %.*s\n", (int)manifest_reply.frame.payload_size, (const char *)manifest_reply.frame.payload);

			/* Parse missing hashes from response */
			{
				const char *resp = (const char *)manifest_reply.frame.payload;
				size_t resp_len = manifest_reply.frame.payload_size;
				const char *missing_prefix = "missing=";
				const char *missing_pos;
				const char *hash_start;
				size_t missing_count = 0U;
				size_t sent_count = 0U;

				missing_pos = strstr(resp, missing_prefix);
				if (missing_pos != NULL) {
					missing_pos += strlen(missing_prefix);
					missing_count = (size_t)atol(missing_pos);
				}

				if (missing_count > 0) {
					hash_start = (const char *)memchr(resp, '\n', resp_len);
					if (hash_start != NULL) {
						hash_start++;
					} else {
						hash_start = resp + resp_len;
					}

					while (hash_start < resp + resp_len && sent_count < missing_count) {
						const char *nl = (const char *)memchr(hash_start, '\n', (size_t)(resp + resp_len - hash_start));
						size_t hash_len;

						if (nl != NULL) {
							hash_len = (size_t)(nl - hash_start);
						} else {
							hash_len = (size_t)(resp + resp_len - hash_start);
						}

						if (hash_len == CS_HASH_HEX_LEN) {
							char hash_hex[CS_HASH_HEX_BUFSZ];
							unsigned char *chunk_data = NULL;
							size_t chunk_size = 0U;

							memcpy(hash_hex, hash_start, CS_HASH_HEX_LEN);
							hash_hex[CS_HASH_HEX_LEN] = '\0';

							if (cs_chunk_store_get(chunk_store, hash_hex, &chunk_data, &chunk_size) == 0) {
								if (cs_engine_sync_send_chunk(&client, hash_hex, chunk_data, chunk_size) == 0) {
									sent_count++;
									printf("    Sent chunk %s (%zu bytes)\n", hash_hex, chunk_size);
								} else {
									CS_LOG_WARN("engine: sync failed to send chunk %s", hash_hex);
								}
								free(chunk_data);
							} else {
								CS_LOG_WARN("engine: sync missing local chunk %s", hash_hex);
							}
						}

						if (nl != NULL) {
							hash_start = nl + 1;
						} else {
							break;
						}
					}
				}

				printf("    Sent %lu/%lu missing chunks\n", (unsigned long)sent_count, (unsigned long)missing_count);
			}

			cs_net_owned_frame_reset(&manifest_reply);
		}
	}

	sync_ok = 1;
	status = 0;

cleanup:
	free(snapshots);

	{
		cs_net_owned_frame_t bye_reply;
		cs_net_client_exchange_text(&client, CS_NET_MESSAGE_BYE, "bye", 3, &bye_reply);
		cs_net_owned_frame_reset(&bye_reply);
	}

	cs_chunk_store_close(chunk_store);
	cs_snapshot_store_close(store);
	cs_net_client_close(&client);
	cs_lock_release(sync_lock);

	if (sync_ok) {
		CS_LOG_INFO("engine: sync complete to %s:%u", host, (unsigned)port);
		printf("Sync completed successfully.\n");
	} else {
		CS_LOG_ERROR("engine: sync failed to %s:%u", host, (unsigned)port);
	}

	return status;
}

static void cs_engine_print_snapshot_entry(const char *snapshot_path) {
	char line[512];
	FILE *fp = fopen(snapshot_path, "rb");
	if (fp == NULL) return;

	char sid[64] = "";
	char stime[64] = "";
	char slabel[256] = "";
	char sfiles[16] = "";
	char ssize[32] = "";

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *nl = strchr(line, '\n');
		if (nl) *nl = '\0';

		if (strncmp(line, "id=", 3) == 0) strncpy(sid, line + 3, sizeof(sid) - 1);
		else if (strncmp(line, "timestamp=", 10) == 0) {
			time_t ts = (time_t)atol(line + 10);
			struct tm *tm_info = localtime(&ts);
			if (tm_info) strftime(stime, sizeof(stime), "%Y-%m-%d %H:%M:%S", tm_info);
		}
		else if (strncmp(line, "label=", 6) == 0) strncpy(slabel, line + 6, sizeof(slabel) - 1);
		else if (strncmp(line, "file_count=", 11) == 0) strncpy(sfiles, line + 11, sizeof(sfiles) - 1);
		else if (strncmp(line, "size_bytes=", 11) == 0) {
			unsigned long long bytes = strtoull(line + 11, NULL, 10);
			if (bytes > 1073741824ULL)
				snprintf(ssize, sizeof(ssize), "%.1f GB", (double)bytes / 1073741824.0);
			else if (bytes > 1048576ULL)
				snprintf(ssize, sizeof(ssize), "%.1f MB", (double)bytes / 1048576.0);
			else if (bytes > 1024ULL)
				snprintf(ssize, sizeof(ssize), "%.1f KB", (double)bytes / 1024.0);
			else {
				unsigned long bytes_ul = (unsigned long)bytes;
				snprintf(ssize, sizeof(ssize), "%lu B", bytes_ul);
			}
		}
	}
	fclose(fp);
	printf("%-30s %-20s %-10s %-12s %s\n", sid, stime, sfiles, ssize, slabel);
}

#ifdef _WIN32
static int cs_engine_do_list_win32(const char *snapshots_dir, int *found_any) {
	char pattern[CS_PATH_CAP];
	WIN32_FIND_DATAA find_data;

	if (snprintf(pattern, sizeof(pattern), "%s\\*.snapshot", snapshots_dir) < 0) return -1;
	HANDLE handle = FindFirstFileA(pattern, &find_data);
	if (handle == INVALID_HANDLE_VALUE) return 0;

	do {
		if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) continue;
		if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;
		*found_any = 1;
		char snapshot_path[CS_PATH_CAP];
		cs_engine_join(snapshot_path, sizeof(snapshot_path), snapshots_dir, find_data.cFileName);
		cs_engine_print_snapshot_entry(snapshot_path);
	} while (FindNextFileA(handle, &find_data) != 0);
	FindClose(handle);
	return 0;
}
#else
static int cs_engine_do_list_posix(const char *snapshots_dir, int *found_any) {
	DIR *dir = opendir(snapshots_dir);
	if (dir == NULL) return 0;

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (strstr(entry->d_name, ".snapshot") == NULL) continue;
		*found_any = 1;
		char snapshot_path[CS_PATH_CAP];
		cs_engine_join(snapshot_path, sizeof(snapshot_path), snapshots_dir, entry->d_name);
		cs_engine_print_snapshot_entry(snapshot_path);
	}
	closedir(dir);
	return 0;
}
#endif

static int cs_engine_do_list(const char *repo_path) {
	char snapshots_dir[CS_PATH_CAP];
	int found_any = 0;

	if (cs_engine_join(snapshots_dir, sizeof(snapshots_dir), repo_path, "snapshots") != 0) {
		fprintf(stderr, "list: failed to build snapshots directory path\n");
		return -1;
	}

	printf("Snapshots:\n");
	printf("%-30s %-20s %-10s %-12s %s\n", "ID", "Timestamp", "Files", "Size", "Label");
	printf("%-30s %-20s %-10s %-12s %s\n", "---", "---------", "-----", "----", "-----");

#ifdef _WIN32
	cs_engine_do_list_win32(snapshots_dir, &found_any);
#else
	cs_engine_do_list_posix(snapshots_dir, &found_any);
#endif

	if (!found_any) {
		printf("  (none)\n");
	}

	return 0;
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
	char passphrase[256] = "";
	int status;
	cs_lock_t *backup_lock = NULL;

	if (source_path == NULL || repo_path == NULL) {
		CS_LOG_ERROR("engine: backup requires source and repo");
		return -1;
	}

	if (!dry_run) {
		if (cs_lock_acquire(repo_path, CS_LOCK_EXCLUSIVE, &backup_lock) != 0) {
			CS_LOG_ERROR("engine: backup failed to acquire lock on %s", repo_path);
			return -1;
		}
	}

	CS_LOG_INFO("engine: starting backup source=%s repo=%s dry_run=%d compress=%d encrypt=%d label=%s "
				"include_count=%lu exclude_count=%lu",
				 source_path, repo_path, dry_run, compress, encrypt, label ? label : "",
				 (unsigned long)include_count, (unsigned long)exclude_count);

	if (encrypt != 0) {
		CS_LOG_INFO("engine: encryption enabled, requesting passphrase");
		printf("Enter encryption passphrase: ");
		if (fgets(passphrase, sizeof(passphrase), stdin) != NULL) {
			size_t plen = strlen(passphrase);
			while (plen > 0U && (passphrase[plen - 1U] == '\n' || passphrase[plen - 1U] == '\r')) {
				passphrase[--plen] = '\0';
			}
		}
	}

	cs_journal_replay(repo_path, NULL, NULL);

	status = -1;

	if (cs_manifest_init(&manifest) != 0) {
		goto cleanup;
	}

	backup_ctx.source_root = source_path;
	backup_ctx.manifest = &manifest;
	backup_ctx.chunk_store = NULL;
	backup_ctx.index_store = NULL;
	backup_ctx.dry_run = dry_run != 0;
	backup_ctx.compress = compress != 0;
	backup_ctx.encrypt = encrypt != 0;
	backup_ctx.passphrase = passphrase[0] != '\0' ? passphrase : NULL;
	backup_ctx.file_count = 0U;
	backup_ctx.total_bytes = 0ULL;
	backup_ctx.total_chunks = 0ULL;

	if (cs_chunker_init(&backup_ctx.chunker, CS_DELTA_DEFAULT_CHUNK_SIZE) != 0) {
		goto cleanup;
	}

	if (!backup_ctx.dry_run) {
		if (cs_engine_prepare_repo(repo_path, &repo) != 0) {
			goto cleanup;
		}

		chunk_store = cs_chunk_store_open(repo_path);
		index_store = cs_index_store_open(repo_path);
		snapshot_store = cs_snapshot_store_open(repo_path);
		if (chunk_store == NULL || index_store == NULL || snapshot_store == NULL) {
			cs_chunk_store_close(chunk_store);
			cs_index_store_close(index_store);
			cs_snapshot_store_close(snapshot_store);
			goto cleanup;
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
		goto cleanup;
	}

	if (backup_ctx.dry_run) {
		CS_LOG_INFO("engine: dry run complete files=%lu chunks=%lu bytes=%lu",
					(unsigned long)backup_ctx.file_count, (unsigned long)backup_ctx.total_chunks, (unsigned long)backup_ctx.total_bytes);
		status = 0;
		goto cleanup;
	}

	if (cs_snapshot_store_create(snapshot_store, source_path, label, &snapshot) != 0) {
		goto cleanup;
	}

	snapshot.file_count = (unsigned long)manifest.file_count;
	snapshot.size_bytes = manifest.total_bytes;
	if (cs_snapshot_store_update(snapshot_store, &snapshot) != 0) {
		goto cleanup;
	}

	if (cs_engine_snapshot_artifact_path(repo_path, snapshot.id, ".manifest", manifest_path, sizeof(manifest_path)) != 0) {
		goto cleanup;
	}

	{
		char tmp_manifest_path[CS_PATH_CAP];
		int written = snprintf(tmp_manifest_path, sizeof(tmp_manifest_path), "%s.tmp", manifest_path);
		if (written < 0 || (size_t)written >= sizeof(tmp_manifest_path)) {
			goto cleanup;
		}
		if (cs_manifest_write(&manifest, tmp_manifest_path) != 0) {
			remove(tmp_manifest_path);
			goto cleanup;
		}
		if (rename(tmp_manifest_path, manifest_path) != 0) {
			remove(tmp_manifest_path);
			goto cleanup;
		}
	}

	if (snprintf(journal_record, sizeof(journal_record), "BACKUP\tsnapshot=%s\tsource=%s\tfiles=%lu\tbytes=%lu", snapshot.id, source_path, (unsigned long)manifest.file_count, (unsigned long)manifest.total_bytes) > 0) {
		cs_journal_append(repo_path, journal_record);
		cs_journal_flush(repo_path);
	}

	CS_LOG_INFO("engine: backup complete snapshot=%s files=%lu chunks=%lu bytes=%lu",
				 snapshot.id, (unsigned long)manifest.file_count, (unsigned long)manifest.total_chunks, (unsigned long)manifest.total_bytes);

	status = 0;

cleanup:
	cs_chunk_store_close(chunk_store);
	cs_index_store_close(index_store);
	cs_snapshot_store_close(snapshot_store);
	cs_manifest_free(&manifest);
	cs_lock_release(backup_lock);
	return status;
}

int cs_engine_restore(const char *repo_path, const char *snapshot_id, const char *out_path, const char *passphrase) {
	cs_manifest_t manifest;
	cs_chunk_store_t *chunk_store = NULL;
	char restore_journal[CS_PATH_CAP * 2U];
	size_t file_index;
	int status;
	cs_lock_t *restore_lock = NULL;

	if (repo_path == NULL || snapshot_id == NULL || out_path == NULL) {
		CS_LOG_ERROR("engine: restore requires repo, snapshot and out path");
		return -1;
	}

	CS_LOG_INFO("engine: restoring snapshot=%s to %s (repo=%s)", snapshot_id, out_path, repo_path);

	if (cs_lock_acquire(repo_path, CS_LOCK_SHARED, &restore_lock) != 0) {
		CS_LOG_ERROR("engine: restore failed to acquire lock on %s", repo_path);
		return -1;
	}

	if (cs_repo_validate(repo_path) != 0) {
		CS_LOG_ERROR("engine: invalid repository %s", repo_path);
		cs_lock_release(restore_lock);
		return -1;
	}

	if (cs_engine_mkdir_p(out_path) != 0) {
		CS_LOG_ERROR("engine: failed to create output directory %s", out_path);
		cs_lock_release(restore_lock);
		return -1;
	}

	if (cs_manifest_init(&manifest) != 0) {
		cs_lock_release(restore_lock);
		return -1;
	}

	status = -1;

	if (cs_engine_restore_manifest(repo_path, snapshot_id, &manifest) != 0) {
		goto restore_cleanup;
	}

	chunk_store = cs_chunk_store_open(repo_path);
	if (chunk_store == NULL) {
		goto restore_cleanup;
	}

	for (file_index = 0U; file_index < manifest.file_count; ++file_index) {
		if (cs_engine_restore_file_internal(chunk_store, &manifest.files[file_index], out_path, passphrase) != 0) {
			goto restore_cleanup;
		}
	}

	if (snprintf(restore_journal, sizeof(restore_journal), "RESTORE\tsnapshot=%s\tout=%s", snapshot_id, out_path) > 0) {
		cs_journal_append(repo_path, restore_journal);
		cs_journal_flush(repo_path);
	}

	CS_LOG_INFO("engine: restore complete snapshot=%s files=%lu", snapshot_id, (unsigned long)manifest.file_count);
	status = 0;

restore_cleanup:
	cs_chunk_store_close(chunk_store);
	cs_manifest_free(&manifest);
	cs_lock_release(restore_lock);
	return status;
}

int cs_engine_restore_file(const char *repo_path, const char *snapshot_id, const char *source_file, const char *out_path, const char *passphrase) {
	cs_manifest_t manifest;
	cs_chunk_store_t *chunk_store = NULL;
	cs_lock_t *restore_lock = NULL;
	int status = -1;

	if (repo_path == NULL || snapshot_id == NULL || source_file == NULL || out_path == NULL) {
		CS_LOG_ERROR("engine: restore_file requires repo, snapshot, source, out");
		return -1;
	}

	CS_LOG_INFO("engine: restoring file=%s from snapshot=%s to %s", source_file, snapshot_id, out_path);

	if (cs_lock_acquire(repo_path, CS_LOCK_SHARED, &restore_lock) != 0) {
		CS_LOG_ERROR("engine: restore_file failed to acquire lock on %s", repo_path);
		return -1;
	}

	if (cs_repo_validate(repo_path) != 0) {
		CS_LOG_ERROR("engine: invalid repository %s", repo_path);
		cs_lock_release(restore_lock);
		return -1;
	}

	if (cs_manifest_init(&manifest) != 0) {
		cs_lock_release(restore_lock);
		return -1;
	}

	if (cs_engine_restore_manifest(repo_path, snapshot_id, &manifest) != 0) {
		goto restore_file_cleanup;
	}

	chunk_store = cs_chunk_store_open(repo_path);
	if (chunk_store == NULL) {
		goto restore_file_cleanup;
	}

	status = -1;

	for (size_t fi = 0U; fi < manifest.file_count; ++fi) {
		const cs_manifest_file_t *file = &manifest.files[fi];
		if (strcmp(file->path, source_file) == 0) {
			char full_out_path[CS_PATH_CAP];
			char parent[CS_PATH_CAP];
			char *slash;

			if (cs_engine_join(full_out_path, sizeof(full_out_path), out_path, file->path) != 0) {
				goto restore_file_cleanup;
			}

			slash = strrchr(full_out_path, cs_path_separator());
			if (slash != NULL) {
				*slash = '\0';
				if (cs_engine_mkdir_p(full_out_path) != 0) {
					*slash = cs_path_separator();
					goto restore_file_cleanup;
				}
				*slash = cs_path_separator();
			}

			cs_engine_join(parent, sizeof(parent), out_path, ".");
			if (cs_engine_mkdir_p(parent) != 0) {
				goto restore_file_cleanup;
			}

			if (cs_engine_restore_file_internal(chunk_store, file, out_path, passphrase) != 0) {
				goto restore_file_cleanup;
			}

			printf("Restored single file: %s -> %s\n", source_file, full_out_path);
			status = 0;
			goto restore_file_cleanup;
		}
	}

	CS_LOG_ERROR("engine: restore_file: '%s' not found in snapshot", source_file);

restore_file_cleanup:
	cs_chunk_store_close(chunk_store);
	cs_manifest_free(&manifest);
	cs_lock_release(restore_lock);
	return status;
}

int cs_engine_verify(const char *repo_path, const char *passphrase) {
	if (repo_path == NULL) {
		CS_LOG_ERROR("engine: verify requires repo path");
		return -1;
	}
	return cs_engine_do_verify(repo_path, passphrase);
}

int cs_engine_prune(const char *repo_path, const char *keep_last, const char *older_than) {
	if (repo_path == NULL) {
		CS_LOG_ERROR("engine: prune requires repo path");
		return -1;
	}
	return cs_engine_do_prune(repo_path, keep_last, older_than);
}

int cs_engine_sync(const char *repo_path, const char *remote) {
	if (repo_path == NULL || remote == NULL) {
		CS_LOG_ERROR("engine: sync requires repo path and remote");
		return -1;
	}
	return cs_engine_do_sync(repo_path, remote);
}

int cs_engine_list(const char *repo_path) {
	if (repo_path == NULL) {
		fprintf(stderr, "list requires --repo PATH\n");
		return -1;
	}
	return cs_engine_do_list(repo_path);
}
