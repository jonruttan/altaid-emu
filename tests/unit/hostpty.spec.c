/*
 * hostpty.spec.c
 *
 * Incomplete tests for hostpty.c functions.
 */

#include "test-runner.h"
#include "hostpty.h"

static char *test_hostpty_hostpty_open(void)
{
	return NULL;
}

static char *test_hostpty_unlockpt(void)
{
	return NULL;
}

static char *test_hostpty_hostpty_make_raw(void)
{
	return NULL;
}

static char *test_hostpty_tcgetattr(void)
{
	return NULL;
}

static char *test_hostpty_hostpty_make_raw_nonblocking(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_hostpty_hostpty_open);
	_run_test(test_hostpty_unlockpt);
	_run_test(test_hostpty_hostpty_make_raw);
	_run_test(test_hostpty_tcgetattr);
	_run_test(test_hostpty_hostpty_make_raw_nonblocking);
	return NULL;
}
