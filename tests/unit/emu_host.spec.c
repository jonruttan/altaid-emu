/*
 * emu_host.spec.c
 *
 * Incomplete tests for emu_host.c functions.
 */

#include "test-runner.h"
#include "emu_host.h"

static char *test_emu_host_parse_serial_fd(void)
{
	return NULL;
}

static char *test_emu_host_open_serial_out_file(void)
{
	return NULL;
}

static char *test_emu_host_resolve_serial_dest(void)
{
	return NULL;
}

static char *test_emu_host_emu_host_init(void)
{
	return NULL;
}

static char *test_emu_host_cassette_open(void)
{
	return NULL;
}

static char *test_emu_host_emu_host_shutdown(void)
{
	return NULL;
}

static char *test_emu_host_emu_host_epoch_reset(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_emu_host_parse_serial_fd);
	_run_test(test_emu_host_open_serial_out_file);
	_run_test(test_emu_host_resolve_serial_dest);
	_run_test(test_emu_host_emu_host_init);
	_run_test(test_emu_host_cassette_open);
	_run_test(test_emu_host_emu_host_shutdown);
	_run_test(test_emu_host_emu_host_epoch_reset);
	return NULL;
}
