#include <stdio.h>

#include "test_backup_restore.c"
#include "test_incremental_resume.c"
#include "test_remote_sync.c"
#include "test_verify_prune.c"

typedef int (*cs_it_fn)(void);

static int cs_it_run(const char *name, cs_it_fn fn) {
	int status;

	status = fn();
	if (status == 0) {
		printf("[PASS] %s\n", name);
	} else {
		printf("[FAIL] %s\n", name);
	}
	return status;
}

int main(void) {
	int failures = 0;

	failures += cs_it_run("backup_restore", cs_integration_test_backup_restore);
	failures += cs_it_run("incremental_resume", cs_integration_test_incremental_resume);
	failures += cs_it_run("remote_sync", cs_integration_test_remote_sync);
	failures += cs_it_run("verify_prune", cs_integration_test_verify_prune);

	if (failures == 0) {
		puts("[PASS] all integration tests completed");
		return 0;
	}

	printf("[FAIL] %d integration test(s) failed\n", failures);
	return 1;
}