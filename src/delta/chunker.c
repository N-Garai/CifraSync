#include "delta/chunker.h"

#include "common/memory.h"

#include <stdio.h>
#include <string.h>

int cs_chunker_init(cs_chunker_t *chunker, size_t chunk_size) {
	if (chunker == NULL || chunk_size == 0U) {
		return -1;
	}
	chunker->chunk_size = chunk_size;
	return 0;
}

void cs_chunker_reset(cs_chunker_t *chunker) {
	if (chunker == NULL) {
		return;
	}
	chunker->chunk_size = 0U;
}

size_t cs_chunker_chunk_size(const cs_chunker_t *chunker) {
	if (chunker == NULL || chunker->chunk_size == 0U) {
		return CS_DELTA_DEFAULT_CHUNK_SIZE;
	}
	return chunker->chunk_size;
}

size_t cs_chunker_estimate_count(const cs_chunker_t *chunker, size_t input_size) {
	size_t chunk_size;

	chunk_size = cs_chunker_chunk_size(chunker);
	if (input_size == 0U) {
		return 0U;
	}
	return (input_size + chunk_size - 1U) / chunk_size;
}

int cs_chunker_chunk_buffer(const cs_chunker_t *chunker,
	const unsigned char *data,
	size_t data_size,
	cs_chunker_visit_fn visit,
	void *ctx) {
	size_t chunk_size;

	if (visit == NULL || (data == NULL && data_size > 0U)) {
		return -1;
	}

	chunk_size = cs_chunker_chunk_size(chunker);
	if (chunk_size == 0U) {
		return -1;
	}

	if (data_size == 0U) {
		cs_chunk_slice_t slice;
		unsigned char hash_bytes[CS_HASH_SHA256_BYTES];
		cs_memzero(&slice, sizeof(slice));
		if (cs_hash_sha256(NULL, 0U, hash_bytes) != 0) {
			cs_memzero(hash_bytes, sizeof(hash_bytes));
			return -1;
		}
		if (cs_hash_bytes_to_hex(hash_bytes, sizeof(hash_bytes), slice.hash_hex, sizeof(slice.hash_hex)) != 0) {
			cs_memzero(hash_bytes, sizeof(hash_bytes));
			return -1;
		}
		cs_memzero(hash_bytes, sizeof(hash_bytes));
		return visit(&slice, data, ctx);
	}

	for (size_t offset = 0U; offset < data_size; offset += chunk_size) {
		cs_chunk_slice_t slice;
		const size_t current_size = (data_size - offset) < chunk_size ? (data_size - offset) : chunk_size;
		unsigned char hash_bytes[CS_HASH_SHA256_BYTES];
		int status;

		cs_memzero(&slice, sizeof(slice));
		slice.offset = offset;
		slice.size = current_size;

		status = cs_hash_sha256(data + offset, current_size, hash_bytes);
		if (status != 0) {
			cs_memzero(hash_bytes, sizeof(hash_bytes));
			return -1;
		}
		if (cs_hash_bytes_to_hex(hash_bytes, sizeof(hash_bytes), slice.hash_hex, sizeof(slice.hash_hex)) != 0) {
			cs_memzero(hash_bytes, sizeof(hash_bytes));
			return -1;
		}
		cs_memzero(hash_bytes, sizeof(hash_bytes));

		status = visit(&slice, data + offset, ctx);
		if (status != 0) {
			return status;
		}
	}

	return 0;
}

