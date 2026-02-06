/*
 * main.spec.c
 *
 * Incomplete tests for main.c functions.
 */

#include "test-runner.h"

static char *test_main_on_signal(void)
{
	return NULL;
}

static char *test_main_main(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_main_on_signal);
	_run_test(test_main_main);
	return NULL;
}
