#ifndef CIFRASYNC_FS_FILE_READER_H
#define CIFRASYNC_FS_FILE_READER_H

#include <stddef.h>

int cs_file_read_all(const char *path, unsigned char **out_data, size_t *out_size);
int cs_file_read_range(const char *path, size_t offset, size_t length, unsigned char **out_data, size_t *out_size);
int cs_file_read_into_buffer(const char *path, unsigned char *buffer, size_t buffer_size, size_t offset, size_t length, size_t *out_size);
int cs_file_write_all(const char *path, const unsigned char *data, size_t size);

#endif

