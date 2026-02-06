/*
 * emu.spec.c
 *
 * Incomplete tests for emu.c functions.
 */

#include "test-runner.h"
#include "emu.h"

static char *test_emu_emu_init(void)
{
	return NULL;
}

static char *test_emu_emu_shutdown(void)
{
	return NULL;
}

static char *test_emu_emu_reset(void)
{
	return NULL;
}

static char *test_emu_emu_run(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_emu_emu_init);
	_run_test(test_emu_emu_shutdown);
	_run_test(test_emu_emu_reset);
	_run_test(test_emu_emu_run);
	return NULL;
}
