#include "../integration/test_support.h"

#include "fs/scanner.h"

typedef struct cs_unit_scan_ctx {
	unsigned long files_seen;
} cs_unit_scan_ctx_t;

static int cs_unit_scan_visit(const cs_fs_metadata_t *metadata, void *ctx) {
	cs_unit_scan_ctx_t *scan_ctx = (cs_unit_scan_ctx_t *)ctx;

	if (metadata == NULL || scan_ctx == NULL) {
		return -1;
	}
	if (!cs_fs_metadata_is_file(metadata)) {
		return 0;
	}
	scan_ctx->files_seen++;
	return 0;
}

int cs_unit_test_scanner(void) {
	char base[CS_IT_PATH_CAP];
	char source_root[CS_IT_PATH_CAP];
	cs_fs_scan_options_t options;
	cs_unit_scan_ctx_t ctx;

	if (cs_it_make_temp_root("scanner", base, sizeof(base)) != 0) {
		return cs_it_fail("unable to create temp root");
	}
	if (cs_it_join_path(source_root, sizeof(source_root), base, "source") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to build source path");
	}
	if (cs_it_write_relative_text_file(source_root, "alpha.txt", "a\n") != 0 ||
		cs_it_write_relative_text_file(source_root, "nested/beta.txt", "b\n") != 0 ||
		cs_it_write_relative_text_file(source_root, "nested/deeper/gamma.txt", "c\n") != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("unable to build source tree");
	}

	cs_fs_scan_options_default(&options);
	options.include_directories = 0;
	ctx.files_seen = 0U;
	if (cs_fs_scan(source_root, &options, cs_unit_scan_visit, &ctx) != 0) {
		cs_it_remove_tree(base);
		return cs_it_fail("scanner traversal failed");
	}
	if (ctx.files_seen != 3UL) {
		cs_it_remove_tree(base);
		return cs_it_fail("scanner file count mismatch");
	}

	cs_it_remove_tree(base);
	return 0;
}
