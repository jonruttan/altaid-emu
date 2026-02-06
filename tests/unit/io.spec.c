/*
 * io.spec.c
 *
 * Unit tests for I/O helpers.
 */

#include "timeutil.c"
#include "io.c"

#include "test-runner.h"

#include <string.h>
#include <unistd.h>

static char *test_write_full_invalid_args(void)
{
	const char buf[4] = { 'a', 'b', 'c', 'd' };

	_it_should(
		"return -1 when fd is invalid",
		-1 == write_full(-1, buf, sizeof(buf))
	);

	_it_should(
		"return 0 when buf is NULL",
		0 == write_full(1, NULL, sizeof(buf))
	);

	_it_should(
		"return 0 when len is 0",
		0 == write_full(1, buf, 0)
	);

	return NULL;
}

static char *test_write_full_pipe_success(void)
{
	int fds[2];
	const char msg[] = "ok";
	char out[8];
	ssize_t n;

	if (pipe(fds) != 0) {
		return "pipe() failed";
	}

	_it_should(
		"write full buffer to fd",
		0 == write_full(fds[1], msg, sizeof(msg))
	);

	n = read(fds[0], out, sizeof(out));
	_it_should(
		"read back full buffer",
		n == (ssize_t)sizeof(msg)
		&& 0 == memcmp(out, msg, sizeof(msg))
	);

	close(fds[0]);
	close(fds[1]);

	return NULL;
}

static char *test_sleep_or_wait_input_usec_zero_noop(void)
{
	uint32_t start;
	uint32_t end;

	start = monotonic_usec();
	sleep_or_wait_input_usec(0, false, -1, true);
	end = monotonic_usec();

	_it_should(
		"sleep_or_wait_input_usec(0) returns quickly",
		20000ull >= (end - start)
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_write_full_invalid_args);
	_run_test(test_write_full_pipe_success);
	_run_test(test_sleep_or_wait_input_usec_zero_noop);

	return NULL;
}
