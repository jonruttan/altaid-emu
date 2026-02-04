/* SPDX-License-Identifier: MIT */

#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE 1
#endif

#include "emu_host.h"

#include "hostpty.h"
#include "log.h"
#include "panel_ansi.h"
#include "panel_text.h"
#include "stateio.h"
#include "timeutil.h"

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int parse_serial_fd(const char *s)
{
	if (!s) return EMU_FD_UNSPEC;
	if (strcmp(s, "stdout") == 0) return STDOUT_FILENO;
	if (strcmp(s, "stderr") == 0) return STDERR_FILENO;
	return EMU_FD_BADSPEC;
}

static int open_serial_out_file(const char *path, int append)
{
	int flags;

	if (!path || !*path) return -1;
	flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
	return open(path, flags, 0644); /* rw-r--r-- */
}

static int resolve_serial_dest(const char *spec, int append, int *out_fd_to_close)
{
	int fd;

	if (out_fd_to_close)
	*out_fd_to_close = -1;
	if (!spec) return EMU_FD_UNSPEC;
	if (strcmp(spec, "-") == 0 || strcmp(spec, "stdout") == 0) return STDOUT_FILENO;
	if (strcmp(spec, "stderr") == 0) return STDERR_FILENO;
	if (strcmp(spec, "none") == 0) return EMU_FD_NONE;

	fd = open_serial_out_file(spec, append);
	if (fd < 0) return EMU_FD_OPENFAIL;
	if (out_fd_to_close)
	*out_fd_to_close = fd;
	return fd;
}

int emu_host_init(struct EmuHost *host, struct EmuCore *core,
const struct Config *cfg)
{
	int fl;

	if (!host || !core || !cfg) return -1;

	memset(host, 0, sizeof(*host));
	host->cfg = *cfg;
	host->pty_fd = -1;
	host->pty_slave_fd = -1;
	host->serial_file_fd = -1;
	host->serial_mirror_file_fd = -1;
	host->serial_out_fd_spec = EMU_FD_UNSPEC;
	host->serial_mirror_fd_spec = EMU_FD_UNSPEC;
	host->serial_fd_override = EMU_FD_UNSPEC;
	host->next_panel_tick = 0;

	(void)setlocale(LC_CTYPE, "");

	/* Cassette attachment is host-facing (file IO). */
	if (host->cfg.cassette_path) {
		if (!cassette_open(&core->cas, host->cfg.cassette_path)) {
			fprintf(stderr, "Failed to open cassette '%s': %s\n",
			host->cfg.cassette_path, strerror(errno));
			cassette_free(&core->cas);
			goto fail;
		}
		core->cas_attached = true;
		if (host->cfg.cassette_rec)
		cassette_start_record(&core->cas, 0);
		else if (host->cfg.cassette_play)
		cassette_start_play(&core->cas, 0);
	}

	/* Optional PTY. */
	if (host->cfg.use_pty) {
		host->pty_fd = hostpty_open(host->pty_name, sizeof(host->pty_name));
		if (host->pty_fd < 0) {
			fprintf(stderr, "Failed to open PTY (posix_openpt): %s\n",
			strerror(errno));
			goto fail;
		}
		if (host->pty_name[0]) {
			host->pty_slave_fd = open(host->pty_name, O_RDWR | O_NOCTTY);
			if (host->pty_slave_fd >= 0)
			hostpty_make_raw(host->pty_slave_fd);
			else
			log_printf("[PTY] Warning: could not open slave '%s': %s\n",
			host->pty_name, strerror(errno));
		}
		hostpty_make_raw(host->pty_fd);
		fprintf(stderr, "PTY: %s\n", host->pty_name);
	}

	/* UI. */
	host->ui.show_panel = host->cfg.start_panel;
	host->ui.panel_compact = host->cfg.panel_compact;
	host->ui.ui_mode = host->cfg.start_ui;
	host->ui.serial_ro = host->cfg.use_pty ? !host->cfg.pty_input : false;
	host->ui.pty_mode = host->cfg.use_pty;
	host->ui.pty_input = host->cfg.pty_input;
	if (host->cfg.state_file)
		strncpy(host->ui.state_path, host->cfg.state_file,
			sizeof(host->ui.state_path));
	else
		host->ui.state_path[0] = '\0';
	host->ui.state_path[sizeof(host->ui.state_path) - 1] = '\0';
	if (host->cfg.ram_file)
		strncpy(host->ui.ram_path, host->cfg.ram_file,
			sizeof(host->ui.ram_path));
	else
		host->ui.ram_path[0] = '\0';
	host->ui.ram_path[sizeof(host->ui.ram_path) - 1] = '\0';
	if (host->cfg.cassette_path)
		strncpy(host->ui.cass_path, host->cfg.cassette_path,
			sizeof(host->ui.cass_path));
	else
		host->ui.cass_path[0] = '\0';
	host->ui.cass_path[sizeof(host->ui.cass_path) - 1] = '\0';

	if (!host->cfg.headless) {
		ui_init(&host->ui);
		host->ui_inited = true;
	}

	if (host->cfg.headless && !host->cfg.use_pty) {
		fl = fcntl(STDIN_FILENO, F_GETFL, 0);
		if (fl >= 0)
		(void)fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);
	}

	/* Panel renderer configuration. */
	panel_ansi_set_ascii(host->cfg.use_ascii);
	panel_ansi_set_altscreen(!host->cfg.no_altscreen);
	panel_ansi_set_split(true);
	panel_ansi_set_term_size_override(host->cfg.term_override,
	host->cfg.term_rows,
	host->cfg.term_cols);

	/* Serial output spec parsing and file handling. */
	if (host->cfg.serial_fd_spec) {
		host->serial_fd_override = parse_serial_fd(host->cfg.serial_fd_spec);
		if (host->serial_fd_override == EMU_FD_BADSPEC) {
			fprintf(stderr,
			"Invalid --serial-fd '%s' (use stdout|stderr)\n",
			host->cfg.serial_fd_spec);
			goto fail;
		}
	}

	if (host->cfg.serial_out_spec) {
		if (host->cfg.use_pty) {
			host->serial_mirror_fd_spec = resolve_serial_dest(
			host->cfg.serial_out_spec,
			host->cfg.serial_append,
			&host->serial_mirror_file_fd);
			if (host->serial_mirror_fd_spec == -4) {
				fprintf(stderr,
				"Failed to open --serial-out '%s': %s\n",
				host->cfg.serial_out_spec, strerror(errno));
				goto fail;
			}
		} else {
			host->serial_out_fd_spec = resolve_serial_dest(
			host->cfg.serial_out_spec,
			host->cfg.serial_append,
			&host->serial_file_fd);
			if (host->serial_out_fd_spec == -4) {
				fprintf(stderr,
				"Failed to open --serial-out '%s': %s\n",
				host->cfg.serial_out_spec, strerror(errno));
				goto fail;
			}
		}
	}

	emu_host_epoch_reset(host, core);

	return 0;

	fail:
	emu_host_shutdown(host, core);
	return -1;
}

void emu_host_shutdown(struct EmuHost *host, struct EmuCore *core)
{
	if (!host || !core) return;

	if (host->panel_active) {
		if (host->ui_active)
		panel_ansi_end();
		else
		panel_text_end();
	}

	if (host->ui_inited) {
		ui_shutdown(&host->ui);
		host->ui_inited = false;
	}

	if (host->pty_slave_fd >= 0) {
		close(host->pty_slave_fd);
		host->pty_slave_fd = -1;
	}
	if (host->pty_fd >= 0) {
		close(host->pty_fd);
		host->pty_fd = -1;
	}

	if (host->serial_file_fd >= 0) {
		close(host->serial_file_fd);
		host->serial_file_fd = -1;
	}
	if (host->serial_mirror_file_fd >= 0) {
		close(host->serial_mirror_file_fd);
		host->serial_mirror_file_fd = -1;
	}

	if (core->cas_attached)
	cassette_stop(&core->cas);
	cassette_free(&core->cas);
}

void emu_host_epoch_reset(struct EmuHost *host, const struct EmuCore *core)
{
	if (!host || !core) return;

	host->wall_start_usec = monotonic_usec();
	host->emu_start_tick = core->ser.tick;
	host->next_panel_tick = core->ser.tick;
}
