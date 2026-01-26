/*
 * timeutil.spec.c
 *
 * Unit tests for time helpers.
 */

#include "timeutil.c"

#include "test-runner.h"

static char *test_emu_tick_to_ns_basic(void)
{
	_it_should(
		"return 0 when hz is 0",
		emu_tick_to_ns(123, 0) == 0
	);

	_it_should(
		"convert ticks to ns at 2Hz",
		emu_tick_to_ns(0, 2) == 0
		&& emu_tick_to_ns(1, 2) == 500000000ull
		&& emu_tick_to_ns(2, 2) == 1000000000ull
		&& emu_tick_to_ns(3, 2) == 1500000000ull
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_emu_tick_to_ns_basic);

	return NULL;
}
