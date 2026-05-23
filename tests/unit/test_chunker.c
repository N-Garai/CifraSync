#include "../integration/test_support.h"

#include "delta/chunker.h"
#include "delta/hash.h"

typedef struct cs_unit_chunk_capture {
	const unsigned char *source;
	size_t source_size;
	size_t chunk_count;
} cs_unit_chunk_capture_t;

static int cs_unit_chunk_visit(const cs_chunk_slice_t *slice, const unsigned char *chunk_data, void *ctx) {
	cs_unit_chunk_capture_t *capture = (cs_unit_chunk_capture_t *)ctx;
	char hash_hex[CS_HASH_HEX_BUFSZ];

	if (slice == NULL || chunk_data == NULL || capture == NULL) {
		return -1;
	}
	if (cs_hash_sha256_hex(chunk_data, slice->size, hash_hex) != 0) {
		return -1;
	}
	if (strcmp(hash_hex, slice->hash_hex) != 0) {
		return -1;
	}
	if (slice->offset + slice->size > capture->source_size || memcmp(capture->source + slice->offset, chunk_data, slice->size) != 0) {
		return -1;
	}

	capture->chunk_count++;
	return 0;
}

int cs_unit_test_chunker(void) {
	static const unsigned char data[] = "abcdefg";
	cs_chunker_t chunker;
	cs_unit_chunk_capture_t capture;

	if (cs_chunker_init(&chunker, 3U) != 0) {
		return cs_it_fail("chunker init failed");
	}
	capture.source = data;
	capture.source_size = sizeof(data) - 1U;
	capture.chunk_count = 0U;
	if (cs_chunker_estimate_count(&chunker, capture.source_size) != 3U) {
		return cs_it_fail("chunker estimate count was incorrect");
	}
	if (cs_chunker_chunk_buffer(&chunker, data, capture.source_size, cs_unit_chunk_visit, &capture) != 0) {
		return cs_it_fail("chunker did not iterate the buffer successfully");
	}
	if (capture.chunk_count != 3U) {
		return cs_it_fail("chunker emitted an unexpected number of slices");
	}

	return 0;
}
