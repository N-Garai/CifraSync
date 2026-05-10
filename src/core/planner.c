#include "core/planner.h"
#include "common/path.h"
#include "common/log.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

static int scan_path(const char *path, cs_plan_t *plan) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (S_ISREG(st.st_mode)) {
        plan->file_count += 1;
        plan->total_bytes += (size_t)st.st_size;
        return 0;
    }
    /* For directories, do a simple non-recursive scan using opendir/readdir would be better, but leave simple for now */
    return 0;
}

int cs_planner_create_plan(const char *source_path, cs_plan_t *out_plan) {
    if (!source_path || !out_plan) return -1;
    out_plan->file_count = 0;
    out_plan->total_bytes = 0;
    if (scan_path(source_path, out_plan) != 0) {
        CS_LOG_WARN("planner: scan failed for %s", source_path);
        return -1;
    }
    return 0;
}

void cs_planner_free(cs_plan_t *plan) {
    /* nothing to free in this simple plan */
    (void)plan;
}
