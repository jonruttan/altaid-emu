/* SPDX-License-Identifier: MIT */

#include "cli.h"
#include "version.h"
#include "emu.h"
#include "log.h"
#include "stateio.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static volatile sig_atomic_t g_stop;
static volatile sig_atomic_t g_winch;

static void on_signal(int sig)
{
	if (sig == SIGWINCH) {
		g_winch = 1;
		return;
	}
	g_stop = 1;
}

int main(int argc, char **argv)
{
	struct Config cfg;
	struct Emu emu;
	int rc;

	/* Keep terminal output immediate (UI correctness + logs). */
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	rc = cli_parse_args(argc, argv, &cfg);
	if (rc == -2) {
		cli_usage(argv[0]);
		return 2;
	}
	if (cfg.show_version) {
		fprintf(stdout, "altaid-emu %s\n", altaid_emu_version());
		return 0;
	}
	if (cfg.show_help || !cfg.rom_path) {
		cli_usage(argv[0]);
		return cfg.rom_path ? 0 : 2;
	}

	if ((cfg.cassette_play || cfg.cassette_rec) && !cfg.cassette_path) {
		fprintf(stderr, "--cass-play/--cass-rec requires --cass <file>\n");
		cli_usage(argv[0]);
		return 2;
	}

	if (cfg.headless) {
		cfg.start_panel = false;
		cfg.start_ui = false;
	}

	if (log_open(cfg.log_path, cfg.quiet, cfg.log_flush) < 0) return 1;

	/* Best-effort signal handling so we can unwind and restore the terminal. */
	signal(SIGTERM, on_signal);
	signal(SIGINT, on_signal);
	signal(SIGQUIT, on_signal);
	signal(SIGWINCH, on_signal);

	rc = emu_init(&emu, &cfg);
	if (rc < 0) {
		log_close();
		return 1;
	}

	rc = emu_run(&emu, &g_stop, &g_winch);

	/* Optional persist-on-exit. */
	if (cfg.state_save_path) {
		char err[256];
		if (!stateio_save_state(&emu.core, cfg.state_save_path,
			err, sizeof(err)))
			log_printf("state-save failed: %s\n", err);
	}
	if (cfg.ram_save_path) {
		char err[256];
		if (!stateio_save_ram(&emu.core, cfg.ram_save_path,
			err, sizeof(err)))
			log_printf("ram-save failed: %s\n", err);
	}
	emu_shutdown(&emu);
	log_close();
	return rc;
}
