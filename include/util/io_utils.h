#ifndef CIFRASYNC_UTIL_IO_UTILS_H
#define CIFRASYNC_UTIL_IO_UTILS_H

#include <stdio.h>
#include <stddef.h>

/* Read exactly len bytes from file, handling partial reads */
int cs_io_read_all(FILE *fp, unsigned char *buffer, size_t len, size_t *bytes_read);

/* Write exactly len bytes to file, handling partial writes */
int cs_io_write_all(FILE *fp, const unsigned char *buffer, size_t len, size_t *bytes_written);

/* Read entire file into allocated buffer (caller must free) */
int cs_io_read_file(const char *path, unsigned char **buffer, size_t *size);

/* Write entire buffer to file */
int cs_io_write_file(const char *path, const unsigned char *buffer, size_t size);

/* Read a line from file (up to max_len bytes, including null terminator) */
int cs_io_read_line(FILE *fp, char *buffer, size_t max_len);

/* Safe fopen wrapper with Windows long-path support */
FILE *cs_io_fopen(const char *path, const char *mode);

/* Safe fclose wrapper */
int cs_io_fclose(FILE *fp);

#endif
