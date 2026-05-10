#ifndef CIFRASYNC_CORE_PLANNER_H
#define CIFRASYNC_CORE_PLANNER_H

#include <stddef.h>

typedef struct cs_plan {
	size_t file_count;
	size_t total_bytes;
} cs_plan_t;

/* Build a lightweight plan (counts) for a source path. */
int cs_planner_create_plan(const char *source_path, cs_plan_t *out_plan);
void cs_planner_free(cs_plan_t *plan);

#endif

