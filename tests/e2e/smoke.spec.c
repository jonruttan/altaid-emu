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

static char *test_version_exits_success(void)
{
	_it_should(
		"exit 0 for --version",
		0 == helper_system_status("./altaid-emu --version")
	);

	return NULL;
}

static char *test_cass_play_requires_file(void)
{
	_it_should(
		"exit non-zero for --cass-play without --cass",
		0 != helper_system_status("./altaid-emu --cass-play")
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_help_exits_success);
	_run_test(test_version_exits_success);
	_run_test(test_cass_play_requires_file);

	return NULL;
}
