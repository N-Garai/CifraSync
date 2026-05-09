#ifndef CIFRASYNC_COMMON_MEMORY_H
#define CIFRASYNC_COMMON_MEMORY_H

#include <stddef.h>

void *cs_malloc(size_t size);
void *cs_calloc(size_t count, size_t elem_size);
void *cs_realloc(void *ptr, size_t size);
void cs_free(void *ptr);

char *cs_strdup(const char *src);
void *cs_memdup(const void *src, size_t size);
void cs_memzero(void *ptr, size_t size);

#endif
