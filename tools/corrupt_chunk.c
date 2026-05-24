#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/constants.h"
#include "common/path.h"
#include "delta/hash.h"
#include "util/io_utils.h"

typedef struct cs_corrupt_options {
	const char *repo_path;
	const char *hash_hex;
	size_t offset;
	unsigned char xor_mask;
} cs_corrupt_options_t;

static void cs_corrupt_options_init(cs_corrupt_options_t *options) {
	if (options == NULL) {
		return;
	}
	options->repo_path = NULL;
	options->hash_hex = NULL;
	options->offset = 0U;
	options->xor_mask = 0xffU;
}

static int cs_parse_size_arg(const char *text, size_t *value) {
	char *end = NULL;
	unsigned long long parsed;

	if (text == NULL || value == NULL || text[0] == '\0') {
		return -1;
	}
	errno = 0;
	parsed = strtoull(text, &end, 10);
	if (errno != 0 || end == text || (end != NULL && *end != '\0')) {
		return -1;
	}
	*value = (size_t)parsed;
	return 0;
}

static int cs_parse_byte_arg(const char *text, unsigned char *value) {
	char *end = NULL;
	unsigned long parsed;

	if (text == NULL || value == NULL || text[0] == '\0') {
		return -1;
	}
	errno = 0;
	parsed = strtoul(text, &end, 0);
	if (errno != 0 || end == text || (end != NULL && *end != '\0') || parsed > 0xffUL) {
		return -1;
	}
	*value = (unsigned char)parsed;
	return 0;
}

static int cs_corrupt_parse_cli(int argc, char **argv, cs_corrupt_options_t *options) {
	int index;

	if (options == NULL) {
		return -1;
	}
	cs_corrupt_options_init(options);

	for (index = 1; index < argc; ++index) {
		const char *arg = argv[index];
		const char *next = (index + 1 < argc) ? argv[index + 1] : NULL;

		if (arg == NULL) {
			continue;
		}
		if (strcmp(arg, "--repo") == 0) {
			if (next == NULL) {
				return -1;
			}
			options->repo_path = next;
			++index;
			continue;
		}
		if (strcmp(arg, "--hash") == 0) {
			if (next == NULL) {
				return -1;
			}
			options->hash_hex = next;
			++index;
			continue;
		}
		if (strcmp(arg, "--offset") == 0) {
			if (next == NULL || cs_parse_size_arg(next, &options->offset) != 0) {
				return -1;
			}
			++index;
			continue;
		}
		if (strcmp(arg, "--xor") == 0) {
			if (next == NULL || cs_parse_byte_arg(next, &options->xor_mask) != 0) {
				return -1;
			}
			++index;
			continue;
		}
		if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
			return 1;
		}
		return -1;
	}

	if (options->repo_path == NULL || options->hash_hex == NULL) {
		return -1;
	}
	if (!cs_hash_is_hex(options->hash_hex)) {
		return -1;
	}

	return 0;
}

static int cs_build_chunk_path(const char *repo_path, const char *hash_hex, char *chunk_path, size_t chunk_path_size) {
	char chunks_dir[CS_PATH_CAP];
	char prefix_dir[CS_PATH_CAP];
	char prefix[3];

	if (repo_path == NULL || hash_hex == NULL || chunk_path == NULL || chunk_path_size == 0U) {
		return -1;
	}
	if (strlen(hash_hex) < 2U) {
		return -1;
	}

	prefix[0] = hash_hex[0];
	prefix[1] = hash_hex[1];
	prefix[2] = '\0';

	if (cs_path_join(chunks_dir, sizeof(chunks_dir), repo_path, "chunks") != 0) {
		return -1;
	}
	if (cs_path_join(prefix_dir, sizeof(prefix_dir), chunks_dir, prefix) != 0) {
		return -1;
	}
	if (cs_path_join(chunk_path, chunk_path_size, prefix_dir, hash_hex) != 0) {
		return -1;
	}
	return 0;
}

int main(int argc, char **argv) {
	cs_corrupt_options_t options;
	char chunk_path[CS_PATH_CAP];
	unsigned char *chunk_data = NULL;
	size_t chunk_size = 0U;
	int parse_status;

	parse_status = cs_corrupt_parse_cli(argc, argv, &options);
	if (parse_status > 0) {
		printf("Usage: %s --repo PATH --hash HEX [--offset N] [--xor BYTE]\n", argv[0]);
		return 0;
	}
	if (parse_status != 0) {
		fprintf(stderr, "corrupt_chunk: invalid arguments\n");
		return 1;
	}

	if (cs_build_chunk_path(options.repo_path, options.hash_hex, chunk_path, sizeof(chunk_path)) != 0) {
		fprintf(stderr, "corrupt_chunk: could not resolve chunk path\n");
		return 1;
	}
	if (cs_io_read_file(chunk_path, &chunk_data, &chunk_size) != 0) {
		fprintf(stderr, "corrupt_chunk: failed to read chunk '%s'\n", chunk_path);
		return 1;
	}
	if (chunk_size == 0U) {
		free(chunk_data);
		fprintf(stderr, "corrupt_chunk: chunk is empty\n");
		return 1;
	}
	if (options.offset >= chunk_size) {
		free(chunk_data);
		fprintf(stderr, "corrupt_chunk: offset is out of range\n");
		return 1;
	}

	chunk_data[options.offset] ^= options.xor_mask;
	if (cs_io_write_file(chunk_path, chunk_data, chunk_size) != 0) {
		free(chunk_data);
		fprintf(stderr, "corrupt_chunk: failed to write corrupted chunk\n");
		return 1;
	}

	printf("corrupted: %s offset=%lu xor=0x%02x\n", chunk_path, (unsigned long)options.offset, (unsigned int)options.xor_mask);
	free(chunk_data);
	return 0;
}

