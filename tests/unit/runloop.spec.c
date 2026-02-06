/*
 * runloop.spec.c
 *
 * Incomplete tests for runloop.c functions.
 */

#include "test-runner.h"
#include "emu_host.h"

static char *test_runloop_tty_bol_update_fd(void)
{
	return NULL;
}

static char *test_runloop_tx_drain(void)
{
	return NULL;
}

static char *test_runloop_same_tty(void)
{
	return NULL;
}

static char *test_runloop_text_snapshot_emit(void)
{
	return NULL;
}

static char *test_runloop_isatty(void)
{
	return NULL;
}

static char *test_runloop_pty_poll_input(void)
{
	return NULL;
}

static char *test_runloop_stdin_poll_input(void)
{
	return NULL;
}

static char *test_runloop_choose_ui_stream(void)
{
	return NULL;
}

static char *test_runloop_ui_term_fd_hint(void)
{
	return NULL;
}

static char *test_runloop_apply_output_streams(void)
{
	return NULL;
}

static char *test_runloop_compute_panel_policy(void)
{
	return NULL;
}

static char *test_runloop_compute_serial_routing(void)
{
	return NULL;
}

static char *test_runloop_manage_panel_lifecycle(void)
{
	return NULL;
}

static char *test_runloop_panel_render(void)
{
	return NULL;
}

static char *test_runloop_realtime_throttle(void)
{
	return NULL;
}

static char *test_runloop_emu_host_runloop(void)
{
	return NULL;
}

static char *test_runloop_cassette_open(void)
{
	return NULL;
}

static char *test_runloop_cassette_save(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_runloop_tty_bol_update_fd);
	_run_test(test_runloop_tx_drain);
	_run_test(test_runloop_same_tty);
	_run_test(test_runloop_text_snapshot_emit);
	_run_test(test_runloop_isatty);
	_run_test(test_runloop_pty_poll_input);
	_run_test(test_runloop_stdin_poll_input);
	_run_test(test_runloop_choose_ui_stream);
	_run_test(test_runloop_ui_term_fd_hint);
	_run_test(test_runloop_apply_output_streams);
	_run_test(test_runloop_compute_panel_policy);
	_run_test(test_runloop_compute_serial_routing);
	_run_test(test_runloop_manage_panel_lifecycle);
	_run_test(test_runloop_panel_render);
	_run_test(test_runloop_realtime_throttle);
	_run_test(test_runloop_emu_host_runloop);
	_run_test(test_runloop_cassette_open);
	_run_test(test_runloop_cassette_save);
	return NULL;
}
