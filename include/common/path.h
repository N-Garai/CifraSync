#ifndef CIFRASYNC_COMMON_PATH_H
#define CIFRASYNC_COMMON_PATH_H

#include <stdbool.h>
#include <stddef.h>

char cs_path_separator(void);

int cs_path_join(char *out, size_t out_size, const char *left, const char *right);
int cs_path_normalize_copy(const char *input, char *output, size_t output_size);
int cs_path_normalize_inplace(char *path, size_t path_size);

bool cs_path_is_absolute(const char *path);
const char *cs_path_basename(const char *path);
int cs_path_dirname(const char *path, char *out, size_t out_size);
bool cs_path_has_parent_reference(const char *path);

#endif
