/* SPDX-License-Identifier: MIT */

#include "serial_routing.h"

#include "emu_host.h"

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
