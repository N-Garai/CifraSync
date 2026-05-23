#include "../integration/test_support.h"

#include "compress/codec.h"

int cs_unit_test_codec(void) {
	static const unsigned char plain[] = "aaaaabbbccccccccxyz";
	unsigned char *compressed = NULL;
	size_t compressed_size = 0U;
	unsigned char *decompressed = NULL;
	size_t decompressed_size = 0U;
	cs_codec_kind_t codec;
	unsigned char fixed_buffer[128];
	size_t fixed_size = sizeof(fixed_buffer);
	int status;

	if (cs_codec_from_name("rle", &codec) != 0 || codec != CS_CODEC_RLE) {
		return cs_it_fail("codec name lookup failed");
	}

	status = cs_codec_compress_alloc(codec, plain, sizeof(plain) - 1U, &compressed, &compressed_size);
	if (status != 0) {
		return cs_it_fail("rle compression failed");
	}
	status = cs_codec_decompress_alloc(codec, compressed, compressed_size, &decompressed, &decompressed_size);
	free(compressed);
	if (status != 0) {
		return cs_it_fail("rle decompression failed");
	}
	if (decompressed_size != sizeof(plain) - 1U || memcmp(decompressed, plain, decompressed_size) != 0) {
		free(decompressed);
		return cs_it_fail("rle roundtrip changed the payload");
	}
	free(decompressed);

	fixed_size = sizeof(fixed_buffer);
	if (cs_codec_compress(CS_CODEC_NONE, plain, sizeof(plain) - 1U, fixed_buffer, &fixed_size) != 0) {
		return cs_it_fail("copy codec compression failed");
	}
	if (fixed_size != sizeof(plain) - 1U || memcmp(fixed_buffer, plain, fixed_size) != 0) {
		return cs_it_fail("copy codec did not preserve input bytes");
	}

	return 0;
}
