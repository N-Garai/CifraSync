#ifndef CIFRASYNC_DELTA_CHUNKER_H
#define CIFRASYNC_DELTA_CHUNKER_H

#include <stddef.h>

#include "delta/hash.h"

#define CS_DELTA_DEFAULT_CHUNK_SIZE (1024U * 1024U)

typedef struct cs_chunker {
	size_t chunk_size;
} cs_chunker_t;

typedef struct cs_chunk_slice {
	size_t offset;
	size_t size;
	char hash_hex[CS_HASH_HEX_BUFSZ];
} cs_chunk_slice_t;

typedef int (*cs_chunker_visit_fn)(const cs_chunk_slice_t *slice, const unsigned char *chunk_data, void *ctx);

int cs_chunker_init(cs_chunker_t *chunker, size_t chunk_size);
void cs_chunker_reset(cs_chunker_t *chunker);
size_t cs_chunker_chunk_size(const cs_chunker_t *chunker);

size_t cs_chunker_estimate_count(const cs_chunker_t *chunker, size_t input_size);
int cs_chunker_chunk_buffer(const cs_chunker_t *chunker,
	const unsigned char *data,
	size_t data_size,
	cs_chunker_visit_fn visit,
	void *ctx);

#endif

