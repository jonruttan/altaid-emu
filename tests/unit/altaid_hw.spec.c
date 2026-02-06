/*
 * altaid_hw.spec.c
 *
 * Incomplete tests for altaid_hw.c functions.
 */

#include "test-runner.h"
#include "altaid_hw.h"

static char *test_altaid_hw_recompute_ram_bank(void)
{
	return NULL;
}

static char *test_altaid_hw_panel_latch_if_complete(void)
{
	return NULL;
}

static char *test_altaid_hw_panel_switch_nibble_for_row(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_init(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_reset_runtime(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_load_rom64k(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_mem_read(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_mem_write(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_io_in(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_io_out(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_press_key(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_tick(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_addr16(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_data8(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_stat4(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_altaid_hw_recompute_ram_bank);
	_run_test(test_altaid_hw_panel_latch_if_complete);
	_run_test(test_altaid_hw_panel_switch_nibble_for_row);
	_run_test(test_altaid_hw_altaid_hw_init);
	_run_test(test_altaid_hw_altaid_hw_reset_runtime);
	_run_test(test_altaid_hw_altaid_hw_load_rom64k);
	_run_test(test_altaid_hw_altaid_mem_read);
	_run_test(test_altaid_hw_altaid_mem_write);
	_run_test(test_altaid_hw_altaid_io_in);
	_run_test(test_altaid_hw_altaid_io_out);
	_run_test(test_altaid_hw_altaid_hw_panel_press_key);
	_run_test(test_altaid_hw_altaid_hw_panel_tick);
	_run_test(test_altaid_hw_altaid_hw_panel_addr16);
	_run_test(test_altaid_hw_altaid_hw_panel_data8);
	_run_test(test_altaid_hw_altaid_hw_panel_stat4);
	return NULL;
}
