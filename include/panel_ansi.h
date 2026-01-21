/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_PANEL_ANSI_H
#define ALTAID_EMU_PANEL_ANSI_H

#include "altaid_hw.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ANSI front panel renderer. */

void panel_ansi_set_output(FILE *out);

bool panel_ansi_is_tty(void);
void panel_ansi_begin(void);
void panel_ansi_end(void);

void panel_ansi_set_refresh(bool enable);
void panel_ansi_set_altscreen(bool enable);
void panel_ansi_set_ascii(bool enable);
void panel_ansi_set_split(bool enable);
void panel_ansi_set_panel_visible(bool enable);
void panel_ansi_set_serial_ro(bool enable);
void panel_ansi_set_statusline(bool enable);
void panel_ansi_set_status_override(const char *s);
void panel_ansi_clear_status_override(void);
void panel_ansi_handle_resize(void);
void panel_ansi_set_term_size_override(bool enable, int rows, int cols);
void panel_ansi_goto_serial(void);

/*
 * TUI serial view support.
 *
 * In split-screen TUI mode, feed raw serial bytes into the ANSI renderer.
 * The renderer maintains a line buffer and redraws the serial area
 * deterministically during panel refresh.
 */
void panel_ansi_serial_reset(void);
void panel_ansi_serial_feed(const uint8_t *buf, size_t len);

void panel_ansi_render(const AltaidHW *hw, const char *pty_name,
bool pty_mode, bool pty_input, uint64_t tick,
uint32_t cpu_hz, uint32_t baud);

#endif /* ALTAID_EMU_PANEL_ANSI_H */
