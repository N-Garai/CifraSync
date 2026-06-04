#include "common/errors.h"

const char *cs_error_to_string(int code) {
	switch (code) {
		case CS_OK: return "success";
		case CS_ERR_USAGE: return "usage error";
		case CS_ERR_INVALID: return "invalid argument";
		case CS_ERR_UNSUPPORTED: return "unsupported operation";
		case CS_ERR_IO: return "I/O error";
		case CS_ERR_NOMEM: return "out of memory";
		case CS_ERR_NOT_FOUND: return "not found";
		case CS_ERR_INTERNAL: return "internal error";
		default: return "unknown error";
	}
}
