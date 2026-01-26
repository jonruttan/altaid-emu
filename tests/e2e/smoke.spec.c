/*
 * smoke.spec.c
 *
 * End-to-end smoke checks.
 *
 * These tests are allowed to shell out (system(3)) to exercise the built
 * emulator binary.
 */

#include "test-runner.h"
#include "test-helper-system.h"

static char *test_help_exits_success(void)
{
	_it_should(
		"exit 0 for --help",
		0 == helper_system_status("./altaid-emu --help")
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_help_exits_success);

	return NULL;
}
