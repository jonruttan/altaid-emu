/* SPDX-License-Identifier: MIT */

/* For fileno() in strict C99 builds. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#include "emu_host.h"

#include "emu_core.h"
#include "cassette.h"
#include "io.h"
#include "io_sys.h"
#include "log.h"
#include "panel_ansi.h"
#include "panel_text.h"
#include "serial_routing.h"
#include "stateio.h"
#include "timeutil.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
	int fd;
	int mirror_fd; /* -1 disables */
} PtyOut;

typedef struct {
	int fd; /* -1 disables */
} FdOut;

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

static void panel_render(const struct EmuHost *host, const struct EmuCore *core,
			 bool ansi);

static size_t tx_drain(struct EmuCore *core, const struct EmuHost *host,
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

static void text_snapshot_emit(const struct EmuHost *host,
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
	panel_render(host, core, false);
	g_tty_at_bol = true;
}

static void pty_poll_input(int pty_fd, SerialDev *ser)
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

static void stdin_poll_input(SerialDev *ser)
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

static FILE *choose_ui_stream(const struct EmuHost *host, int term_fd_hint)
{
	if (host->ui.ui_mode) {
		if (term_fd_hint == STDOUT_FILENO && isatty(STDOUT_FILENO))
			return stdout;
		if (term_fd_hint == STDERR_FILENO && isatty(STDERR_FILENO))
			return stderr;
		/* Prefer any tty stream for UI output. */
		if (isatty(STDERR_FILENO))
			return stderr;
		if (isatty(STDOUT_FILENO))
			return stdout;
	}
	return stderr;
}


static int ui_term_fd_hint(const struct EmuHost *host, int serial_spec_fd,
int serial_mirror_spec_fd)
{
	if (host->cfg.use_pty) {
		if (serial_mirror_spec_fd == STDOUT_FILENO ||
		serial_mirror_spec_fd == STDERR_FILENO)
		return serial_mirror_spec_fd;
		return -1;
	}

	if (serial_spec_fd == STDOUT_FILENO || serial_spec_fd == STDERR_FILENO) return serial_spec_fd;
	if (host->serial_fd_override == STDOUT_FILENO ||
	host->serial_fd_override == STDERR_FILENO)
	return host->serial_fd_override;
	return -1;
}

static void apply_output_streams(FILE *ui_out)
{
	panel_ansi_set_output(ui_out);
	panel_text_set_output(ui_out);
	ui_set_output(ui_out);
}

static void compute_panel_policy(const struct EmuHost *host, bool tui_active,
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

static void compute_serial_routing(const struct EmuHost *host, const FILE *ui_out,
	bool tui_active, int *out_fd)
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

static void manage_panel_lifecycle(struct EmuHost *host, bool tui_active,
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

static void panel_render(const struct EmuHost *host, const struct EmuCore *core,
bool tui_active)
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

static void realtime_throttle(const struct EmuHost *host, const struct EmuCore *core)
{
	uint32_t now_wall;
	uint32_t emu_usec;
	uint32_t wall_elapsed;
	uint32_t delta;

	if (!host->cfg.realtime) return;

	now_wall = monotonic_usec();
	wall_elapsed = now_wall - host->wall_start_usec;
	emu_usec = emu_tick_to_usec(core->ser.tick - host->emu_start_tick, core->cfg.cpu_hz);
	if (emu_usec <= wall_elapsed) return;

	delta = emu_usec - wall_elapsed;
	sleep_or_wait_input_usec(delta, host->cfg.use_pty, host->pty_fd,
	host->cfg.headless);
}

int emu_host_runloop(struct EmuHost *host, struct EmuCore *core,
volatile sig_atomic_t *stop_flag,
volatile sig_atomic_t *winch_flag)
{
	PtyOut pty_out;
	FdOut serial_out;
	FILE *ui_out;
	bool tui_active;
	bool ansi_live;
	size_t tx_bytes;
	bool tx_had_nl;
	uint32_t effective_panel_hz;
	uint64_t panel_period;
	bool panel_refresh;
	bool text_snapshot_mode;
	bool text_snapshot_done;
	bool burst_pending;
	bool snapshot_pending;
	bool snapshot_seen;
	unsigned int snapshot_stable;
	uint16_t snapshot_last_addr;
	uint8_t snapshot_last_data;
	uint8_t snapshot_last_stat;
	uint64_t last_burst_tick;
	size_t burst_bytes;
	bool burst_had_nl;
	uint64_t burst_gap;
	uint64_t snapshot_settle;
	uint32_t snapshot_seq;
	uint64_t snapshot_tick;
	uint64_t batch_cycles;
	uint64_t key_hold_cycles;
	int term_fd_hint;
	int serial_fd;

	if (!host || !core) return 1;

	memset(&pty_out, 0, sizeof(pty_out));
	memset(&serial_out, 0, sizeof(serial_out));
	pty_out.fd = host->pty_fd;
	pty_out.mirror_fd = -1;
	serial_out.fd = -1;

	key_hold_cycles = (uint64_t)core->cfg.cpu_hz *
	(uint64_t)host->cfg.hold_ms / 1000ull;
	if (key_hold_cycles == 0)
	key_hold_cycles = 1;

	/*
	 * Text-mode "burst" snapshots should follow output closely.
	 * A long idle gap can miss transient panel states (e.g., ROM updates
	 * the front-panel display while printing a response, then restores
	 * it quickly after the prompt). Use a small gap (~5ms) to terminate
	 * bursts without spamming during normal output.
	 */
	burst_gap = core->cfg.cpu_hz / 200u; /* ~5ms */
	if (burst_gap == 0)
		burst_gap = 1;

	batch_cycles = core->cfg.cpu_hz / 2000u; /* ~0.5ms */
	if (batch_cycles < 32)
	batch_cycles = 32;

	effective_panel_hz = 0;
	panel_period = 0;
	panel_refresh = false;
	text_snapshot_mode = false;
	text_snapshot_done = false;
	burst_pending = false;
	snapshot_pending = false;
	snapshot_seen = false;
	snapshot_stable = 0;
	snapshot_last_addr = 0;
	snapshot_last_data = 0;
	snapshot_last_stat = 0;
	last_burst_tick = 0;
	burst_bytes = 0;
	burst_had_nl = false;
	/*
	 * After a burst ends, allow the ROM enough time to finish a full front-panel
	 * scan and settle to its post-command display state. We also require two
	 * consecutive identical latches, so this is a timeout, not a minimum delay.
	 */
	snapshot_settle = core->cfg.cpu_hz / 20u; /* ~50ms */
	if (snapshot_settle == 0)
		snapshot_settle = 1;
	snapshot_seq = 0;
	snapshot_tick = 0;
	ansi_live = false;

	for (;;) {
		if (stop_flag && *stop_flag) break;

		if (winch_flag && *winch_flag) {
			*winch_flag = 0;
			if (ansi_live)
			panel_ansi_handle_resize();
		}

		/* Host inputs. */
		if (host->cfg.use_pty)
		pty_poll_input(host->pty_fd, &core->ser);
		if (!host->cfg.headless)
		ui_poll(&host->ui, &core->ser, &core->hw, core->ser.tick,
		key_hold_cycles);
		else if (!host->cfg.use_pty)
		stdin_poll_input(&core->ser);

		if (host->ui.quit) break;

		if (host->ui.reset) {
			host->ui.reset = false;
			emu_core_reset(core);
			emu_host_epoch_reset(host, core);
			text_snapshot_done = false;
			burst_pending = false;
			burst_bytes = 0;
			burst_had_nl = false;
				snapshot_pending = false;
		}

			term_fd_hint = ui_term_fd_hint(host,
				host->serial_out_fd_spec,
				host->serial_mirror_fd_spec);
			ui_out = choose_ui_stream(host, term_fd_hint);
			apply_output_streams(ui_out);

			/*
			 * TUI requires an interactive tty. Prefer the chosen UI stream.
			 * (If both stdout/stderr are redirected, --ui is effectively off.)
			 */
			tui_active = host->ui.ui_mode && ui_out &&
				(isatty(fileno(ui_out)) != 0);

			/* Keep renderer state in sync before we (re)start the ANSI UI. */
			panel_ansi_set_panel_visible(host->ui.show_panel);
			panel_ansi_set_serial_ro(host->ui.serial_ro);
			panel_ansi_set_statusline(true);
			panel_ansi_set_split(true);

		compute_panel_policy(host, tui_active, &effective_panel_hz,
		&panel_period, &panel_refresh,
		&text_snapshot_mode);

		if (panel_refresh) {
			panel_period = core->cfg.cpu_hz /
			(uint64_t)(effective_panel_hz ? effective_panel_hz : 1);
			if (panel_period == 0)
			panel_period = 1;
		}

			manage_panel_lifecycle(host, tui_active, &ansi_live);

			if (host->ui.help_requested) {
				const char *help;

				help = ui_help_string(host->ui.help_direct);
				host->ui.help_requested = false;
				host->ui.help_direct = false;
				host->ui.event = true;

				if (tui_active) {
					panel_ansi_serial_feed((const uint8_t *)help,
										strlen(help));
				} else {
					FILE *out = ui_out ? ui_out : stderr;
					fputs(help, out);
					fflush(out);
				}
			}

			/*
			 * Prompt UI: in TUI mode the prompt is shown on the statusline.
			 * In non-TUI mode, ui.c prints an inline prompt to the panel stream.
			 */
			if (tui_active) {
				static bool prompt_was_active;

				if (host->ui.prompt_active) {
					char line[600];
					const char *what = "File";

					switch (host->ui.prompt_kind) {
					case UI_PROMPT_STATE_FILE:
						what = "State file";
						break;
					case UI_PROMPT_RAM_FILE:
						what = "RAM file";
						break;
					case UI_PROMPT_CASS_FILE:
						what = "Cassette file";
						break;
					default:
						break;
					}

					snprintf(line, sizeof(line), "%s: %s", what,
							host->ui.prompt_buf);
					panel_ansi_set_status_override(line);
					prompt_was_active = true;
				} else if (prompt_was_active) {
					panel_ansi_clear_status_override();
					prompt_was_active = false;
				}
			}

			/*
			 * Persistence + cassette operations (Ctrl-P commands).
			 * These are serviced synchronously between batches.
			 */
			if (host->ui.req_state_save) {
				char err[256];
				const char *p = host->ui.state_path[0] ?
					host->ui.state_path : "altaid.state";

				host->ui.req_state_save = false;
				if (!stateio_save_state(core, p, err, sizeof(err))) {
					if (tui_active) {
						char msg[600];
						snprintf(msg, sizeof(msg), "[STATE] save failed: %s\n", err);
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					} else {
						fprintf(ui_out ? ui_out : stderr,
							"[STATE] save failed: %s\n", err);
					}
				} else if (tui_active) {
					char msg[600];
					snprintf(msg, sizeof(msg), "[STATE] Saved: %s\n", p);
					panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
				} else {
					fprintf(ui_out ? ui_out : stderr, "[STATE] Saved: %s\n", p);
				}
				host->ui.event = true;
			}

			if (host->ui.req_state_load) {
				char err[256];
				const char *p = host->ui.state_path[0] ?
					host->ui.state_path : "altaid.state";

				host->ui.req_state_load = false;
				if (!stateio_load_state(core, p, err, sizeof(err))) {
					if (tui_active) {
						char msg[600];
						snprintf(msg, sizeof(msg), "[STATE] load failed: %s\n", err);
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					} else {
						fprintf(ui_out ? ui_out : stderr,
							"[STATE] load failed: %s\n", err);
					}
				} else {
					emu_host_epoch_reset(host, core);
					text_snapshot_done = false;
					burst_pending = false;
					burst_bytes = 0;
					burst_had_nl = false;
					snapshot_pending = false;
					if (tui_active) {
						char msg[600];
						snprintf(msg, sizeof(msg), "[STATE] Loaded: %s\n", p);
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					} else {
						fprintf(ui_out ? ui_out : stderr,
							"[STATE] Loaded: %s\n", p);
					}
				}
				host->ui.event = true;
			}

			if (host->ui.req_ram_save) {
				char err[256];
				const char *p = host->ui.ram_path[0] ?
					host->ui.ram_path : "altaid.ram";

				host->ui.req_ram_save = false;
				if (!stateio_save_ram(core, p, err, sizeof(err))) {
					if (tui_active) {
						char msg[600];
						snprintf(msg, sizeof(msg), "[RAM] save failed: %s\n", err);
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					} else {
						fprintf(ui_out ? ui_out : stderr,
							"[RAM] save failed: %s\n", err);
					}
				} else if (tui_active) {
					char msg[600];
					snprintf(msg, sizeof(msg), "[RAM] Saved: %s\n", p);
					panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
				} else {
					fprintf(ui_out ? ui_out : stderr, "[RAM] Saved: %s\n", p);
				}
				host->ui.event = true;
			}

			if (host->ui.req_ram_load) {
				char err[256];
				const char *p = host->ui.ram_path[0] ?
					host->ui.ram_path : "altaid.ram";

				host->ui.req_ram_load = false;
				if (!stateio_load_ram(core, p, err, sizeof(err))) {
					if (tui_active) {
						char msg[600];
						snprintf(msg, sizeof(msg), "[RAM] load failed: %s\n", err);
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					} else {
						fprintf(ui_out ? ui_out : stderr,
							"[RAM] load failed: %s\n", err);
					}
				} else if (tui_active) {
					char msg[600];
					snprintf(msg, sizeof(msg), "[RAM] Loaded: %s\n", p);
					panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
				} else {
					fprintf(ui_out ? ui_out : stderr, "[RAM] Loaded: %s\n", p);
				}
				host->ui.event = true;
			}

			if (host->ui.req_cass_attach) {
				const char *p = host->ui.cass_path;

				host->ui.req_cass_attach = false;
				if (!p || !p[0]) {
					const char *msg = "[CASS] attach failed: no path\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else if (!cassette_open(&core->cas, p)) {
					char msg[600];
					snprintf(msg, sizeof(msg), "[CASS] attach failed: %s\n", p);
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else {
					char msg[600];
					core->cas_attached = true;
					snprintf(msg, sizeof(msg), "[CASS] Attached: %s\n", p);
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				}
				host->ui.event = true;
			}

			if (host->ui.req_cass_play) {
				host->ui.req_cass_play = false;
				if (!core->cas_attached) {
					const char *msg = "[CASS] No tape attached\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else {
					const char *msg = "[CASS] Play\n";
					cassette_start_play(&core->cas, core->ser.tick);
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				}
				host->ui.event = true;
			}

			if (host->ui.req_cass_rec) {
				host->ui.req_cass_rec = false;
				if (!core->cas_attached) {
					const char *msg = "[CASS] No tape attached\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else {
					const char *msg = "[CASS] Record\n";
					cassette_start_record(&core->cas, core->ser.tick);
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				}
				host->ui.event = true;
			}

			if (host->ui.req_cass_stop) {
				const char *msg = "[CASS] Stop\n";
				host->ui.req_cass_stop = false;
				cassette_stop(&core->cas);
				if (tui_active)
					panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
				else
					fputs(msg, ui_out ? ui_out : stderr);
				host->ui.event = true;
			}

			if (host->ui.req_cass_rewind) {
				host->ui.req_cass_rewind = false;
				if (!core->cas_attached) {
					const char *msg = "[CASS] No tape attached\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else {
					const char *msg = "[CASS] Rewind\n";
					cassette_rewind(&core->cas);
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				}
				host->ui.event = true;
			}

			if (host->ui.req_cass_ff) {
				host->ui.req_cass_ff = false;
				if (!core->cas_attached) {
					const char *msg = "[CASS] No tape attached\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else {
					const char *msg = "[CASS] Fast-forward 10s\n";
					cassette_ff(&core->cas, 10, core->ser.tick);
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				}
				host->ui.event = true;
			}

			if (host->ui.req_cass_save) {
				host->ui.req_cass_save = false;
				if (!core->cas_attached) {
					const char *msg = "[CASS] No tape attached\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else if (!cassette_save(&core->cas)) {
					const char *msg = "[CASS] save failed\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				} else {
					const char *msg = "[CASS] Saved\n";
					if (tui_active)
						panel_ansi_serial_feed((const uint8_t *)msg, strlen(msg));
					else
						fputs(msg, ui_out ? ui_out : stderr);
				}
				host->ui.event = true;
			}

		/* Serial routing (non-PTY) or mirror fd (PTY). */
		if (host->cfg.use_pty) {
			if (host->serial_mirror_fd_spec != -2)
			pty_out.mirror_fd = host->serial_mirror_fd_spec;
				else
					pty_out.mirror_fd = -1;
		} else {
			compute_serial_routing(host, ui_out, tui_active, &serial_fd);
			serial_out.fd = serial_fd;
		}

		/* Run core for one batch. */
		emu_core_run_batch(core, batch_cycles);

			/* Panel rendering. */
			if (ansi_live) {

			if (host->ui.event) {
				host->ui.event = false;
				host->next_panel_tick = 0;
			}

			/* Refresh cadence drives both panel + statusline. */
			if (panel_refresh && core->ser.tick >= host->next_panel_tick) {
				host->next_panel_tick = core->ser.tick + panel_period;
				panel_render(host, core, true);
			}
		} else if (host->ui.show_panel) {
			if (panel_refresh) {
				if (core->ser.tick >= host->next_panel_tick) {
					host->next_panel_tick = core->ser.tick + panel_period;
					panel_render(host, core, false);
				}
			} else if (host->cfg.panel_text_mode == PANEL_TEXT_MODE_CHANGE) {
				panel_render(host, core, false);
			} else if (text_snapshot_mode) {
				if (!text_snapshot_done) {
					panel_render(host, core, false);
					text_snapshot_done = true;
				}
			}
		}

			/* Drain decoded TX bytes to host outputs. */
			tx_bytes = tx_drain(core, host, &pty_out, &serial_out, ansi_live,
					   ui_out, &tx_had_nl);
			if (text_snapshot_mode) {
				if (tx_bytes) {
					snapshot_pending = false;
					burst_pending = true;
					last_burst_tick = core->ser.tick;
					burst_bytes += tx_bytes;
					if (tx_had_nl)
						burst_had_nl = true;
				} else if (burst_pending &&
					   (core->ser.tick - last_burst_tick) > burst_gap) {
					bool emit;

					emit = host->cfg.panel_echo_chars;
					if (!emit) {
						if (burst_had_nl || burst_bytes >= 8)
							emit = true;
					}
					if (emit) {
						snapshot_pending = true;
						/*
						 * The front panel is multiplexed; latches can briefly show
						 * mixed nibbles during transitions. After a serial burst ends,
						 * wait for a stable latched value (two consecutive identical
						 * latches) before printing a snapshot.
						 */
						snapshot_seq = core->hw.panel_latched_seq;
						snapshot_tick = core->ser.tick;
						snapshot_seen = false;
						snapshot_stable = 0;
					}

					burst_pending = false;
					burst_bytes = 0;
					burst_had_nl = false;
				}

					if (snapshot_pending) {
						/*
						 * Wait for at least one new latched panel update after the
						 * burst, then require two consecutive identical latches. This
						 * filters transient mixed-nibble values during LED multiplexing.
						 */
						if (core->hw.panel_latched_seq != snapshot_seq) {
							uint16_t a;
							uint8_t d;
							uint8_t s;

							snapshot_seq = core->hw.panel_latched_seq;
							a = core->hw.panel_latched_addr;
							d = core->hw.panel_latched_data;
							s = core->hw.panel_latched_stat;

							if (!snapshot_seen) {
								snapshot_last_addr = a;
								snapshot_last_data = d;
								snapshot_last_stat = s;
								snapshot_seen = true;
								snapshot_stable = 1;
							} else if (a == snapshot_last_addr &&
								   d == snapshot_last_data &&
								   s == snapshot_last_stat) {
								snapshot_stable++;
							} else {
								snapshot_last_addr = a;
								snapshot_last_data = d;
								snapshot_last_stat = s;
								snapshot_stable = 1;
							}
						}

						if ((snapshot_seen && snapshot_stable >= 2) ||
						    (core->ser.tick - snapshot_tick) > snapshot_settle) {
							text_snapshot_emit(host, core,
								  ui_out ? ui_out : stderr);
							snapshot_pending = false;
						}
					}
			}


		realtime_throttle(host, core);
	}

	return 0;
}
