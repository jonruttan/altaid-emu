/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_APP_CONFIG_H
#define ALTAID_APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Application run configuration.
 *
 * This is not CLI-specific: it is the validated configuration consumed by the
 * emulator and host integration layers.
 */

enum panel_text_mode {
	PANEL_TEXT_MODE_BURST = 0,
	PANEL_TEXT_MODE_CHANGE,
};

/*
 * Persistence specifications.
 *
 * An IoSpec describes one of:
 *   - "ram:<file>"                 full 512 KiB RAM
 *   - "ram@<addr>:<file>"          raw blob, bank 0, at addr
 *   - "ram@<bank>.<addr>:<file>"   raw blob, specific bank, at addr
 *   - "state:<file>"               CPU + devices + RAM snapshot
 */
enum io_spec_kind {
	IO_SPEC_RAM = 0,
	IO_SPEC_STATE,
};

struct IoSpec {
	enum io_spec_kind	kind;
	unsigned		bank;		/* ram only */
	uint16_t		addr;		/* ram only */
	bool			has_addr;	/* false = full ram */
	const char		*path;
};

#define APP_IO_SPEC_MAX 16

struct AppConfig {
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
	const char	*serial_out_spec;	/* stdout|stderr|-|none|<file> */
	const char	*serial_fd_spec;	/* stdout|stderr */
	bool		serial_append;

	/* Cassette. */
	const char	*cassette_path;
	bool		cassette_play;
	bool		cassette_rec;

	/* Persistence: parsed --load/--save/--default specs. */
	struct IoSpec	load_specs[APP_IO_SPEC_MAX];
	unsigned	load_count;
	struct IoSpec	save_specs[APP_IO_SPEC_MAX];
	unsigned	save_count;
	struct IoSpec	default_specs[APP_IO_SPEC_MAX];
	unsigned	default_count;

	/* Other. */
	const char	*log_path;
	bool		log_flush;
	bool		quiet;
	bool		headless;
	bool		realtime;
	bool		show_help;
	bool		show_version;
};

#endif /* ALTAID_APP_CONFIG_H */
