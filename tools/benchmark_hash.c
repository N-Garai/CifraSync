#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/constants.h"
#include "delta/hash.h"
#include "util/io_utils.h"

typedef struct cs_benchmark_options {
	const char *file_path;
	size_t buffer_size;
	size_t iterations;
} cs_benchmark_options_t;

static void cs_benchmark_options_init(cs_benchmark_options_t *options) {
	if (options == NULL) {
		return;
	}
	options->file_path = NULL;
	options->buffer_size = 4U * 1024U * 1024U;
	options->iterations = 32U;
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

static int cs_benchmark_parse_cli(int argc, char **argv, cs_benchmark_options_t *options) {
	int index;

	if (options == NULL) {
		return -1;
	}
	cs_benchmark_options_init(options);

	for (index = 1; index < argc; ++index) {
		const char *arg = argv[index];
		const char *next = (index + 1 < argc) ? argv[index + 1] : NULL;

		if (arg == NULL) {
			continue;
		}
		if (strcmp(arg, "--file") == 0) {
			if (next == NULL) {
				return -1;
			}
			options->file_path = next;
			++index;
			continue;
		}
		if (strcmp(arg, "--size") == 0) {
			if (next == NULL || cs_parse_size_arg(next, &options->buffer_size) != 0) {
				return -1;
			}
			++index;
			continue;
		}
		if (strcmp(arg, "--iterations") == 0) {
			if (next == NULL || cs_parse_size_arg(next, &options->iterations) != 0 || options->iterations == 0U) {
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

	return 0;
}

static void cs_fill_pattern(unsigned char *buffer, size_t size) {
	size_t index;

	for (index = 0U; index < size; ++index) {
		buffer[index] = (unsigned char)('A' + (index % 23U));
	}
}

int main(int argc, char **argv) {
	cs_benchmark_options_t options;
	unsigned char *buffer = NULL;
	size_t buffer_size = 0U;
	unsigned char digest[CS_HASH_SHA256_BYTES];
	char digest_hex[CS_HASH_HEX_BUFSZ];
	size_t iteration;
	clock_t start_ticks;
	clock_t end_ticks;
	double elapsed_seconds;
	int parse_status;

	parse_status = cs_benchmark_parse_cli(argc, argv, &options);
	if (parse_status > 0) {
		printf("Usage: %s [--file PATH] [--size BYTES] [--iterations N]\n", argv[0]);
		return 0;
	}
	if (parse_status != 0) {
		fprintf(stderr, "benchmark_hash: invalid arguments\n");
		return 1;
	}

	if (options.file_path != NULL) {
		if (cs_io_read_file(options.file_path, &buffer, &buffer_size) != 0) {
			fprintf(stderr, "benchmark_hash: failed to read file '%s'\n", options.file_path);
			return 1;
		}
	} else {
		buffer = (unsigned char *)malloc(options.buffer_size);
		if (buffer == NULL) {
			fprintf(stderr, "benchmark_hash: out of memory\n");
			return 1;
		}
		buffer_size = options.buffer_size;
		cs_fill_pattern(buffer, buffer_size);
	}

	start_ticks = clock();
	for (iteration = 0U; iteration < options.iterations; ++iteration) {
		if (cs_hash_sha256(buffer, buffer_size, digest) != 0) {
			free(buffer);
			fprintf(stderr, "benchmark_hash: hashing failed\n");
			return 1;
		}
	}
	end_ticks = clock();

	if (cs_hash_bytes_to_hex(digest, sizeof(digest), digest_hex, sizeof(digest_hex)) != 0) {
		free(buffer);
		fprintf(stderr, "benchmark_hash: hex conversion failed\n");
		return 1;
	}

	elapsed_seconds = (double)(end_ticks - start_ticks) / (double)CLOCKS_PER_SEC;
	if (elapsed_seconds <= 0.0) {
		elapsed_seconds = 0.000001;
	}

	printf("hash: %s\n", digest_hex);
	printf("bytes: %lu\n", (unsigned long)buffer_size);
	printf("iterations: %lu\n", (unsigned long)options.iterations);
	printf("elapsed_seconds: %.6f\n", elapsed_seconds);
	printf("throughput_mib_per_sec: %.2f\n", ((double)buffer_size * (double)options.iterations) / (1024.0 * 1024.0) / elapsed_seconds);

	free(buffer);
	return 0;
}

