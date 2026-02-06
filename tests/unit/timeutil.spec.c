/*
 * timeutil.spec.c
 *
 * Unit tests for time helpers.
 */

#include "timeutil.c"

#include "test-runner.h"

static char *test_emu_tick_to_usec_basic(void)
{
	_it_should(
		"return 0 when hz is 0",
		0 == emu_tick_to_usec(123, 0)
	);

	_it_should(
		"convert ticks to usec at 2Hz",
		0 == emu_tick_to_usec(0, 2)
		&& 500000ull == emu_tick_to_usec(1, 2)
		&& 1000000ull == emu_tick_to_usec(2, 2)
		&& 1500000ull == emu_tick_to_usec(3, 2)
	);

	return NULL;
}

static char *test_emu_tick_to_usec_rounding(void)
{
	_it_should(
		"round using quotient/remainder math",
		1250000ull == emu_tick_to_usec(5, 4)
		&& 1750000ull == emu_tick_to_usec(7, 4)
	);

	return NULL;
}

static char *test_monotonic_usec_non_decreasing(void)
{
	uint32_t a = monotonic_usec();
	uint32_t b = monotonic_usec();

	_it_should(
		"return non-decreasing values",
		b >= a
	);

	return NULL;
}

static char *test_sleep_usec_zero_noop(void)
{
	uint32_t start;
	uint32_t end;

	start = monotonic_usec();
	sleep_usec(0);
	end = monotonic_usec();

	_it_should(
		"sleep_usec(0) returns quickly",
		(end - start) < 20000ull
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_emu_tick_to_usec_basic);
	_run_test(test_emu_tick_to_usec_rounding);
	_run_test(test_monotonic_usec_non_decreasing);
	_run_test(test_sleep_usec_zero_noop);

	return NULL;
}
