#ifndef CIFRASYNC_COMMON_ERRORS_H
#define CIFRASYNC_COMMON_ERRORS_H

enum {
	CS_OK = 0,
	CS_ERR_USAGE = 1,
	CS_ERR_INVALID = 2,
	CS_ERR_UNSUPPORTED = 3,
	CS_ERR_IO = 4,
	CS_ERR_NOMEM = 5,
	CS_ERR_NOT_FOUND = 6,
	CS_ERR_INTERNAL = 7
};

const char *cs_error_to_string(int code);

#endif
