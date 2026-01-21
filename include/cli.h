/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_CLI_H
#define ALTAID_CLI_H

#include <stdbool.h>
#include <stdint.h>

/*
 * CLI / configuration.
 *
 * Keep argument parsing and user-facing option defaults out of the emulator
 * core. The core consumes a validated Config.
 */

enum panel_text_mode {
	PANEL_TEXT_MODE_BURST = 0,
	PANEL_TEXT_MODE_CHANGE,
};

struct Config {
	/* Required positional ROM path (64 KiB). */
	const char	*rom_path;

	/* Core. */
	uint32_t	cpu_hz;
	uint32_t	baud;

	/* Panel/UI. */
	bool		start_panel;
	bool		start_ui;
	bool		use_ascii;
	bool		no_altscreen;
	uint32_t	panel_hz;
	bool		panel_hz_set;
	enum panel_text_mode	panel_text_mode;
	bool		panel_echo_chars;
	bool		panel_compact;	/* text panel: compact one-line */
	uint32_t	hold_ms;
	int		term_rows;
	int		term_cols;
	bool		term_override;

	/* I/O. */
	bool		use_pty;
	bool		pty_input;	/* allow local keyboard input in --pty mode */
	const char	*serial_out_spec; /* stdout|stderr|-|none|<file> */
	const char	*serial_fd_spec;  /* stdout|stderr */
	bool		serial_append;

	/* Cassette. */
	const char	*cassette_path;
	bool		cassette_play;
	bool		cassette_rec;

	/* Persistence (state/RAM). */
	const char	*state_file;      /* default path used by Ctrl-P commands */
	const char	*ram_file;        /* default path used by Ctrl-P commands */
	const char	*state_load_path; /* load full state at startup */
	const char	*state_save_path; /* save full state on exit */
	const char	*ram_load_path;   /* load RAM at startup */
	const char	*ram_save_path;   /* save RAM on exit */

	/* Other. */
	const char	*log_path;
	bool		log_flush;
	bool		quiet;
	bool		headless;
	bool		realtime;
	bool		show_help;
	bool		show_version;
};

void cli_usage(const char *argv0);
int cli_parse_args(int argc, char **argv, struct Config *cfg);

#endif /* ALTAID_CLI_H */
