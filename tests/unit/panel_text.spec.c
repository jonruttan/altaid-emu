/*
 * panel_text.spec.c
 *
 * Incomplete tests for panel_text.c functions.
 */

#include "test-runner.h"
#include "panel_text.h"

static char *test_panel_text_panel_text_set_output(void)
{
	return NULL;
}

static char *test_panel_text_out_stream(void)
{
	return NULL;
}

static char *test_panel_text_panel_text_set_compact(void)
{
	return NULL;
}

static char *test_panel_text_panel_text_set_emit_mode(void)
{
	return NULL;
}

static char *test_panel_text_panel_text_begin(void)
{
	return NULL;
}

static char *test_panel_text_panel_text_end(void)
{
	return NULL;
}

static char *test_panel_text_snapshot_from(void)
{
	return NULL;
}

static char *test_panel_text_snapshot_equal(void)
{
	return NULL;
}

static char *test_panel_text_bits16(void)
{
	return NULL;
}

static char *test_panel_text_bits8(void)
{
	return NULL;
}

static char *test_panel_text_bits4(void)
{
	return NULL;
}

static char *test_panel_text_panel_text_render(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_panel_text_panel_text_set_output);
	_run_test(test_panel_text_out_stream);
	_run_test(test_panel_text_panel_text_set_compact);
	_run_test(test_panel_text_panel_text_set_emit_mode);
	_run_test(test_panel_text_panel_text_begin);
	_run_test(test_panel_text_panel_text_end);
	_run_test(test_panel_text_snapshot_from);
	_run_test(test_panel_text_snapshot_equal);
	_run_test(test_panel_text_bits16);
	_run_test(test_panel_text_bits8);
	_run_test(test_panel_text_bits4);
	_run_test(test_panel_text_panel_text_render);
	return NULL;
}
