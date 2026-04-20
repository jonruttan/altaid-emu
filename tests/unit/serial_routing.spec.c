/* SPDX-License-Identifier: MIT */

/*
 * serial_routing.spec.c
 *
 * Unit tests for serial output routing rules and the stdin dispatch
 * that splits bytes between panel-press events (Ctrl-P prefix) and
 * UART RX.
 */

#include "serial.c"
#include "altaid_hw.c"
#include "serial_routing.c"

#include "test-runner.h"

#include <stdarg.h>
#include <string.h>

/* Stub log_printf so altaid_hw.c's diagnostic path links in tests. */
void log_printf(const char *fmt, ...)
{
	(void)fmt;
}

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

/*
 * Helpers for the stdin-dispatch tests: a fresh SerialDev + AltaidHW
 * pair plus a zeroed StdinPanelState.
 */
static void dispatch_setup(SerialDev *ser, AltaidHW *hw,
			   struct StdinPanelState *state)
{
	serial_init(ser, 2000000u, 9600u);
	altaid_hw_init(hw);
	state->prefix_active = false;
}

/* Count bytes currently queued for RX (head-to-tail on the ring). */
static uint32_t rx_queue_len(const SerialDev *ser)
{
	return (uint32_t)((ser->rx_qt - ser->rx_qh) & SERIAL_RX_QUEUE_MASK);
}

static char *test_stdin_dispatch_plain_bytes_flow_to_rx(void)
{
	SerialDev ser;
	AltaidHW hw;
	struct StdinPanelState state;
	const uint8_t buf[] = { 'h', 'i' };

	dispatch_setup(&ser, &hw, &state);

	serial_routing_stdin_dispatch(buf, sizeof(buf), &ser, &hw,
				      0ull, 1000ull, &state);

	_it_should(
		"plain text bytes reach the UART RX queue",
		2u == rx_queue_len(&ser)
		&& (uint8_t)'h' == ser.rx_q[0]
		&& (uint8_t)'i' == ser.rx_q[1]
		&& false == state.prefix_active
		&& false == hw.fp_key_down[0]
	);

	return NULL;
}

static char *test_stdin_dispatch_ctrl_p_chord_presses_d(void)
{
	SerialDev ser;
	AltaidHW hw;
	struct StdinPanelState state;
	const uint8_t buf[] = { 0x10, '3' };  /* Ctrl-P + '3' -> D2 */

	dispatch_setup(&ser, &hw, &state);

	serial_routing_stdin_dispatch(buf, sizeof(buf), &ser, &hw,
				      1000ull, 500ull, &state);

	_it_should(
		"Ctrl-P + '3' presses D2 and enqueues no RX bytes",
		0u == rx_queue_len(&ser)
		&& true == hw.fp_key_down[2]
		&& 1500ull == hw.fp_key_until[2]
		&& false == state.prefix_active
	);

	return NULL;
}

static char *test_stdin_dispatch_ctrl_p_chord_presses_run_mode_next(void)
{
	SerialDev ser;
	AltaidHW hw;
	struct StdinPanelState state;

	/* RUN */
	dispatch_setup(&ser, &hw, &state);
	{
		const uint8_t b[] = { 0x10, 'r' };
		serial_routing_stdin_dispatch(b, sizeof(b), &ser, &hw,
					      0ull, 100ull, &state);
	}
	_it_should("Ctrl-P r presses RUN (key 8)",
		true == hw.fp_key_down[8]);

	/* MODE */
	dispatch_setup(&ser, &hw, &state);
	{
		const uint8_t b[] = { 0x10, 'm' };
		serial_routing_stdin_dispatch(b, sizeof(b), &ser, &hw,
					      0ull, 100ull, &state);
	}
	_it_should("Ctrl-P m presses MODE (key 9)",
		true == hw.fp_key_down[9]);

	/* NEXT */
	dispatch_setup(&ser, &hw, &state);
	{
		const uint8_t b[] = { 0x10, 'n' };
		serial_routing_stdin_dispatch(b, sizeof(b), &ser, &hw,
					      0ull, 100ull, &state);
	}
	_it_should("Ctrl-P n presses NEXT (key 10)",
		true == hw.fp_key_down[10]);

	/* N chord: D7 + NEXT */
	dispatch_setup(&ser, &hw, &state);
	{
		const uint8_t b[] = { 0x10, 'N' };
		serial_routing_stdin_dispatch(b, sizeof(b), &ser, &hw,
					      0ull, 100ull, &state);
	}
	_it_should("Ctrl-P N presses D7 + NEXT as a chord",
		true == hw.fp_key_down[7]
		&& true == hw.fp_key_down[10]);

	return NULL;
}

static char *test_stdin_dispatch_unknown_chord_is_dropped(void)
{
	SerialDev ser;
	AltaidHW hw;
	struct StdinPanelState state;
	const uint8_t buf[] = { 0x10, 'z', 'A' };  /* unknown chord, then 'A' */

	dispatch_setup(&ser, &hw, &state);

	serial_routing_stdin_dispatch(buf, sizeof(buf), &ser, &hw,
				      0ull, 100ull, &state);

	_it_should(
		"unknown chord is dropped, prefix clears, subsequent byte reaches RX",
		false == state.prefix_active
		&& 1u == rx_queue_len(&ser)
		&& (uint8_t)'A' == ser.rx_q[0]
	);

	return NULL;
}

static char *test_stdin_dispatch_prefix_spans_two_polls(void)
{
	SerialDev ser;
	AltaidHW hw;
	struct StdinPanelState state;
	const uint8_t first[] = { 0x10 };
	const uint8_t second[] = { '3' };

	dispatch_setup(&ser, &hw, &state);

	serial_routing_stdin_dispatch(first, sizeof(first), &ser, &hw,
				      0ull, 100ull, &state);
	_it_should(
		"isolated Ctrl-P leaves prefix active and queue empty",
		true == state.prefix_active
		&& 0u == rx_queue_len(&ser)
		&& false == hw.fp_key_down[2]
	);

	serial_routing_stdin_dispatch(second, sizeof(second), &ser, &hw,
				      0ull, 100ull, &state);
	_it_should(
		"follow-up chord byte completes the press",
		false == state.prefix_active
		&& 0u == rx_queue_len(&ser)
		&& true == hw.fp_key_down[2]
	);

	return NULL;
}

static char *test_stdin_dispatch_newline_translated_to_cr(void)
{
	SerialDev ser;
	AltaidHW hw;
	struct StdinPanelState state;
	const uint8_t buf[] = { 'a', '\n' };

	dispatch_setup(&ser, &hw, &state);

	serial_routing_stdin_dispatch(buf, sizeof(buf), &ser, &hw,
				      0ull, 100ull, &state);

	_it_should(
		"'\\n' translated to '\\r' on the RX queue",
		2u == rx_queue_len(&ser)
		&& (uint8_t)'a' == ser.rx_q[0]
		&& (uint8_t)'\r' == ser.rx_q[1]
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_tui_routes_to_ui_fd);
	_run_test(test_panel_prefers_ui_stream);
	_run_test(test_explicit_serial_fd_wins);
	_run_test(test_stdin_dispatch_plain_bytes_flow_to_rx);
	_run_test(test_stdin_dispatch_ctrl_p_chord_presses_d);
	_run_test(test_stdin_dispatch_ctrl_p_chord_presses_run_mode_next);
	_run_test(test_stdin_dispatch_unknown_chord_is_dropped);
	_run_test(test_stdin_dispatch_prefix_spans_two_polls);
	_run_test(test_stdin_dispatch_newline_translated_to_cr);

	return NULL;
}
