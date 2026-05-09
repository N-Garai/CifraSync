#include "common/memory.h"

#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void *cs_malloc(size_t size) {
	if (size == 0U) {
		return NULL;
	}
	return malloc(size);
}

void *cs_calloc(size_t count, size_t elem_size) {
	if (count == 0U || elem_size == 0U) {
		return NULL;
	}
	if (count > (SIZE_MAX / elem_size)) {
		return NULL;
	}
	return calloc(count, elem_size);
}

void *cs_realloc(void *ptr, size_t size) {
	if (size == 0U) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, size);
}

void cs_free(void *ptr) {
	free(ptr);
}

char *cs_strdup(const char *src) {
	size_t len;
	char *copy;

	if (src == NULL) {
		return NULL;
	}

	len = strlen(src);
	copy = (char *)cs_malloc(len + 1U);
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, src, len + 1U);
	return copy;
}

void *cs_memdup(const void *src, size_t size) {
	void *copy;

	if (src == NULL || size == 0U) {
		return NULL;
	}

	copy = cs_malloc(size);
	if (copy == NULL) {
		return NULL;
	}

	memcpy(copy, src, size);
	return copy;
}

void cs_memzero(void *ptr, size_t size) {
	volatile unsigned char *cursor;
	size_t i;

	if (ptr == NULL || size == 0U) {
		return;
	}

	cursor = (volatile unsigned char *)ptr;
	for (i = 0U; i < size; ++i) {
		cursor[i] = 0U;
	}
}
