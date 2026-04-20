/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_SERIAL_ROUTING_H
#define ALTAID_SERIAL_ROUTING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "altaid_hw.h"
#include "serial.h"

/*
 * Stateful prefix machine for Ctrl-P panel-input parsing on headless
 * stdin.  Owned by the caller (EmuHost) so a Ctrl-P byte that arrives
 * in one read still pairs with its chord byte in the next.
 *
 * Recognized input grammar:
 *
 *   <ctrl-p> <key>    single press of one key
 *
 * <key> is one of: '1'..'8' (D0..D7), 'r' (RUN), 'm' (MODE), 'n' (NEXT).
 * Unknown chord bytes are dropped.
 *
 * Multi-key chords are expressed by rapid-fire single presses: bytes
 * arriving in the same stdin poll are all dispatched at the same
 * emulated tick with the same hold duration, so the CPU's scan of
 * the switch matrix sees every key pressed simultaneously until any
 * of the holds expire.  No special chord syntax is needed.
 */
struct StdinPanelState {
	bool prefix_active;
};

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

/*
 * Parse `n` bytes from `buf` as either UART RX input or Ctrl-P panel
 * commands.  Mirrors the TUI's panel-prefix behavior: a Ctrl-P (0x10)
 * byte is consumed and the following byte is interpreted as a chord:
 *   '1'..'8' -> D0..D7
 *   'r'/'m'/'n' -> RUN/MODE/NEXT
 *   'N' -> D7 + NEXT chord
 * Recognized chords fire altaid_hw_panel_press_key().  Unrecognized
 * chords are dropped (not passed to serial).  All other bytes flow to
 * the serial RX queue with the usual '\n' -> '\r' translation.
 *
 * The prefix state carries across calls via *state so a Ctrl-P at the
 * end of one buffer still pairs with the chord byte at the start of
 * the next.
 */
void serial_routing_stdin_dispatch(const uint8_t *buf, size_t n,
				   SerialDev *ser, AltaidHW *hw,
				   uint64_t now_tick, uint64_t hold_cycles,
				   struct StdinPanelState *state);

/*
 * Drain bytes available on STDIN (non-blocking) and split them between
 * the UART RX queue and the panel-press API via
 * serial_routing_stdin_dispatch().
 */
void serial_routing_stdin_poll_with_panel(SerialDev *ser, AltaidHW *hw,
					  uint64_t now_tick,
					  uint64_t hold_cycles,
					  struct StdinPanelState *state);

#endif /* ALTAID_SERIAL_ROUTING_H */
