/* SPDX-License-Identifier: MIT */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#include "runloop_render.h"

#include "cassette.h"
#include "io.h"
#include "panel_ansi.h"
#include "panel_text.h"
#include "serial_routing.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Track whether the shared controlling TTY cursor is currently at BOL.
 * STDOUT/STDERR share terminal state, so a panel snapshot must force a newline
 * when the last serial output ended without one.
 */
static bool g_tty_at_bol = true;

static void tty_bol_update_fd(int fd, const uint8_t *buf, size_t n)
{
	if (n == 0)
		return;
	if (fd != STDOUT_FILENO && fd != STDERR_FILENO)
		return;
	if (isatty(fd) == 0)
		return;
	g_tty_at_bol = (buf[n - 1] == (uint8_t)'\n');
}

static bool same_tty(int a, int b)
{
	struct stat sa;
	struct stat sb;

	if (a < 0 || b < 0)
		return false;
	if (isatty(a) == 0 || isatty(b) == 0)
		return false;
	if (fstat(a, &sa) != 0)
		return false;
	if (fstat(b, &sb) != 0)
		return false;
	return (sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino);
}

size_t runloop_tx_drain(struct EmuCore *core, const struct EmuHost *host,
			const PtyOut *pty_out, const FdOut *serial_out,
			bool tui_active, FILE *ui_out, bool *had_nl)
{
	int ui_fd;
	uint8_t tmp[512];
	size_t n;
	bool need_goto;
	bool have_tui;
	size_t drained;

	if (!core || !host)
		return 0;

	ui_fd = -1;
	if (ui_out)
		ui_fd = fileno(ui_out);

	have_tui = tui_active && ui_out && ui_fd >= 0 && (isatty(ui_fd) != 0);

	drained = 0;
	if (had_nl)
		*had_nl = false;

	/*
	 * Drain TX once. If TUI is active, feed the serial buffer for deterministic
	 * redraw. Raw serial bytes are still written to non-UI destinations.
	 */
	need_goto = have_tui;
	for (;;) {
		n = emu_core_tx_pop(core, tmp, sizeof(tmp));
		if (!n)
			break;

		drained += n;
		if (had_nl) {
			if (memchr(tmp, '\n', n) || memchr(tmp, '\r', n))
				*had_nl = true;
		}

		if (have_tui)
			panel_ansi_serial_feed(tmp, n);

		if (host->cfg.use_pty) {
			if (pty_out && pty_out->fd >= 0)
				(void)write_full(pty_out->fd, tmp, n);
			if (pty_out && pty_out->mirror_fd >= 0) {
				if (need_goto) {
					panel_ansi_goto_serial();
					need_goto = false;
				}
				(void)write_full(pty_out->mirror_fd, tmp, n);
				tty_bol_update_fd(pty_out->mirror_fd, tmp, n);
			}
			continue;
		}

		if (serial_out && serial_out->fd >= 0) {
			/* Don't spew raw serial into the TUI's own output stream. */
			if (!have_tui || serial_out->fd != ui_fd) {
				if (need_goto) {
					panel_ansi_goto_serial();
					need_goto = false;
				}
				(void)write_full(serial_out->fd, tmp, n);
				tty_bol_update_fd(serial_out->fd, tmp, n);
			}
		}
	}
	return drained;
}

void runloop_text_snapshot_emit(const struct EmuHost *host,
				const struct EmuCore *core, FILE *ui_out)
{
	int ui_fd;

	if (!host || !core || !ui_out)
		return;
	if (!host->ui.show_panel)
		return;

	ui_fd = fileno(ui_out);
	if (ui_fd == STDOUT_FILENO || ui_fd == STDERR_FILENO) {
		if (isatty(ui_fd) != 0 && !g_tty_at_bol) {
			(void)write_full(ui_fd, "\n", 1);
			g_tty_at_bol = true;
		}
	}
	runloop_panel_render(host, core, false);
	g_tty_at_bol = true;
}

void runloop_panel_render(const struct EmuHost *host,
			  const struct EmuCore *core, bool tui_active)
{
	if (tui_active)
		panel_ansi_render(&core->hw, host->pty_name, host->cfg.use_pty,
			host->ui.pty_input, core->ser.tick, core->cfg.cpu_hz,
			core->cfg.baud);
	else
		panel_text_render(&core->hw, host->pty_name, host->cfg.use_pty,
			host->ui.pty_input, core->ser.tick, core->cfg.cpu_hz,
			core->cfg.baud);
}

void runloop_compute_panel_policy(const struct EmuHost *host, bool tui_active,
	uint32_t *effective_panel_hz, uint64_t *panel_period,
	bool *panel_refresh, bool *text_snapshot_mode)
{
	uint32_t new_hz;

	/*
	 * In TUI mode, we always refresh the ANSI renderer (statusline + split).
	 * Panel visibility only affects what gets drawn in the upper section.
	 */
	if (tui_active) {
		if (host->cfg.panel_hz_set)
			new_hz = host->cfg.panel_hz;
		else
			new_hz = host->ui.show_panel ? 15u : 10u;
		*panel_refresh = true;
		*text_snapshot_mode = false;
	} else {
		if (host->cfg.panel_hz_set)
			new_hz = host->cfg.panel_hz;
		else
			new_hz = 0;
		*panel_refresh = (new_hz > 0);
		*text_snapshot_mode = (host->ui.show_panel &&
			host->cfg.panel_text_mode == PANEL_TEXT_MODE_BURST &&
			!(*panel_refresh));
	}

	if (new_hz != *effective_panel_hz) {
		*effective_panel_hz = new_hz;
		*panel_period = 0;
	}

	if (tui_active)
		panel_ansi_set_refresh(true);
	else
		panel_ansi_set_refresh(*panel_refresh);
	panel_text_set_compact(host->ui.panel_compact);
}

void runloop_compute_serial_routing(const struct EmuHost *host,
	const FILE *ui_out, bool tui_active, int *out_fd)
{
	int ui_fd;
	int spec;
	int over;
	bool same_as_stdout;

	ui_fd = ui_out ? fileno((FILE *)ui_out) : -1;
	spec = host->serial_out_fd_spec;
	over = host->serial_fd_override;
	same_as_stdout = (ui_fd >= 0) && same_tty(ui_fd, STDOUT_FILENO);

	*out_fd = serial_routing_fd(ui_fd, tui_active, host->ui.show_panel,
		spec, over, same_as_stdout);
}

void runloop_manage_panel_lifecycle(struct EmuHost *host, bool tui_active,
				    bool *ansi_live)
{
	enum panel_renderer {
		PR_NONE,
		PR_ANSI,
		PR_TEXT,
	} want, have;

	if (tui_active)
		want = PR_ANSI;
	else if (host->ui.show_panel)
		want = PR_TEXT;
	else
		want = PR_NONE;

	if (!host->panel_active)
		have = PR_NONE;
	else
		have = host->ui_active ? PR_ANSI : PR_TEXT;

	if (want == have)
		return;

	/* Stop old renderer. */
	if (have == PR_ANSI)
		panel_ansi_end();
	else if (have == PR_TEXT)
		panel_text_end();

	host->panel_active = false;
	host->ui_active = false;
	*ansi_live = false;

	/* Start new renderer. */
	if (want == PR_ANSI) {
		panel_ansi_begin();
		host->panel_active = true;
		host->ui_active = true;
		*ansi_live = true;
	} else if (want == PR_TEXT) {
		panel_text_set_emit_mode(host->cfg.panel_text_mode == PANEL_TEXT_MODE_CHANGE ?
			PANEL_TEXT_EMIT_CHANGE : PANEL_TEXT_EMIT_BURST);
		panel_text_begin();
		host->panel_active = true;
		host->ui_active = false;
		*ansi_live = false;
	}
}
