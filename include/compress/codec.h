#ifndef CIFRASYNC_COMPRESS_CODEC_H
#define CIFRASYNC_COMPRESS_CODEC_H

#include <stddef.h>

typedef enum cs_codec_kind {
	CS_CODEC_NONE = 0,
	CS_CODEC_RLE = 1
} cs_codec_kind_t;

const char *cs_codec_name(cs_codec_kind_t codec);
int cs_codec_from_name(const char *name, cs_codec_kind_t *out_codec);

/* Returns the worst-case compressed size needed for the codec and input length. */
size_t cs_codec_max_compressed_size(cs_codec_kind_t codec, size_t input_size);

/*
 * Compress/decompress into caller-provided output buffers.
 * out_size must contain capacity on input, and is updated to written size on success.
 */
int cs_codec_compress(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char *output,
	size_t *out_size);

int cs_codec_decompress(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char *output,
	size_t *out_size);

/*
 * Convenience alloc variants. Returned buffers must be freed with free().
 */
int cs_codec_compress_alloc(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char **out_data,
	size_t *out_size);

int cs_codec_decompress_alloc(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char **out_data,
	size_t *out_size);

#endif
