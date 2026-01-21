/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_PANEL_TEXT_H
#define ALTAID_EMU_PANEL_TEXT_H

#include "altaid_hw.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Non-ANSI front panel renderer (no escape sequences). */

enum panel_text_emit_mode {
	PANEL_TEXT_EMIT_BURST = 0,
	PANEL_TEXT_EMIT_CHANGE,
};

void panel_text_begin(void);
void panel_text_end(void);

void panel_text_set_output(FILE *out);
void panel_text_set_compact(bool enable);
void panel_text_set_emit_mode(enum panel_text_emit_mode mode);

void panel_text_render(const AltaidHW *hw, const char *pty_name, bool pty_mode,
		bool pty_input, uint64_t tick, uint32_t cpu_hz,
		uint32_t baud);

#endif /* ALTAID_EMU_PANEL_TEXT_H */
