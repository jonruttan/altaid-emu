/*
 * panel_ansi.spec.c
 *
 * Incomplete tests for panel_ansi.c functions.
 */

#include "test-runner.h"
#include "panel_ansi.h"

static char *test_panel_ansi_panel_ansi_set_output(void)
{
	return NULL;
}

static char *test_panel_ansi_out_stream(void)
{
	return NULL;
}

static char *test_panel_ansi_out_fd(void)
{
	return NULL;
}

static char *test_panel_ansi_term_write_full(void)
{
	return NULL;
}

static char *test_panel_ansi_clamp_int(void)
{
	return NULL;
}

static char *test_panel_ansi_env_int(void)
{
	return NULL;
}

static char *test_panel_ansi_probe_term_size(void)
{
	return NULL;
}

static char *test_panel_ansi_isatty(void)
{
	return NULL;
}

static char *test_panel_ansi_ioctl(void)
{
	return NULL;
}

static char *test_panel_ansi_recompute_layout(void)
{
	return NULL;
}

static char *test_panel_ansi_apply_split_region(void)
{
	return NULL;
}

static char *test_panel_ansi_ser_commit_line(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_serial_reset(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_serial_feed(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_is_tty(void)
{
	return NULL;
}

static char *test_panel_ansi_buf_append(void)
{
	return NULL;
}

static char *test_panel_ansi_is_csi_final(void)
{
	return NULL;
}

static char *test_panel_ansi_buf_append_visible(void)
{
	return NULL;
}

static char *test_panel_ansi_vbar_l(void)
{
	return NULL;
}

static char *test_panel_ansi_vbar_r(void)
{
	return NULL;
}

static char *test_panel_ansi_bordered_line(void)
{
	return NULL;
}

static char *test_panel_ansi_border_hline(void)
{
	return NULL;
}

static char *test_panel_ansi_border_top(void)
{
	return NULL;
}

static char *test_panel_ansi_border_mid(void)
{
	return NULL;
}

static char *test_panel_ansi_border_bottom(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_begin(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_end(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_refresh(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_ascii(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_split(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_panel_visible(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_serial_ro(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_statusline(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_status_override(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_clear_status_override(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_altscreen(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_set_term_size_override(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_handle_resize(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_goto_serial(void)
{
	return NULL;
}

static char *test_panel_ansi_led(void)
{
	return NULL;
}

static char *test_panel_ansi_led_bits16(void)
{
	return NULL;
}

static char *test_panel_ansi_led_bits8(void)
{
	return NULL;
}

static char *test_panel_ansi_led_bits4(void)
{
	return NULL;
}

static char *test_panel_ansi_button(void)
{
	return NULL;
}

static char *test_panel_ansi_data_buttons(void)
{
	return NULL;
}

static char *test_panel_ansi_ser_line_from_end(void)
{
	return NULL;
}

static char *test_panel_ansi_panel_ansi_render(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_panel_ansi_panel_ansi_set_output);
	_run_test(test_panel_ansi_out_stream);
	_run_test(test_panel_ansi_out_fd);
	_run_test(test_panel_ansi_term_write_full);
	_run_test(test_panel_ansi_clamp_int);
	_run_test(test_panel_ansi_env_int);
	_run_test(test_panel_ansi_probe_term_size);
	_run_test(test_panel_ansi_isatty);
	_run_test(test_panel_ansi_ioctl);
	_run_test(test_panel_ansi_recompute_layout);
	_run_test(test_panel_ansi_apply_split_region);
	_run_test(test_panel_ansi_ser_commit_line);
	_run_test(test_panel_ansi_panel_ansi_serial_reset);
	_run_test(test_panel_ansi_panel_ansi_serial_feed);
	_run_test(test_panel_ansi_panel_ansi_is_tty);
	_run_test(test_panel_ansi_buf_append);
	_run_test(test_panel_ansi_is_csi_final);
	_run_test(test_panel_ansi_buf_append_visible);
	_run_test(test_panel_ansi_vbar_l);
	_run_test(test_panel_ansi_vbar_r);
	_run_test(test_panel_ansi_bordered_line);
	_run_test(test_panel_ansi_border_hline);
	_run_test(test_panel_ansi_border_top);
	_run_test(test_panel_ansi_border_mid);
	_run_test(test_panel_ansi_border_bottom);
	_run_test(test_panel_ansi_panel_ansi_begin);
	_run_test(test_panel_ansi_panel_ansi_end);
	_run_test(test_panel_ansi_panel_ansi_set_refresh);
	_run_test(test_panel_ansi_panel_ansi_set_ascii);
	_run_test(test_panel_ansi_panel_ansi_set_split);
	_run_test(test_panel_ansi_panel_ansi_set_panel_visible);
	_run_test(test_panel_ansi_panel_ansi_set_serial_ro);
	_run_test(test_panel_ansi_panel_ansi_set_statusline);
	_run_test(test_panel_ansi_panel_ansi_set_status_override);
	_run_test(test_panel_ansi_panel_ansi_clear_status_override);
	_run_test(test_panel_ansi_panel_ansi_set_altscreen);
	_run_test(test_panel_ansi_panel_ansi_set_term_size_override);
	_run_test(test_panel_ansi_panel_ansi_handle_resize);
	_run_test(test_panel_ansi_panel_ansi_goto_serial);
	_run_test(test_panel_ansi_led);
	_run_test(test_panel_ansi_led_bits16);
	_run_test(test_panel_ansi_led_bits8);
	_run_test(test_panel_ansi_led_bits4);
	_run_test(test_panel_ansi_button);
	_run_test(test_panel_ansi_data_buttons);
	_run_test(test_panel_ansi_ser_line_from_end);
	_run_test(test_panel_ansi_panel_ansi_render);
	return NULL;
}
