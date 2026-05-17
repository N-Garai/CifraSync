#include "delta/manifest.h"

#include "common/memory.h"
#include "common/path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cs_manifest_ensure_file_capacity(cs_manifest_t *manifest) {
	cs_manifest_file_t *files;
	size_t new_capacity;

	if (manifest == NULL) {
		return -1;
	}

	if (manifest->file_capacity == 0U) {
		new_capacity = 8U;
	} else {
		new_capacity = manifest->file_capacity * 2U;
	}

	files = (cs_manifest_file_t *)cs_realloc(manifest->files, new_capacity * sizeof(*files));
	if (files == NULL) {
		return -1;
	}

	manifest->files = files;
	manifest->file_capacity = new_capacity;
	return 0;
}

static int cs_manifest_file_ensure_chunk_capacity(cs_manifest_file_t *file) {
	cs_manifest_chunk_t *chunks;
	size_t new_capacity;

	if (file == NULL) {
		return -1;
	}

	if (file->chunk_capacity == 0U) {
		new_capacity = 8U;
	} else {
		new_capacity = file->chunk_capacity * 2U;
	}

	chunks = (cs_manifest_chunk_t *)cs_realloc(file->chunks, new_capacity * sizeof(*chunks));
	if (chunks == NULL) {
		return -1;
	}

	file->chunks = chunks;
	file->chunk_capacity = new_capacity;
	return 0;
}

static int cs_manifest_validate_relative_path(const char *relative_path) {
	if (relative_path == NULL || relative_path[0] == '\0') {
		return -1;
	}
	if (cs_path_is_absolute(relative_path) || cs_path_has_parent_reference(relative_path)) {
		return -1;
	}
	return 0;
}

typedef struct cs_manifest_chunk_visit_ctx {
	cs_manifest_t *manifest;
	cs_manifest_file_t *file;
} cs_manifest_chunk_visit_ctx_t;

static int cs_manifest_chunk_visit(const cs_chunk_slice_t *slice, const unsigned char *chunk_data, void *ctx) {
	cs_manifest_chunk_visit_ctx_t *visit_ctx;
	(void)chunk_data;

	if (slice == NULL || ctx == NULL) {
		return -1;
	}

	visit_ctx = (cs_manifest_chunk_visit_ctx_t *)ctx;
	if (visit_ctx->manifest == NULL || visit_ctx->file == NULL) {
		return -1;
	}
	return cs_manifest_add_chunk(visit_ctx->manifest, visit_ctx->file, slice->offset, slice->size, slice->hash_hex);
}

int cs_manifest_init(cs_manifest_t *manifest) {
	if (manifest == NULL) {
		return -1;
	}

	cs_memzero(manifest, sizeof(*manifest));
	return 0;
}

void cs_manifest_reset(cs_manifest_t *manifest) {
	if (manifest == NULL) {
		return;
	}

	for (size_t file_index = 0U; file_index < manifest->file_count; ++file_index) {
		cs_manifest_file_t *file = &manifest->files[file_index];
		cs_free(file->chunks);
		file->chunks = NULL;
		file->chunk_count = 0U;
		file->chunk_capacity = 0U;
	}

	manifest->file_count = 0U;
	manifest->total_bytes = 0ULL;
	manifest->total_chunks = 0U;
}

void cs_manifest_free(cs_manifest_t *manifest) {
	if (manifest == NULL) {
		return;
	}

	cs_manifest_reset(manifest);
	cs_free(manifest->files);
	manifest->files = NULL;
	manifest->file_capacity = 0U;
}

int cs_manifest_add_file(cs_manifest_t *manifest,
	const char *relative_path,
	time_t modified_time,
	unsigned long long size_bytes,
	cs_manifest_file_t **out_file) {
	cs_manifest_file_t *file;

	if (manifest == NULL || cs_manifest_validate_relative_path(relative_path) != 0) {
		return -1;
	}

	if (manifest->file_count >= manifest->file_capacity) {
		if (cs_manifest_ensure_file_capacity(manifest) != 0) {
			return -1;
		}
	}

	file = &manifest->files[manifest->file_count];
	cs_memzero(file, sizeof(*file));
	strncpy(file->path, relative_path, sizeof(file->path) - 1U);
	file->modified_time = modified_time;
	file->size_bytes = size_bytes;

	manifest->file_count++;
	manifest->total_bytes += size_bytes;

	if (out_file != NULL) {
		*out_file = file;
	}

	return 0;
}

int cs_manifest_add_chunk(cs_manifest_t *manifest,
	cs_manifest_file_t *file,
	size_t offset,
	size_t size,
	const char *hash_hex) {
	cs_manifest_chunk_t *chunk;

	if (manifest == NULL || file == NULL || hash_hex == NULL || !cs_hash_is_hex(hash_hex)) {
		return -1;
	}

	if (file->chunk_count >= file->chunk_capacity) {
		if (cs_manifest_file_ensure_chunk_capacity(file) != 0) {
			return -1;
		}
	}

	chunk = &file->chunks[file->chunk_count];
	cs_memzero(chunk, sizeof(*chunk));
	chunk->offset = offset;
	chunk->size = size;
	strncpy(chunk->hash_hex, hash_hex, sizeof(chunk->hash_hex) - 1U);

	file->chunk_count++;
	manifest->total_chunks++;
	return 0;
}

int cs_manifest_append_file_from_buffer(cs_manifest_t *manifest,
	const char *relative_path,
	const unsigned char *data,
	size_t data_size,
	size_t chunk_size,
	time_t modified_time,
	cs_manifest_file_t **out_file) {
	cs_chunker_t chunker;
	cs_manifest_file_t *file;
	int status;

	if (cs_chunker_init(&chunker, chunk_size) != 0) {
		return -1;
	}

	status = cs_manifest_add_file(manifest, relative_path, modified_time, (unsigned long long)data_size, &file);
	if (status != 0) {
		return status;
	}

	{
		cs_manifest_chunk_visit_ctx_t ctx;
		ctx.manifest = manifest;
		ctx.file = file;
		status = cs_chunker_chunk_buffer(&chunker, data, data_size, cs_manifest_chunk_visit, &ctx);
	}

	if (status != 0) {
		return status;
	}

	if (out_file != NULL) {
		*out_file = file;
	}
	return 0;
}

static int cs_manifest_write_file(FILE *fp, const cs_manifest_file_t *file) {
	if (fprintf(fp, "FILE\t%s\t%lld\t%llu\t%zu\n", file->path, (long long)file->modified_time, file->size_bytes, file->chunk_count) < 0) {
		return -1;
	}
	for (size_t index = 0U; index < file->chunk_count; ++index) {
		const cs_manifest_chunk_t *chunk = &file->chunks[index];
		if (fprintf(fp, "CHUNK\t%zu\t%zu\t%s\n", chunk->offset, chunk->size, chunk->hash_hex) < 0) {
			return -1;
		}
	}
	if (fprintf(fp, "END\n") < 0) {
		return -1;
	}
	return 0;
}

int cs_manifest_write(const cs_manifest_t *manifest, const char *path) {
	FILE *fp;

	if (manifest == NULL || path == NULL) {
		return -1;
	}

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return -1;
	}

	if (fprintf(fp, "CIFRASYNC-MANIFEST\t1\n") < 0) {
		fclose(fp);
		return -1;
	}
	if (fprintf(fp, "TOTAL\t%llu\t%zu\n", manifest->total_bytes, manifest->total_chunks) < 0) {
		fclose(fp);
		return -1;
	}

	for (size_t index = 0U; index < manifest->file_count; ++index) {
		if (cs_manifest_write_file(fp, &manifest->files[index]) != 0) {
			fclose(fp);
			return -1;
		}
	}

	fclose(fp);
	return 0;
}

static char *cs_trim_line(char *line) {
	size_t length;

	if (line == NULL) {
		return NULL;
	}

	length = strlen(line);
	while (length > 0U && (line[length - 1U] == '\n' || line[length - 1U] == '\r')) {
		line[--length] = '\0';
	}
	return line;
}

int cs_manifest_load(const char *path, cs_manifest_t *manifest) {
	FILE *fp;
	char line[1024];
	cs_manifest_file_t *current_file = NULL;

	if (path == NULL || manifest == NULL) {
		return -1;
	}

	if (cs_manifest_init(manifest) != 0) {
		return -1;
	}

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return -1;
	}

	while (fgets(line, sizeof(line), fp) != NULL) {
		char *cursor;
		char *kind;

		if (cs_trim_line(line) == NULL || line[0] == '\0') {
			continue;
		}

		cursor = strchr(line, '\t');
		if (cursor == NULL) {
			continue;
		}
		*cursor++ = '\0';
		kind = line;

		if (strcmp(kind, "FILE") == 0) {
			char *path_field = cursor;
			char *mtime_field;
			char *size_field;
			char *chunk_count_field;

			mtime_field = strchr(path_field, '\t');
			if (mtime_field == NULL) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}
			*mtime_field++ = '\0';
			size_field = strchr(mtime_field, '\t');
			if (size_field == NULL) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}
			*size_field++ = '\0';
			chunk_count_field = strchr(size_field, '\t');
			if (chunk_count_field == NULL) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}
			*chunk_count_field++ = '\0';

			if (cs_manifest_add_file(manifest, path_field, (time_t)strtoll(mtime_field, NULL, 10), (unsigned long long)strtoull(size_field, NULL, 10), &current_file) != 0) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}
			(void)chunk_count_field;
		} else if (strcmp(kind, "CHUNK") == 0) {
			char *offset_field = cursor;
			char *size_field;
			char *hash_field;

			if (current_file == NULL) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}

			size_field = strchr(offset_field, '\t');
			if (size_field == NULL) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}
			*size_field++ = '\0';
			hash_field = strchr(size_field, '\t');
			if (hash_field == NULL) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}
			*hash_field++ = '\0';

			if (cs_manifest_add_chunk(manifest, current_file, (size_t)strtoull(offset_field, NULL, 10), (size_t)strtoull(size_field, NULL, 10), hash_field) != 0) {
				fclose(fp);
				cs_manifest_free(manifest);
				return -1;
			}
		} else if (strcmp(kind, "END") == 0) {
			current_file = NULL;
		}
	}

	fclose(fp);
	return 0;
}

