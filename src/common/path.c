#include "common/path.h"

#include <ctype.h>
#include <string.h>

static int is_sep(char ch) {
	return ch == '/' || ch == '\\';
}

char cs_path_separator(void) {
#ifdef _WIN32
	return '\\';
#else
	return '/';
#endif
}

static size_t copy_normalized(const char *input, char *output, size_t output_size, int *ok) {
	char sep;
	size_t in_i;
	size_t out_i;
	int last_was_sep;

	*ok = 0;
	if (input == NULL || output == NULL || output_size == 0U) {
		return 0U;
	}

	sep = cs_path_separator();
	in_i = 0U;
	out_i = 0U;
	last_was_sep = 0;

	while (input[in_i] != '\0') {
		char ch = input[in_i++];

		if (is_sep(ch)) {
			if (last_was_sep) {
				continue;
			}
			ch = sep;
			last_was_sep = 1;
		} else {
			last_was_sep = 0;
		}

		if (out_i + 1U >= output_size) {
			return 0U;
		}
		output[out_i++] = ch;
	}

	if (out_i == 0U) {
		if (output_size < 2U) {
			return 0U;
		}
		output[out_i++] = '.';
	}

	if (out_i > 1U && output[out_i - 1U] == sep) {
#ifdef _WIN32
		if (!(out_i == 3U && isalpha((unsigned char)output[0]) != 0 && output[1] == ':')) {
			out_i--;
		}
#else
		out_i--;
#endif
	}

	output[out_i] = '\0';
	*ok = 1;
	return out_i;
}

int cs_path_normalize_copy(const char *input, char *output, size_t output_size) {
	int ok;
	(void)copy_normalized(input, output, output_size, &ok);
	return ok ? 0 : -1;
}

int cs_path_normalize_inplace(char *path, size_t path_size) {
	int ok;
	size_t length;

	if (path == NULL || path_size == 0U) {
		return -1;
	}

	length = copy_normalized(path, path, path_size, &ok);
	(void)length;
	return ok ? 0 : -1;
}

int cs_path_join(char *out, size_t out_size, const char *left, const char *right) {
	char sep;
	size_t l_len;
	size_t r_start;
	size_t i;

	if (out == NULL || out_size == 0U || left == NULL || right == NULL) {
		return -1;
	}

	sep = cs_path_separator();
	l_len = strlen(left);
	r_start = 0U;

	while (right[r_start] != '\0' && is_sep(right[r_start])) {
		r_start++;
	}

	if (l_len == 0U) {
		if (cs_path_normalize_copy(right + r_start, out, out_size) != 0) {
			return -1;
		}
		return 0;
	}

	if (r_start >= strlen(right)) {
		if (cs_path_normalize_copy(left, out, out_size) != 0) {
			return -1;
		}
		return 0;
	}

	i = 0U;
	if (l_len + 2U + strlen(right + r_start) > out_size) {
		return -1;
	}

	memcpy(out, left, l_len);
	i = l_len;

	if (i > 0U && !is_sep(out[i - 1U])) {
		out[i++] = sep;
	}

	memcpy(out + i, right + r_start, strlen(right + r_start) + 1U);
	return cs_path_normalize_inplace(out, out_size);
}

bool cs_path_is_absolute(const char *path) {
	if (path == NULL || path[0] == '\0') {
		return false;
	}

#ifdef _WIN32
	if ((isalpha((unsigned char)path[0]) != 0) && path[1] == ':' && is_sep(path[2])) {
		return true;
	}
	if (is_sep(path[0]) && is_sep(path[1])) {
		return true;
	}
	return false;
#else
	return path[0] == '/';
#endif
}

const char *cs_path_basename(const char *path) {
	const char *cursor;
	const char *last;

	if (path == NULL || path[0] == '\0') {
		return path;
	}

	last = path;
	cursor = path;
	while (*cursor != '\0') {
		if (is_sep(*cursor)) {
			last = cursor + 1;
		}
		cursor++;
	}
	return last;
}

int cs_path_dirname(const char *path, char *out, size_t out_size) {
	const char *base;
	size_t dir_len;

	if (path == NULL || out == NULL || out_size == 0U) {
		return -1;
	}

	base = cs_path_basename(path);
	if (base == path) {
		if (out_size < 2U) {
			return -1;
		}
		out[0] = '.';
		out[1] = '\0';
		return 0;
	}

	dir_len = (size_t)(base - path);
	while (dir_len > 1U && is_sep(path[dir_len - 1U])) {
		dir_len--;
	}

	if (dir_len + 1U > out_size) {
		return -1;
	}

	memcpy(out, path, dir_len);
	out[dir_len] = '\0';
	return 0;
}

bool cs_path_has_parent_reference(const char *path) {
	const char *segment;
	const char *cursor;

	if (path == NULL) {
		return false;
	}

	segment = path;
	cursor = path;
	while (1) {
		if (*cursor == '\0' || is_sep(*cursor)) {
			size_t len = (size_t)(cursor - segment);
			if (len == 2U && segment[0] == '.' && segment[1] == '.') {
				return true;
			}
			if (*cursor == '\0') {
				break;
			}
			segment = cursor + 1;
		}
		cursor++;
	}

	return false;
}
