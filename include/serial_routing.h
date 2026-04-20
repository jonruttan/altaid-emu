/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_SERIAL_ROUTING_H
#define ALTAID_SERIAL_ROUTING_H

#include <stdbool.h>

#include "serial.h"

/*
 * Compute the serial output fd selection.
 *
 * ui_fd: fileno(ui_out) or -1
 * tui_active: true when TUI is active
 * panel_visible: true when panel snapshots are enabled
 * serial_spec_fd: resolved serial-out spec (EMU_FD_UNSPEC if none)
 * serial_override_fd: resolved serial fd override (EMU_FD_UNSPEC if none)
 * ui_stdout_same_tty: true if ui_fd and stdout share the same tty
 */
int serial_routing_fd(int ui_fd, bool tui_active, bool panel_visible,
	int serial_spec_fd, int serial_override_fd, bool ui_stdout_same_tty);

/*
 * Drain bytes available on pty_fd (non-blocking) into the emulated serial
 * RX queue. No-op when pty_fd < 0 or ser is NULL.
 */
void serial_routing_pty_poll(int pty_fd, SerialDev *ser);

/*
 * Drain bytes available on STDIN (non-blocking) into the emulated serial
 * RX queue. '\n' is translated to '\r' to match terminal conventions.
 */
void serial_routing_stdin_poll(SerialDev *ser);

#endif /* ALTAID_SERIAL_ROUTING_H */
