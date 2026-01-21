/*
 * smoke.spec.c
 *
 * Placeholder smoke test.
 *
 * It exists to verify that the test harness wiring works.
 */

#include "test-runner.h"
#include "i8080.c"

static char *test_smoke(void)
{
	_it_should("be able to perform a basic test", 1 == 1);

	return NULL;
}

static char *run_tests()
{
	_run_test(test_smoke);

	return NULL;
}
