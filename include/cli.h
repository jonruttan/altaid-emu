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

/*
 * Persistence specifications.  An IoSpec is one of:
 *   ram:<file>                 full 512 KiB RAM
 *   ram@<addr>:<file>          raw blob, bank 0, at addr
 *   ram@<bank>.<addr>:<file>   raw blob, specific bank, at addr
 *   state:<file>               CPU + devices + RAM snapshot
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

#define CLI_IO_SPEC_MAX 16

/*
 * Scripted front-panel input events.  One of:
 *   PANEL_EVENT_PRESS   momentary press of any key (D0..D7, RUN, MODE, NEXT)
 *   PANEL_EVENT_SWITCH  latched set/clear of a data switch (D0..D7 only)
 *
 * key_index:
 *   0..7  -> D0..D7
 *   8     -> RUN
 *   9     -> MODE
 *   10    -> NEXT
 *
 * Switch events use key_index 0..7 only; `value` holds on/off.
 * Press events use `hold_ms` for auto-release.
 */
enum panel_event_kind {
	PANEL_EVENT_PRESS = 0,
	PANEL_EVENT_SWITCH,
};

struct PanelEvent {
	enum panel_event_kind	kind;
	uint8_t		key_index;
	bool		value;		/* switch only */
	uint32_t	at_ms;		/* emulated-time deadline */
	uint32_t	hold_ms;	/* press only */
};

#define CLI_PANEL_EVENT_MAX 32

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
	const char	*serial_in_spec;  /* stdin|-|none; NULL = default by mode */
	const char	*serial_out_spec; /* stdout|stderr|-|none|<file> */
	const char	*serial_fd_spec;  /* stdout|stderr */
	bool		serial_append;

	/* Cassette. */
	const char	*cassette_path;
	bool		cassette_play;
	bool		cassette_rec;

	/* Persistence: parsed --load/--save/--default specs. */
	struct IoSpec	load_specs[CLI_IO_SPEC_MAX];
	unsigned	load_count;
	struct IoSpec	save_specs[CLI_IO_SPEC_MAX];
	unsigned	save_count;
	struct IoSpec	default_specs[CLI_IO_SPEC_MAX];
	unsigned	default_count;

	/* Scripted front-panel input: --press / --switch. */
	struct PanelEvent	panel_events[CLI_PANEL_EVENT_MAX];
	unsigned		panel_event_count;

	/* Other. */
	const char	*log_path;
	bool		log_flush;
	bool		quiet;
	bool		headless;
	bool		realtime;
	bool		show_help;
	bool		show_version;
	bool		debug_panel;	/* trace panel key press/release/scan events */

	/*
	 * Deterministic exit for testing: stop the runloop once the emulator
	 * has run this many milliseconds of CPU time. 0 means "run forever".
	 */
	uint32_t	max_run_ms;
};

void cli_usage(const char *argv0);
int cli_parse_args(int argc, char **argv, struct Config *cfg);

#endif /* ALTAID_CLI_H */
