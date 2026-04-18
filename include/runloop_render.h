/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_RUNLOOP_RENDER_H
#define ALTAID_RUNLOOP_RENDER_H

/*
 * Panel + TX rendering helpers used by the runloop.
 *
 * These are the "what goes on the user's screen" bits: draining decoded
 * serial TX to the right fds, orchestrating panel renderer lifecycle,
 * computing panel-refresh cadence, and emitting text-mode snapshots.
 *
 * Shared file-static state lives inside runloop_render.c (not exposed).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "emu_core.h"
#include "emu_host.h"

/*
 * Destination for decoded TX bytes in PTY mode.
 *  - fd:        master PTY fd (-1 disables)
 *  - mirror_fd: optional terminal mirror (-1 disables)
 */
typedef struct {
	int	fd;
	int	mirror_fd;
} PtyOut;

/*
 * Destination for decoded TX bytes in non-PTY mode.
 *  - fd: serial-out fd (-1 disables)
 */
typedef struct {
	int	fd;
} FdOut;

/*
 * Drain any decoded TX bytes sitting in EmuCore's tx buffer to the right
 * host destinations (PTY, serial-out file, TUI serial view). Returns the
 * number of bytes drained; *had_nl is set true if a newline byte was seen.
 */
size_t runloop_tx_drain(struct EmuCore *core, const struct EmuHost *host,
			const PtyOut *pty_out, const FdOut *serial_out,
			bool tui_active, FILE *ui_out, bool *had_nl);

/*
 * Emit a text-mode panel snapshot to ui_out, inserting a leading newline
 * if the tty cursor isn't at beginning-of-line.
 */
void runloop_text_snapshot_emit(const struct EmuHost *host,
				const struct EmuCore *core, FILE *ui_out);

/*
 * Render the front panel via the active renderer (ANSI or text).
 */
void runloop_panel_render(const struct EmuHost *host,
			  const struct EmuCore *core, bool tui_active);

/*
 * Recompute panel refresh policy from config + UI state. Sets
 * *effective_panel_hz, *panel_period (caller resets if hz changes),
 * *panel_refresh, and *text_snapshot_mode.
 */
void runloop_compute_panel_policy(const struct EmuHost *host, bool tui_active,
	uint32_t *effective_panel_hz, uint64_t *panel_period,
	bool *panel_refresh, bool *text_snapshot_mode);

/*
 * Resolve the serial-output fd based on current UI stream, TUI state,
 * panel visibility, and user overrides.
 */
void runloop_compute_serial_routing(const struct EmuHost *host,
	const FILE *ui_out, bool tui_active, int *out_fd);

/*
 * Start/stop the panel renderer(s) to match the desired state derived
 * from tui_active and host->ui.show_panel. Updates host->panel_active,
 * host->ui_active, and *ansi_live accordingly.
 */
void runloop_manage_panel_lifecycle(struct EmuHost *host, bool tui_active,
				    bool *ansi_live);

#endif /* ALTAID_RUNLOOP_RENDER_H */
