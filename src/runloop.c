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
#include "log.h"
#include "panel_ansi.h"
#include "panel_text.h"
#include "runloop_render.h"
#include "runloop_time.h"
#include "serial_routing.h"
#include "stateio.h"

#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
	uint64_t run_deadline_tick;
	bool have_run_deadline;
	int term_fd_hint;
	int serial_fd;

	if (!host || !core) return 1;

	have_run_deadline = (host->cfg.max_run_ms > 0);
	run_deadline_tick = have_run_deadline
		? (uint64_t)core->cfg.cpu_hz *
			(uint64_t)host->cfg.max_run_ms / 1000ull
		: 0;

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

		if (have_run_deadline && core->ser.tick >= run_deadline_tick)
			break;

		if (winch_flag && *winch_flag) {
			*winch_flag = 0;
			if (ansi_live)
			panel_ansi_handle_resize();
		}

		/* Host inputs. */
		if (host->cfg.use_pty)
		serial_routing_pty_poll(host->pty_fd, &core->ser);
		if (!host->cfg.headless)
		ui_poll(&host->ui, &core->ser, &core->hw, core->ser.tick,
		key_hold_cycles);
		else if (!host->cfg.use_pty)
		serial_routing_stdin_poll(&core->ser);

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

		runloop_compute_panel_policy(host, tui_active, &effective_panel_hz,
		&panel_period, &panel_refresh,
		&text_snapshot_mode);

		if (panel_refresh) {
			panel_period = core->cfg.cpu_hz /
			(uint64_t)(effective_panel_hz ? effective_panel_hz : 1);
			if (panel_period == 0)
			panel_period = 1;
		}

			runloop_manage_panel_lifecycle(host, tui_active, &ansi_live);

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
				uint32_t ram_off = 0;

				/* Derive bank/addr from --default ram@... if present. */
				for (unsigned i = 0; i < host->cfg.default_count; i++) {
					const struct IoSpec *s = &host->cfg.default_specs[i];
					if (s->kind == IO_SPEC_RAM && s->has_addr)
						ram_off = (uint32_t)s->bank * 0x10000u + s->addr;
				}

				host->ui.req_ram_load = false;
				if (!stateio_load_ram(core, p, ram_off, err, sizeof(err))) {
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
			runloop_compute_serial_routing(host, ui_out, tui_active, &serial_fd);
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
				runloop_panel_render(host, core, true);
			}
		} else if (host->ui.show_panel) {
			if (panel_refresh) {
				if (core->ser.tick >= host->next_panel_tick) {
					host->next_panel_tick = core->ser.tick + panel_period;
					runloop_panel_render(host, core, false);
				}
			} else if (host->cfg.panel_text_mode == PANEL_TEXT_MODE_CHANGE) {
				runloop_panel_render(host, core, false);
			} else if (text_snapshot_mode) {
				if (!text_snapshot_done) {
					runloop_panel_render(host, core, false);
					text_snapshot_done = true;
				}
			}
		}

			/* Drain decoded TX bytes to host outputs. */
			tx_bytes = runloop_tx_drain(core, host, &pty_out, &serial_out, ansi_live,
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
							runloop_text_snapshot_emit(host, core,
								  ui_out ? ui_out : stderr);
							snapshot_pending = false;
						}
					}
			}


		runloop_realtime_throttle(host, core);
	}

	return 0;
}
