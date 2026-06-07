#include <stddef.h>
#include <stdio.h>

#include "integration/test_backup_restore.c"
#include "integration/test_encrypted_backup.c"
#include "integration/test_incremental_resume.c"
#include "integration/test_remote_sync.c"
#include "integration/test_verify_prune.c"
#include "unit/test_chunker.c"
#include "unit/test_cipher.c"
#include "unit/test_codec.c"
#include "unit/test_hash.c"
#include "unit/test_index_store.c"
#include "unit/test_parser.c"
#include "unit/test_repo.c"
#include "unit/test_scanner.c"
#include "unit/test_snapshot_store.c"

typedef int (*cs_test_fn)(void);

typedef struct cs_test_case {
	const char *name;
	cs_test_fn fn;
} cs_test_case_t;

static int cs_run_case(const cs_test_case_t *test_case) {
	int status;

	status = test_case->fn();
	if (status == 0) {
		printf("[PASS] %s\n", test_case->name);
	} else {
		printf("[FAIL] %s\n", test_case->name);
	}

	return status;
}

int main(void) {
	const cs_test_case_t tests[] = {
		{"unit_parser", cs_unit_test_parser},
		{"unit_hash", cs_unit_test_hash},
		{"unit_chunker", cs_unit_test_chunker},
		{"unit_codec", cs_unit_test_codec},
		{"unit_cipher", cs_unit_test_cipher},
		{"unit_repo", cs_unit_test_repo},
		{"unit_index_store", cs_unit_test_index_store},
		{"unit_snapshot_store", cs_unit_test_snapshot_store},
		{"unit_scanner", cs_unit_test_scanner},
		{"integration_backup_restore", cs_integration_test_backup_restore},
		{"integration_encrypted_backup", cs_integration_test_encrypted_backup},
		{"integration_incremental_resume", cs_integration_test_incremental_resume},
		{"integration_remote_sync", cs_integration_test_remote_sync},
		{"integration_verify_prune", cs_integration_test_verify_prune},
	};
	size_t failures = 0U;
	size_t index;

	for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
		if (cs_run_case(&tests[index]) != 0) {
			failures++;
		}
	}

	if (failures == 0U) {
		puts("[PASS] all unit and integration tests completed");
		return 0;
	}

	printf("[FAIL] %lu test(s) failed\n", (unsigned long)failures);
	return 1;
}