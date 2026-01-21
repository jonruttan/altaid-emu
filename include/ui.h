/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_UI_H
#define ALTAID_EMU_UI_H

#include "altaid_hw.h"
#include "serial.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
	enum {
		UI_PROMPT_NONE = 0,
		UI_PROMPT_STATE_FILE,
		UI_PROMPT_RAM_FILE,
		UI_PROMPT_CASS_FILE,
	} prompt_kind;

	char	state_path[512];
	char	ram_path[512];
	char	cass_path[512];

	bool	prompt_active;
	char	prompt_buf[512];
	unsigned prompt_len;

	bool	req_state_save;
	bool	req_state_load;
	bool	req_ram_save;
	bool	req_ram_load;

	bool	req_cass_attach;
	bool	req_cass_save;
	bool	req_cass_play;
	bool	req_cass_rec;
	bool	req_cass_stop;
	bool	req_cass_rewind;
	bool	req_cass_ff;

	bool	panel_prefix;	/* saw Ctrl-P */
	bool	show_panel;	/* toggle live panel rendering */
	bool	panel_compact;	/* text-mode panel compact */
	bool	ui_mode;	/* full-screen UI mode */
	bool	serial_ro;	/* serial input disabled (read-only) */
	bool	help_requested;	/* request help text be displayed */
	bool	help_direct;	/* direct-panel variant */
	bool	reset;		/* request machine reset */
	bool	quit;		/* request emulator shutdown */
	bool	pty_mode;	/* --pty active */
	bool	pty_input;	/* reserved; PTY mode forces serial read-only */
	bool	event;		/* UI performed an action */
} UI;

const char *ui_help_string(bool direct);

void ui_init(UI *ui);
void ui_shutdown(UI *ui);

void ui_set_output(FILE *out);

void ui_poll(UI *ui, SerialDev *s, AltaidHW *hw, uint64_t now_tick,
uint64_t key_hold_cycles);

#endif /* ALTAID_EMU_UI_H */
