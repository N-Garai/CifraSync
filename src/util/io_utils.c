#include "util/io_utils.h"

#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#define CS_MAX_PATH 32767
#else
#include <unistd.h>
#define CS_MAX_PATH 4096
#endif

/* Read exactly len bytes from file, handling partial reads */
int cs_io_read_all(FILE *fp, unsigned char *buffer, size_t len, size_t *bytes_read) {
	size_t total_read = 0;
	size_t chunk_size;
	
	if (fp == NULL || buffer == NULL || bytes_read == NULL) {
		return -1;
	}
	
	*bytes_read = 0;
	
	while (total_read < len) {
		chunk_size = fread(buffer + total_read, 1, len - total_read, fp);
		if (chunk_size == 0) {
			if (feof(fp)) {
				*bytes_read = total_read;
				return (total_read == len) ? 0 : -1;
			}
			if (ferror(fp)) {
				return -1;
			}
		}
		total_read += chunk_size;
	}
	
	*bytes_read = total_read;
	return 0;
}

/* Write exactly len bytes to file, handling partial writes */
int cs_io_write_all(FILE *fp, const unsigned char *buffer, size_t len, size_t *bytes_written) {
	size_t total_written = 0;
	size_t chunk_size;
	
	if (fp == NULL || buffer == NULL || bytes_written == NULL) {
		return -1;
	}
	
	*bytes_written = 0;
	
	while (total_written < len) {
		chunk_size = fwrite(buffer + total_written, 1, len - total_written, fp);
		if (chunk_size == 0) {
			if (ferror(fp)) {
				return -1;
			}
		}
		total_written += chunk_size;
	}
	
	*bytes_written = total_written;
	return (total_written == len) ? 0 : -1;
}

/* Read entire file into allocated buffer (caller must free) */
int cs_io_read_file(const char *path, unsigned char **buffer, size_t *size) {
	FILE *fp;
	unsigned char *data;
	long file_size;
	size_t bytes_read;
	int ret;
	
	if (path == NULL || buffer == NULL || size == NULL) {
		return -1;
	}
	
	fp = fopen(path, "rb");
	if (fp == NULL) {
		return -1;
	}
	
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return -1;
	}
	
	file_size = ftell(fp);
	if (file_size < 0) {
		fclose(fp);
		return -1;
	}
	
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return -1;
	}
	
	data = (unsigned char *)malloc((size_t)file_size + 1);
	if (data == NULL) {
		fclose(fp);
		return -1;
	}
	
	ret = cs_io_read_all(fp, data, (size_t)file_size, &bytes_read);
	fclose(fp);
	
	if (ret != 0 || bytes_read != (size_t)file_size) {
		free(data);
		return -1;
	}
	
	data[file_size] = '\0';
	*buffer = data;
	*size = (size_t)file_size;
	
	return 0;
}

/* Write entire buffer to file */
int cs_io_write_file(const char *path, const unsigned char *buffer, size_t size) {
	FILE *fp;
	size_t bytes_written;
	int ret;
	
	if (path == NULL || buffer == NULL) {
		return -1;
	}
	
	fp = fopen(path, "wb");
	if (fp == NULL) {
		return -1;
	}
	
	ret = cs_io_write_all(fp, buffer, size, &bytes_written);
	fclose(fp);
	
	if (ret != 0 || bytes_written != size) {
		return -1;
	}
	
	return 0;
}

/* Read a line from file (up to max_len bytes, including null terminator) */
int cs_io_read_line(FILE *fp, char *buffer, size_t max_len) {
	int c;
	size_t pos = 0;
	
	if (fp == NULL || buffer == NULL || max_len == 0) {
		return -1;
	}
	
	while (pos < max_len - 1) {
		c = fgetc(fp);
		if (c == EOF) {
			if (pos == 0) {
				return -1;
			}
			break;
		}
		if (c == '\n') {
			break;
		}
		if (c != '\r') {
			buffer[pos++] = (char)c;
		}
	}
	
	buffer[pos] = '\0';
	return (int)pos;
}

/* Safe fopen wrapper with Windows long-path support */
FILE *cs_io_fopen(const char *path, const char *mode) {
	FILE *fp;
	
	if (path == NULL || mode == NULL) {
		return NULL;
	}
	
	fp = fopen(path, mode);
	return fp;
}

/* Safe fclose wrapper */
int cs_io_fclose(FILE *fp) {
	if (fp == NULL) {
		return -1;
	}
	return fclose(fp);
}
