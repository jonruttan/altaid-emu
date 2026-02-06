/*
 * cli.spec.c
 *
 * Incomplete tests for cli.c functions.
 */

#include "test-runner.h"
#include "cli.h"

static char *test_cli_cfg_defaults(void)
{
	return NULL;
}

static char *test_cli_parse_u32(void)
{
	return NULL;
}

static char *test_cli_parse_i32(void)
{
	return NULL;
}

static char *test_cli_cli_usage(void)
{
	return NULL;
}

static char *test_cli_cli_parse_args(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_cli_cfg_defaults);
	_run_test(test_cli_parse_u32);
	_run_test(test_cli_parse_i32);
	_run_test(test_cli_cli_usage);
	_run_test(test_cli_cli_parse_args);
	return NULL;
}
