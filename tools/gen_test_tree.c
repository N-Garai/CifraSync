#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/constants.h"
#include "common/path.h"
#include "util/io_utils.h"

#ifdef _WIN32
#include <direct.h>
#define cs_tool_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define cs_tool_mkdir(path) mkdir(path, 0755)
#endif

typedef struct cs_tree_options {
	const char *root_path;
	size_t depth;
	size_t branches;
	size_t files_per_dir;
} cs_tree_options_t;

static void cs_tree_options_init(cs_tree_options_t *options) {
	if (options == NULL) {
		return;
	}
	options->root_path = NULL;
	options->depth = 3U;
	options->branches = 2U;
	options->files_per_dir = 3U;
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

static int cs_tree_parse_cli(int argc, char **argv, cs_tree_options_t *options) {
	int index;

	if (options == NULL) {
		return -1;
	}
	cs_tree_options_init(options);

	for (index = 1; index < argc; ++index) {
		const char *arg = argv[index];
		const char *next = (index + 1 < argc) ? argv[index + 1] : NULL;

		if (arg == NULL) {
			continue;
		}
		if (strcmp(arg, "--root") == 0) {
			if (next == NULL) {
				return -1;
			}
			options->root_path = next;
			++index;
			continue;
		}
		if (strcmp(arg, "--depth") == 0) {
			if (next == NULL || cs_parse_size_arg(next, &options->depth) != 0) {
				return -1;
			}
			++index;
			continue;
		}
		if (strcmp(arg, "--branches") == 0) {
			if (next == NULL || cs_parse_size_arg(next, &options->branches) != 0) {
				return -1;
			}
			++index;
			continue;
		}
		if (strcmp(arg, "--files") == 0) {
			if (next == NULL || cs_parse_size_arg(next, &options->files_per_dir) != 0) {
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

	if (options->root_path == NULL || options->depth == 0U || options->branches == 0U || options->files_per_dir == 0U) {
		return -1;
	}

	return 0;
}

static int cs_mkdir_one(const char *path) {
	if (path == NULL || path[0] == '\0') {
		return -1;
	}
	if (cs_tool_mkdir(path) == 0) {
		return 0;
	}
	if (errno == EEXIST) {
		return 0;
	}
	return -1;
}

static int cs_mkdir_p(const char *path) {
	char temp[CS_PATH_CAP];
	size_t length;
	size_t index;

	if (path == NULL) {
		return -1;
	}
	length = strlen(path);
	if (length == 0U || length >= sizeof(temp)) {
		return -1;
	}
	memcpy(temp, path, length + 1U);
	if (cs_mkdir_one(temp) == 0) {
		return 0;
	}

	for (index = 1U; index < length; ++index) {
		if (temp[index] == '/' || temp[index] == '\\') {
			char saved = temp[index];
			temp[index] = '\0';
			if (!(index == 2U && temp[1] == ':')) {
				if (cs_mkdir_one(temp) != 0) {
					return -1;
				}
			}
			temp[index] = saved;
		}
	}

	return cs_mkdir_one(temp);
}

static int cs_write_text_file(const char *path, const char *text) {
	return cs_io_write_file(path, (const unsigned char *)text, strlen(text));
}

static int cs_generate_level(const char *base_path,
	size_t level,
	const cs_tree_options_t *options,
	size_t *directory_count,
	size_t *file_count) {
	char child_path[CS_PATH_CAP];
	size_t branch_index;
	size_t file_index;

	if (base_path == NULL || options == NULL || directory_count == NULL || file_count == NULL) {
		return -1;
	}
	if (cs_mkdir_p(base_path) != 0) {
		return -1;
	}
	(*directory_count)++;

	for (file_index = 0U; file_index < options->files_per_dir; ++file_index) {
		char content[256];
		snprintf(content,
			sizeof(content),
			"tree file level=%lu file=%lu base=%s\n",
			(unsigned long)level,
			(unsigned long)file_index,
			base_path);
		if (snprintf(child_path, sizeof(child_path), "%s%cfile_%lu_%lu.txt", base_path, cs_path_separator(), (unsigned long)level, (unsigned long)file_index) < 0) {
			return -1;
		}
		if (cs_write_text_file(child_path, content) != 0) {
			return -1;
		}
		(*file_count)++;
	}

	if (level >= options->depth) {
		return 0;
	}

	for (branch_index = 0U; branch_index < options->branches; ++branch_index) {
		char next_path[CS_PATH_CAP];
		if (snprintf(next_path, sizeof(next_path), "%s%cdir_%lu_%lu", base_path, cs_path_separator(), (unsigned long)level, (unsigned long)branch_index) < 0) {
			return -1;
		}
		if (cs_generate_level(next_path, level + 1U, options, directory_count, file_count) != 0) {
			return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv) {
	cs_tree_options_t options;
	size_t directory_count = 0U;
	size_t file_count = 0U;
	int parse_status;

	parse_status = cs_tree_parse_cli(argc, argv, &options);
	if (parse_status > 0) {
		printf("Usage: %s --root PATH [--depth N] [--branches N] [--files N]\n", argv[0]);
		return 0;
	}
	if (parse_status != 0) {
		fprintf(stderr, "gen_test_tree: invalid arguments\n");
		return 1;
	}

	if (cs_generate_level(options.root_path, 0U, &options, &directory_count, &file_count) != 0) {
		fprintf(stderr, "gen_test_tree: failed to create test tree\n");
		return 1;
	}

	printf("root: %s\n", options.root_path);
	printf("directories: %lu\n", (unsigned long)directory_count);
	printf("files: %lu\n", (unsigned long)file_count);
	return 0;
}

