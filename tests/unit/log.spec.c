/*
 * log.spec.c
 *
 * Incomplete tests for log.c functions.
 */

#include "test-runner.h"
#include "log.h"

static char *test_log_log_set_quiet(void)
{
	return NULL;
}

static char *test_log_log_open(void)
{
	return NULL;
}

static char *test_log_log_close(void)
{
	return NULL;
}

static char *test_log_log_vprintf(void)
{
	return NULL;
}

static char *test_log_log_printf(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_log_log_set_quiet);
	_run_test(test_log_log_open);
	_run_test(test_log_log_close);
	_run_test(test_log_log_vprintf);
	_run_test(test_log_log_printf);
	return NULL;
}
