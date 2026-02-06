/*
 * io.spec.c
 *
 * Unit tests for I/O helpers.
 */

/*
 * Force io.c to use in-memory file helpers instead of host syscalls.
 * Must be defined before io.c includes io_sys.h.
 */
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "test-helper-file.h"

#define ALTAID_IO_READ  helper_file_read
#define ALTAID_IO_WRITE helper_file_write

#include "timeutil.c"
#include "io.c"

#include "test-runner.h"

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

static char *test_write_full_in_memory_success(void)
{
	const int fd = TEST_HELPER_FILE_FILE1;
	const char msg[] = "ok";
	char capture[32];

	memset(capture, 0, sizeof(capture));
	helper_file_buffer_ptr[fd] = capture;
	helper_file_buffer_length[fd] = sizeof(capture) - 1;
	helper_file_reset();

	_it_should(
		"write full buffer to fd",
		0 == write_full(fd, msg, sizeof(msg))
	);

	_it_should(
		"capture full buffer in memory",
		0 == memcmp(helper_file_str(fd), msg, sizeof(msg))
	);

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
	_run_test(test_write_full_in_memory_success);
	_run_test(test_sleep_or_wait_input_usec_zero_noop);

	return NULL;
}
