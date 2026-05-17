#include "fs/file_reader.h"

#include "common/memory.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

static int cs_file_read_internal(const char *path, size_t offset, size_t length, unsigned char **out_data, size_t *out_size) {
	FILE *fp;
	unsigned char *buffer = NULL;
	size_t file_size;
	size_t want_size;
	size_t read_total = 0U;

	if (path == NULL || out_data == NULL || out_size == NULL) {
		return -1;
	}

	{
		struct stat st;
		if (stat(path, &st) != 0) {
			return -1;
		}
		if (st.st_size < 0) {
			return -1;
		}
		file_size = (size_t)st.st_size;
	}

	fp = fopen(path, "rb");
	if (fp == NULL) {
		return -1;
	}
	if (offset > file_size) {
		fclose(fp);
		return -1;
	}

	want_size = file_size - offset;
	if (length != 0U && length < want_size) {
		want_size = length;
	}

	if (want_size == 0U) {
		fclose(fp);
		*out_data = NULL;
		*out_size = 0U;
		return 0;
	}

	buffer = (unsigned char *)cs_malloc(want_size);
	if (buffer == NULL) {
		fclose(fp);
		return -1;
	}

	if (offset > (size_t)LONG_MAX || fseek(fp, (long)offset, SEEK_SET) != 0) {
		cs_free(buffer);
		fclose(fp);
		return -1;
	}

	while (read_total < want_size) {
		size_t chunk = fread(buffer + read_total, 1U, want_size - read_total, fp);
		if (chunk == 0U) {
			if (ferror(fp) != 0) {
				cs_free(buffer);
				fclose(fp);
				return -1;
			}
			break;
		}
		read_total += chunk;
	}

	fclose(fp);
	*out_data = buffer;
	*out_size = read_total;
	return 0;
}

int cs_file_read_all(const char *path, unsigned char **out_data, size_t *out_size) {
	return cs_file_read_internal(path, 0U, 0U, out_data, out_size);
}

int cs_file_read_range(const char *path, size_t offset, size_t length, unsigned char **out_data, size_t *out_size) {
	return cs_file_read_internal(path, offset, length, out_data, out_size);
}

int cs_file_read_into_buffer(const char *path, unsigned char *buffer, size_t buffer_size, size_t offset, size_t length, size_t *out_size) {
	unsigned char *data = NULL;
	size_t data_size = 0U;
	int status;

	if (buffer == NULL || buffer_size == 0U) {
		return -1;
	}

	status = cs_file_read_internal(path, offset, length, &data, &data_size);
	if (status != 0) {
		return status;
	}
	if (data_size > buffer_size) {
		cs_free(data);
		return -1;
	}

	memcpy(buffer, data, data_size);
	cs_free(data);
	if (out_size != NULL) {
		*out_size = data_size;
	}
	return 0;
}

int cs_file_write_all(const char *path, const unsigned char *data, size_t size) {
	FILE *fp;
	size_t written = 0U;

	if (path == NULL || (data == NULL && size > 0U)) {
		return -1;
	}

	fp = fopen(path, "wb");
	if (fp == NULL) {
		return -1;
	}

	while (written < size) {
		size_t chunk = fwrite(data + written, 1U, size - written, fp);
		if (chunk == 0U) {
			fclose(fp);
			return -1;
		}
		written += chunk;
	}

	if (fflush(fp) != 0) {
		fclose(fp);
		return -1;
	}

	fclose(fp);
	return 0;
}

