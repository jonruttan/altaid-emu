/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_HOST_H
#define ALTAID_EMU_HOST_H

#include "cli.h"
#include "emu_core.h"
#include "ui.h"

#include <stdbool.h>
#include <signal.h>
#include <stdint.h>

/*
* EmuHost owns host integration:
* CLI config, PTY/stdio routing, UI lifecycle, wall-clock throttling.
*/
struct EmuHost {
	struct Config	cfg;

	UI		ui;
	bool		panel_active;
	bool		ui_active;
	bool		ui_inited;

	int		pty_fd;
	int		pty_slave_fd;
	char		pty_name[128];

	int		serial_file_fd;
	int		serial_mirror_file_fd;
	int		serial_out_fd_spec;
	int		serial_mirror_fd_spec;
	int		serial_fd_override;

	uint64_t	next_panel_tick;

	uint32_t	wall_start_usec;
	uint64_t	emu_start_tick;
};

/*
* File-descriptor "spec" sentinel values used by EmuHost.
*
* Keep these negative so they don't collide with valid file descriptors.
*/
#define EMU_FD_UNSPEC		(-2)
#define EMU_FD_NONE		(-1)
#define EMU_FD_BADSPEC		(-3)
#define EMU_FD_OPENFAIL		(-4)

int emu_host_init(struct EmuHost *host, struct EmuCore *core,
const struct Config *cfg);
void emu_host_shutdown(struct EmuHost *host, struct EmuCore *core);

void emu_host_epoch_reset(struct EmuHost *host, const struct EmuCore *core);

int emu_host_runloop(struct EmuHost *host, struct EmuCore *core,
volatile sig_atomic_t *stop_flag,
volatile sig_atomic_t *winch_flag);

#endif /* ALTAID_EMU_HOST_H */
