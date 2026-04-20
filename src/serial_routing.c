/* SPDX-License-Identifier: MIT */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "serial_routing.h"

#include "emu_host.h"
#include "io_sys.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int serial_routing_fd(int ui_fd, bool tui_active, bool panel_visible,
	int serial_spec_fd, int serial_override_fd, bool ui_stdout_same_tty)
{
	/*
	 * In TUI mode, keep cursor-control sequences and serial bytes on the same
	 * terminal stream to avoid panel corruption from cross-stream interleaving.
	 */
	if (tui_active && ui_fd >= 0) {
		if (serial_spec_fd == EMU_FD_UNSPEC ||
			serial_spec_fd == STDOUT_FILENO ||
			serial_spec_fd == STDERR_FILENO) {
			return ui_fd;
		}
		if (serial_override_fd == STDOUT_FILENO ||
			serial_override_fd == STDERR_FILENO) {
			return ui_fd;
		}
	}

	/*
	 * Non-TUI panel snapshots are written to the UI stream (stderr by default).
	 * If serial output defaults to stdout, the two streams can interleave out of
	 * order on the same terminal. When the panel is enabled and the serial output
	 * destination is unspecified, prefer the UI stream to keep output ordered.
	 */
	if (!tui_active && panel_visible && ui_fd >= 0) {
		if (serial_spec_fd == EMU_FD_UNSPEC &&
			serial_override_fd == EMU_FD_UNSPEC &&
			ui_stdout_same_tty) {
			return ui_fd;
		}
	}

	if (serial_spec_fd != EMU_FD_UNSPEC)
		return serial_spec_fd;
	if (serial_override_fd != EMU_FD_UNSPEC)
		return serial_override_fd;
	return STDOUT_FILENO;
}

void serial_routing_pty_poll(int pty_fd, SerialDev *ser)
{
	fd_set rfds;
	struct timeval tv;
	uint8_t buf[256];
	int r;

	if (pty_fd < 0 || !ser) return;

	FD_ZERO(&rfds);
	FD_SET(pty_fd, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	r = select(pty_fd + 1, &rfds, NULL, NULL, &tv);
	if (r <= 0) return;

	for (;;) {
		ssize_t n = ALTAID_IO_READ(pty_fd, buf, sizeof(buf));
		if (n > 0) {
			ssize_t i;
			for (i = 0; i < n; i++)
			serial_host_enqueue(ser, buf[i]);

			FD_ZERO(&rfds);
			FD_SET(pty_fd, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 0;
			r = select(pty_fd + 1, &rfds, NULL, NULL, &tv);
			if (r <= 0) return;
			continue;
		}
		if (n == 0) return;
		if (errno == EAGAIN || errno == EWOULDBLOCK) return;
		return;
	}
}

void serial_routing_stdin_poll(SerialDev *ser)
{
	uint8_t buf[256];

	if (!ser) return;

	for (;;) {
		ssize_t n = ALTAID_IO_READ(STDIN_FILENO, buf, sizeof(buf));
		if (n > 0) {
			ssize_t i;
			for (i = 0; i < n; i++) {
				uint8_t ch = buf[i];
				if (ch == (uint8_t)'\n')
				ch = (uint8_t)'\r';
				serial_host_enqueue(ser, ch);
			}
			continue;
		}
		if (n == 0) return;
		if (errno == EAGAIN || errno == EWOULDBLOCK) return;
		return;
	}
}

/*
 * Ctrl-P panel-prefix byte (matches TUI's panel-prefix in src/ui.c).
 */
#define PANEL_PREFIX_BYTE	0x10u

/*
 * Apply a single chord byte that followed Ctrl-P.  Returns true if the
 * chord was recognized (press fired); false otherwise.  Callers always
 * clear the prefix flag either way -- unknown chords are dropped, not
 * passed through to serial.
 */
static bool dispatch_panel_chord(uint8_t chord, AltaidHW *hw,
				 uint64_t now_tick, uint64_t hold_cycles)
{
	if (!hw) return false;

	if (chord >= (uint8_t)'1' && chord <= (uint8_t)'8') {
		altaid_hw_panel_press_key(hw, (uint8_t)(chord - (uint8_t)'1'),
					  now_tick, hold_cycles);
		return true;
	}
	if (chord == (uint8_t)'r') {
		altaid_hw_panel_press_key(hw, 8, now_tick, hold_cycles);
		return true;
	}
	if (chord == (uint8_t)'m') {
		altaid_hw_panel_press_key(hw, 9, now_tick, hold_cycles);
		return true;
	}
	if (chord == (uint8_t)'n') {
		altaid_hw_panel_press_key(hw, 10, now_tick, hold_cycles);
		return true;
	}
	if (chord == (uint8_t)'N') {
		/* TUI alias: D7 + NEXT chord ("back" in some monitors). */
		altaid_hw_panel_press_key(hw, 7, now_tick, hold_cycles);
		altaid_hw_panel_press_key(hw, 10, now_tick, hold_cycles);
		return true;
	}
	return false;
}

void serial_routing_stdin_dispatch(const uint8_t *buf, size_t n,
				   SerialDev *ser, AltaidHW *hw,
				   uint64_t now_tick, uint64_t hold_cycles,
				   struct StdinPanelState *state)
{
	size_t i;

	if (!buf || !state) return;

	for (i = 0; i < n; i++) {
		uint8_t ch = buf[i];

		if (state->prefix_active) {
			state->prefix_active = false;
			(void)dispatch_panel_chord(ch, hw, now_tick,
						   hold_cycles);
			continue;
		}

		if (ch == PANEL_PREFIX_BYTE) {
			state->prefix_active = true;
			continue;
		}

		if (ser) {
			if (ch == (uint8_t)'\n')
				ch = (uint8_t)'\r';
			serial_host_enqueue(ser, ch);
		}
	}
}

void serial_routing_stdin_poll_with_panel(SerialDev *ser, AltaidHW *hw,
					  uint64_t now_tick,
					  uint64_t hold_cycles,
					  struct StdinPanelState *state)
{
	uint8_t buf[256];

	if (!state) return;

	for (;;) {
		ssize_t n = ALTAID_IO_READ(STDIN_FILENO, buf, sizeof(buf));
		if (n > 0) {
			serial_routing_stdin_dispatch(buf, (size_t)n, ser, hw,
						      now_tick, hold_cycles,
						      state);
			continue;
		}
		if (n == 0) return;
		if (errno == EAGAIN || errno == EWOULDBLOCK) return;
		return;
	}
}
