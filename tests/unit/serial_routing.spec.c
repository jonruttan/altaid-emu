/*
 * serial_routing.spec.c
 *
 * Unit tests for serial output routing rules.
 */

#include "serial_routing.c"

#include "test-runner.h"

static char *test_tui_routes_to_ui_fd(void)
{
	int fd;

	fd = serial_routing_fd(2, true, true, EMU_FD_UNSPEC, EMU_FD_UNSPEC, true);

	_it_should(
		"tui routes serial to ui stream",
		2 == fd
	);

	return NULL;
}

static char *test_panel_prefers_ui_stream(void)
{
	int fd;

	fd = serial_routing_fd(2, false, true, EMU_FD_UNSPEC, EMU_FD_UNSPEC, true);

	_it_should(
		"panel prefers ui stream when stdout matches",
		2 == fd
	);

	return NULL;
}

static char *test_explicit_serial_fd_wins(void)
{
	int fd;

	fd = serial_routing_fd(2, false, true, 1, EMU_FD_UNSPEC, true);

	_it_should(
		"explicit serial spec wins",
		1 == fd
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_tui_routes_to_ui_fd);
	_run_test(test_panel_prefers_ui_stream);
	_run_test(test_explicit_serial_fd_wins);

	return NULL;
}
