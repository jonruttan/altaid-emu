/*
 * ui.spec.c
 *
 * Incomplete tests for ui.c functions.
 */

#include "test-runner.h"
#include "ui.h"

static char *test_ui_ui_set_output(void)
{
	return NULL;
}

static char *test_ui_out_stream(void)
{
	return NULL;
}

static char *test_ui_ui_help_string(void)
{
	return NULL;
}

static char *test_ui_panel_dump(void)
{
	return NULL;
}

static char *test_ui_prompt_begin(void)
{
	return NULL;
}

static char *test_ui_prompt_cancel(void)
{
	return NULL;
}

static char *test_ui_prompt_commit(void)
{
	return NULL;
}

static char *test_ui_prompt_handle_char(void)
{
	return NULL;
}

static char *test_ui_ui_init(void)
{
	return NULL;
}

static char *test_ui_ui_shutdown(void)
{
	return NULL;
}

static char *test_ui_ui_toggle_ui_mode(void)
{
	return NULL;
}

static char *test_ui_ui_toggle_panel(void)
{
	return NULL;
}

static char *test_ui_ui_toggle_panel_compact(void)
{
	return NULL;
}

static char *test_ui_ui_request_reset(void)
{
	return NULL;
}

static char *test_ui_ui_toggle_serial_ro(void)
{
	return NULL;
}

static char *test_ui_ui_toggle_pty_input(void)
{
	return NULL;
}

static char *test_ui_ui_handle_panel_key(void)
{
	return NULL;
}

static char *test_ui_ui_handle_prefixed(void)
{
	return NULL;
}

static char *test_ui_KEY_CTRL(void)
{
	return NULL;
}

static char *test_ui_ui_handle_normal_char(void)
{
	return NULL;
}

static char *test_ui_ui_poll(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_ui_ui_set_output);
	_run_test(test_ui_out_stream);
	_run_test(test_ui_ui_help_string);
	_run_test(test_ui_panel_dump);
	_run_test(test_ui_prompt_begin);
	_run_test(test_ui_prompt_cancel);
	_run_test(test_ui_prompt_commit);
	_run_test(test_ui_prompt_handle_char);
	_run_test(test_ui_ui_init);
	_run_test(test_ui_ui_shutdown);
	_run_test(test_ui_ui_toggle_ui_mode);
	_run_test(test_ui_ui_toggle_panel);
	_run_test(test_ui_ui_toggle_panel_compact);
	_run_test(test_ui_ui_request_reset);
	_run_test(test_ui_ui_toggle_serial_ro);
	_run_test(test_ui_ui_toggle_pty_input);
	_run_test(test_ui_ui_handle_panel_key);
	_run_test(test_ui_ui_handle_prefixed);
	_run_test(test_ui_KEY_CTRL);
	_run_test(test_ui_ui_handle_normal_char);
	_run_test(test_ui_ui_poll);
	return NULL;
}
