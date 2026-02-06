/*
 * emu_core.spec.c
 *
 * Incomplete tests for emu_core.c functions.
 */

#include "test-runner.h"
#include "emu_core.h"

static char *test_emu_core_txbuf_clear(void)
{
	return NULL;
}

static char *test_emu_core_txbuf_putch_cb(void)
{
	return NULL;
}

static char *test_emu_core_emu_core_tx_pop(void)
{
	return NULL;
}

static char *test_emu_core_emu_core_init(void)
{
	return NULL;
}

static char *test_emu_core_emu_core_load_rom64k(void)
{
	return NULL;
}

static char *test_emu_core_emu_core_reset(void)
{
	return NULL;
}

static char *test_emu_core_set_hw_lines(void)
{
	return NULL;
}

static char *test_emu_core_emu_core_run_batch(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_emu_core_txbuf_clear);
	_run_test(test_emu_core_txbuf_putch_cb);
	_run_test(test_emu_core_emu_core_tx_pop);
	_run_test(test_emu_core_emu_core_init);
	_run_test(test_emu_core_emu_core_load_rom64k);
	_run_test(test_emu_core_emu_core_reset);
	_run_test(test_emu_core_set_hw_lines);
	_run_test(test_emu_core_emu_core_run_batch);
	return NULL;
}
