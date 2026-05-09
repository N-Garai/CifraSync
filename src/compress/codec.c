#include "compress/codec.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

enum {
	CS_CODEC_OK = 0,
	CS_CODEC_ERR_INVALID = -1,
	CS_CODEC_ERR_BUFFER = -2,
	CS_CODEC_ERR_NOMEM = -3
};

const char *cs_codec_name(cs_codec_kind_t codec) {
	switch (codec) {
		case CS_CODEC_NONE:
			return "none";
		case CS_CODEC_RLE:
			return "rle";
		default:
			return "unknown";
	}
}

int cs_codec_from_name(const char *name, cs_codec_kind_t *out_codec) {
	if (name == NULL || out_codec == NULL) {
		return CS_CODEC_ERR_INVALID;
	}

	if (strcmp(name, "none") == 0) {
		*out_codec = CS_CODEC_NONE;
		return CS_CODEC_OK;
	}
	if (strcmp(name, "rle") == 0) {
		*out_codec = CS_CODEC_RLE;
		return CS_CODEC_OK;
	}

	return CS_CODEC_ERR_INVALID;
}

size_t cs_codec_max_compressed_size(cs_codec_kind_t codec, size_t input_size) {
	switch (codec) {
		case CS_CODEC_NONE:
			return input_size;
		case CS_CODEC_RLE:
			if (input_size > (SIZE_MAX / 2U)) {
				return 0U;
			}
			return input_size * 2U;
		default:
			return 0U;
	}
}

static int copy_codec(const unsigned char *input,
	size_t input_size,
	unsigned char *output,
	size_t *out_size) {
	if (out_size == NULL) {
		return CS_CODEC_ERR_INVALID;
	}
	if (input_size > 0U && (input == NULL || output == NULL)) {
		return CS_CODEC_ERR_INVALID;
	}
	if (*out_size < input_size) {
		return CS_CODEC_ERR_BUFFER;
	}
	if (input_size > 0U) {
		memcpy(output, input, input_size);
	}
	*out_size = input_size;
	return CS_CODEC_OK;
}

static int rle_compress(const unsigned char *input,
	size_t input_size,
	unsigned char *output,
	size_t *out_size) {
	size_t in_i;
	size_t out_i;

	if (out_size == NULL) {
		return CS_CODEC_ERR_INVALID;
	}
	if (input_size > 0U && (input == NULL || output == NULL)) {
		return CS_CODEC_ERR_INVALID;
	}

	in_i = 0U;
	out_i = 0U;
	while (in_i < input_size) {
		unsigned char value;
		unsigned char run;

		value = input[in_i];
		run = 1U;
		while ((size_t)run < 255U && (in_i + (size_t)run) < input_size && input[in_i + (size_t)run] == value) {
			run++;
		}

		if (out_i + 2U > *out_size) {
			return CS_CODEC_ERR_BUFFER;
		}
		output[out_i++] = run;
		output[out_i++] = value;

		in_i += (size_t)run;
	}

	*out_size = out_i;
	return CS_CODEC_OK;
}

static int rle_decompress(const unsigned char *input,
	size_t input_size,
	unsigned char *output,
	size_t *out_size) {
	size_t in_i;
	size_t out_i;

	if (out_size == NULL) {
		return CS_CODEC_ERR_INVALID;
	}
	if ((input_size % 2U) != 0U) {
		return CS_CODEC_ERR_INVALID;
	}
	if (input_size > 0U && (input == NULL || output == NULL)) {
		return CS_CODEC_ERR_INVALID;
	}

	in_i = 0U;
	out_i = 0U;
	while (in_i < input_size) {
		unsigned char run = input[in_i++];
		unsigned char value = input[in_i++];
		size_t run_size = (size_t)run;

		if (run == 0U) {
			return CS_CODEC_ERR_INVALID;
		}
		if (out_i + run_size > *out_size) {
			return CS_CODEC_ERR_BUFFER;
		}

		memset(output + out_i, (int)value, run_size);
		out_i += run_size;
	}

	*out_size = out_i;
	return CS_CODEC_OK;
}

int cs_codec_compress(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char *output,
	size_t *out_size) {
	switch (codec) {
		case CS_CODEC_NONE:
			return copy_codec(input, input_size, output, out_size);
		case CS_CODEC_RLE:
			return rle_compress(input, input_size, output, out_size);
		default:
			return CS_CODEC_ERR_INVALID;
	}
}

int cs_codec_decompress(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char *output,
	size_t *out_size) {
	switch (codec) {
		case CS_CODEC_NONE:
			return copy_codec(input, input_size, output, out_size);
		case CS_CODEC_RLE:
			return rle_decompress(input, input_size, output, out_size);
		default:
			return CS_CODEC_ERR_INVALID;
	}
}

int cs_codec_compress_alloc(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char **out_data,
	size_t *out_size) {
	size_t capacity;
	unsigned char *buffer;
	size_t written;
	int rc;

	if (out_data == NULL || out_size == NULL) {
		return CS_CODEC_ERR_INVALID;
	}

	capacity = cs_codec_max_compressed_size(codec, input_size);
	if (capacity == 0U && input_size != 0U) {
		return CS_CODEC_ERR_INVALID;
	}
	if (capacity == 0U) {
		capacity = 1U;
	}

	buffer = (unsigned char *)malloc(capacity);
	if (buffer == NULL) {
		return CS_CODEC_ERR_NOMEM;
	}

	written = capacity;
	rc = cs_codec_compress(codec, input, input_size, buffer, &written);
	if (rc != CS_CODEC_OK) {
		free(buffer);
		return rc;
	}

	*out_data = buffer;
	*out_size = written;
	return CS_CODEC_OK;
}

int cs_codec_decompress_alloc(cs_codec_kind_t codec,
	const unsigned char *input,
	size_t input_size,
	unsigned char **out_data,
	size_t *out_size) {
	size_t capacity;
	unsigned char *buffer;
	size_t written;
	int rc;

	if (out_data == NULL || out_size == NULL || input == NULL) {
		return CS_CODEC_ERR_INVALID;
	}

	if (codec == CS_CODEC_NONE) {
		capacity = input_size;
	} else if (codec == CS_CODEC_RLE) {
		size_t in_i;
		if ((input_size % 2U) != 0U) {
			return CS_CODEC_ERR_INVALID;
		}
		capacity = 0U;
		for (in_i = 0U; in_i < input_size; in_i += 2U) {
			size_t run_size = (size_t)input[in_i];
			if (run_size == 0U || capacity > (SIZE_MAX - run_size)) {
				return CS_CODEC_ERR_INVALID;
			}
			capacity += run_size;
		}
	} else {
		return CS_CODEC_ERR_INVALID;
	}

	if (capacity == 0U) {
		capacity = 1U;
	}

	buffer = (unsigned char *)malloc(capacity);
	if (buffer == NULL) {
		return CS_CODEC_ERR_NOMEM;
	}

	written = capacity;
	rc = cs_codec_decompress(codec, input, input_size, buffer, &written);
	if (rc != CS_CODEC_OK) {
		free(buffer);
		return rc;
	}

	*out_data = buffer;
	*out_size = written;
	return CS_CODEC_OK;
}
