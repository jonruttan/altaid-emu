/* SPDX-License-Identifier: MIT */

#include "ui.h"

#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#define KEY_CTRL(x) ((x) & 0x1f)

static struct termios g_old;
static FILE *g_out; /* defaults to stderr */

void ui_set_output(FILE *out)
{
	g_out = out ? out : stderr;
}

static FILE *out_stream(void)
{
	return g_out ? g_out : stderr;
}

static const char g_help_prefixed[] =
	"\n[Altaid UI] Ctrl-P then one key\n"
	"  1..8  press DATA key D0..D7 (momentary)\n"
	"  r     press RUN\n"
	"  m     press MODE\n"
	"  n     press NEXT\n"
	"  N     press NEXT+D7 chord (\"back\" in some monitors)\n"
	"  p     toggle panel visibility (upper section)\n"
	"  c     toggle text panel compact/verbose (non-UI)\n"
	"  u     toggle full-screen UI mode (--ui)\n"
	"  i     toggle local serial input read-only\n"
	"  t     toggle local keyboard input in --pty mode (--pty-input)\n"
	"\n"
	"State/RAM (persistence):\n"
	"  s     save machine state to current state file\n"
	"  l     load machine state from current state file\n"
	"  f     set state filename (prompts)\n"
	"  b     save RAM banks to current RAM file\n"
	"  g     load RAM banks from current RAM file\n"
	"  M     set RAM filename (prompts)\n"
	"\n"
	"Cassette transport:\n"
	"  a     set/attach cassette filename (prompts)\n"
	"  P     play\n"
	"  R     record\n"
	"  K     stop\n"
	"  W     rewind\n"
	"  J     fast-forward 10s\n"
	"  V     save tape image now\n"
	"  d     dump panel snapshot\n"
	"  Ctrl-P <key>  prefix form of the above\n"
	"  Ctrl-P Ctrl-P  alias for Ctrl-P i\n"
	"  Ctrl-P Ctrl-R  reset emulated machine\n"
	"  h/?   show this help\n"
	"  q     quit emulator\n\n";

static const char g_help_direct[] =
	"\n[Altaid UI] panel keys are direct (no prefix)\n"
	"  1..8  press DATA key D0..D7 (momentary)\n"
	"  r     press RUN\n"
	"  m     press MODE\n"
	"  n     press NEXT\n"
	"  N     press NEXT+D7 chord (\"back\" in some monitors)\n"
	"  p     toggle panel visibility (upper section)\n"
	"  c     toggle text panel compact/verbose (non-UI)\n"
	"  u     toggle full-screen UI mode (--ui)\n"
	"  i     toggle local serial input read-only\n"
	"  t     toggle local keyboard input in --pty mode (--pty-input)\n"
	"\n"
	"Additional commands are available via Ctrl-P prefix (persistence/tape, etc).\n"
	"  d     dump panel snapshot\n"
	"  Ctrl-P <key>  prefix form of the above\n"
	"  Ctrl-P Ctrl-P  alias for Ctrl-P i\n"
	"  Ctrl-P Ctrl-R  reset emulated machine\n"
	"  h/?   show this help\n"
	"  q     quit emulator\n\n";
const char *ui_help_string(bool direct)
{
	return direct ? g_help_direct : g_help_prefixed;
}

static void panel_dump(const AltaidHW *hw)
{
	uint16_t a = altaid_hw_panel_addr16(hw);
	uint8_t d = altaid_hw_panel_data8(hw);
	uint8_t s = altaid_hw_panel_stat4(hw);

	fprintf(out_stream(),
	"\n[PANEL] ADDR=%04X DATA=%02X STAT=%X  RAM_BANK=%u  ROM_HALF=%u  "
	"ROM_LO=%u  ROM_HI=%u\n",
	a, d, s,
	(unsigned)hw->ram_bank,
	(unsigned)hw->rom_half,
	(unsigned)hw->rom_low_mapped,
	(unsigned)hw->rom_hi_mapped);
}

static void prompt_begin(UI *ui, int kind, const char *label,
				const char *initial)
{
	if (!ui)
		return;

	ui->prompt_active = true;
	ui->prompt_kind = kind;
	ui->prompt_len = 0;
	ui->prompt_buf[0] = '\0';

	if (initial) {
		strncpy(ui->prompt_buf, initial, sizeof(ui->prompt_buf));
		ui->prompt_buf[sizeof(ui->prompt_buf) - 1] = '\0';
		ui->prompt_len = (unsigned)strlen(ui->prompt_buf);
	}

	ui->event = true;

	if (!ui->ui_mode) {
		fprintf(out_stream(), "\n[%s] Enter filename: %s", label,
			ui->prompt_buf);
		fflush(out_stream());
	}
}

static void prompt_cancel(UI *ui)
{
	if (!ui)
		return;
	ui->prompt_active = false;
	ui->prompt_kind = UI_PROMPT_NONE;
	ui->prompt_len = 0;
	ui->prompt_buf[0] = '\0';
	ui->event = true;
	if (!ui->ui_mode) {
		fputs("\n[CANCEL]\n", out_stream());
		fflush(out_stream());
	}
}

static void prompt_commit(UI *ui)
{
	char *s;
	unsigned len;

	if (!ui)
		return;

	s = ui->prompt_buf;
	len = ui->prompt_len;
	while (len && (s[len - 1] == ' ' || s[len - 1] == '\t')) {
		s[len - 1] = '\0';
		len--;
	}
	while (*s == ' ' || *s == '\t')
		s++;

	if (*s == '\0') {
		prompt_cancel(ui);
		return;
	}

	if (ui->prompt_kind == UI_PROMPT_STATE_FILE) {
		strncpy(ui->state_path, s, sizeof(ui->state_path));
		ui->state_path[sizeof(ui->state_path) - 1] = '\0';
		if (!ui->ui_mode) {
			fprintf(out_stream(), "\n[STATE] File set to '%s'\n", ui->state_path);
			fflush(out_stream());
		}
	} else if (ui->prompt_kind == UI_PROMPT_RAM_FILE) {
		strncpy(ui->ram_path, s, sizeof(ui->ram_path));
		ui->ram_path[sizeof(ui->ram_path) - 1] = '\0';
		if (!ui->ui_mode) {
			fprintf(out_stream(), "\n[RAM] File set to '%s'\n", ui->ram_path);
			fflush(out_stream());
		}
	} else if (ui->prompt_kind == UI_PROMPT_CASS_FILE) {
		strncpy(ui->cass_path, s, sizeof(ui->cass_path));
		ui->cass_path[sizeof(ui->cass_path) - 1] = '\0';
		ui->req_cass_attach = true;
		if (!ui->ui_mode) {
			fprintf(out_stream(), "\n[CASS] Attach '%s'\n", ui->cass_path);
			fflush(out_stream());
		}
	}

	ui->prompt_active = false;
	ui->prompt_kind = UI_PROMPT_NONE;
	ui->prompt_len = 0;
	ui->prompt_buf[0] = '\0';
	ui->event = true;
}

static void prompt_handle_char(UI *ui, int ch)
{
	if (!ui)
		return;

	if (ch == '\r' || ch == '\n') {
		prompt_commit(ui);
		return;
	}
	if (ch == 0x1b) { /* ESC */
		prompt_cancel(ui);
		return;
	}
	if (ch == 0x7f || ch == '\b') { /* backspace */
		if (ui->prompt_len) {
			ui->prompt_len--;
			ui->prompt_buf[ui->prompt_len] = '\0';
			ui->event = true;
			if (!ui->ui_mode) {
				fputs("\b \b", out_stream());
				fflush(out_stream());
			}
		}
		return;
	}
	if (ch < 0x20 || ch > 0x7e)
		return;
	if (ui->prompt_len + 1 >= sizeof(ui->prompt_buf))
		return;

	ui->prompt_buf[ui->prompt_len++] = (char)ch;
	ui->prompt_buf[ui->prompt_len] = '\0';
	ui->event = true;
	if (!ui->ui_mode) {
		fputc(ch, out_stream());
		fflush(out_stream());
	}
}

void ui_init(UI *ui)
{
	struct termios t;

	bool show_panel = ui->show_panel;
	bool panel_compact = ui->panel_compact;
	bool ui_mode = ui->ui_mode;
	bool serial_ro = ui->serial_ro;
	bool pty_mode = ui->pty_mode;
	bool pty_input = ui->pty_input;
	char state_path[sizeof(ui->state_path)];
	char ram_path[sizeof(ui->ram_path)];
	char cass_path[sizeof(ui->cass_path)];

	strncpy(state_path, ui->state_path, sizeof(state_path));
	state_path[sizeof(state_path) - 1] = '\0';
	strncpy(ram_path, ui->ram_path, sizeof(ram_path));
	ram_path[sizeof(ram_path) - 1] = '\0';
	strncpy(cass_path, ui->cass_path, sizeof(cass_path));
	cass_path[sizeof(cass_path) - 1] = '\0';

	ui->panel_prefix = false;
	ui->show_panel = show_panel;
	ui->panel_compact = panel_compact;
	ui->ui_mode = ui_mode;
	ui->serial_ro = pty_mode ? !pty_input : serial_ro;
	ui->help_requested = false;
	ui->help_direct = false;
	ui->reset = false;
	ui->quit = false;
	ui->pty_mode = pty_mode;
	ui->pty_input = pty_input;
	ui->event = false;
	ui->prompt_kind = UI_PROMPT_NONE;
	ui->prompt_active = false;
	ui->prompt_len = 0;
	ui->prompt_buf[0] = '\0';
	ui->req_state_save = false;
	ui->req_state_load = false;
	ui->req_ram_save = false;
	ui->req_ram_load = false;
	ui->req_cass_attach = false;
	ui->req_cass_save = false;
	ui->req_cass_play = false;
	ui->req_cass_rec = false;
	ui->req_cass_stop = false;
	ui->req_cass_rewind = false;
	ui->req_cass_ff = false;
	strncpy(ui->state_path, state_path, sizeof(ui->state_path));
	ui->state_path[sizeof(ui->state_path) - 1] = '\0';
	strncpy(ui->ram_path, ram_path, sizeof(ui->ram_path));
	ui->ram_path[sizeof(ui->ram_path) - 1] = '\0';
	strncpy(ui->cass_path, cass_path, sizeof(ui->cass_path));
	ui->cass_path[sizeof(ui->cass_path) - 1] = '\0';

	(void)tcgetattr(STDIN_FILENO, &g_old);
	t = g_old;

	/* Non-canonical, no-echo keyboard input. Keep ISIG so Ctrl-C works. */
	t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
	(void)tcsetattr(STDIN_FILENO, TCSANOW, &t);
	(void)fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void ui_shutdown(UI *ui)
{
	(void)ui;
	(void)tcsetattr(STDIN_FILENO, TCSANOW, &g_old);
}

static void ui_toggle_ui_mode(UI *ui)
{
	ui->ui_mode = !ui->ui_mode;
	ui->event = true;
	if (!ui->ui_mode) {
		fprintf(out_stream(), "\n[UI] UI mode %s (--ui)\n\n",
			ui->ui_mode ? "ENABLED" : "DISABLED");
		fflush(out_stream());
	}
}

static void ui_toggle_panel(UI *ui)
{
	ui->show_panel = !ui->show_panel;
	ui->event = true;
}

static void ui_toggle_panel_compact(UI *ui)
{
	ui->panel_compact = !ui->panel_compact;
	ui->event = true;
	if (!ui->ui_mode) {
		fprintf(out_stream(), "\n[PANEL] Text mode is now %s\n\n",
			ui->panel_compact ? "COMPACT" : "VERBOSE");
		fflush(out_stream());
	}
}

static void ui_request_reset(UI *ui)
{
	ui->reset = true;
	ui->event = true;
	if (!ui->ui_mode) {
		fprintf(out_stream(), "\n[RESET] Machine reset requested\n\n");
		fflush(out_stream());
	}
}

static void ui_toggle_serial_ro(UI *ui)
{
	if (!ui)
		return;

	if (ui->pty_mode && !ui->pty_input) {
		ui->serial_ro = true;
		ui->event = true;
		if (!ui->ui_mode) {
			fprintf(out_stream(),
			"\n[PTY] Local input is read-only (use Ctrl-P t or --pty-input)\n\n");
			fflush(out_stream());
		}
		return;
	}

	ui->serial_ro = !ui->serial_ro;
	ui->event = true;
	if (!ui->ui_mode) {
		fprintf(out_stream(),
			"\n[SERIAL] Local input %s\n\n",
			ui->serial_ro ? "READ-ONLY" : "ENABLED");
		fflush(out_stream());
	}
}

static void ui_toggle_pty_input(UI *ui)
{
	if (!ui)
		return;
	if (!ui->pty_mode) {
		ui->event = true;
		if (!ui->ui_mode) {
			fprintf(out_stream(), "\n[PTY] --pty is not enabled\n\n");
			fflush(out_stream());
		}
		return;
	}

	ui->pty_input = !ui->pty_input;
	ui->serial_ro = ui->pty_input ? false : true;
	ui->event = true;

	if (!ui->ui_mode)
		fprintf(out_stream(),
		"\n[PTY] Local keyboard input %s\n"
		"      %s\n\n",
		ui->pty_input ? "ENABLED" : "DISABLED",
		ui->pty_input ?
		"Panel keys are now Ctrl-P-prefixed." :
		"Panel keys are direct; serial input is read-only.");
	if (!ui->ui_mode)
		fflush(out_stream());
}

static bool ui_handle_panel_key(UI *ui, AltaidHW *hw, int ch,
uint64_t now_tick, uint64_t key_hold_cycles,
bool direct_help)
{
	if (ch >= '1' && ch <= '8') {
		uint8_t key = (uint8_t)(ch - '1');
		altaid_hw_panel_press_key(hw, key, now_tick, key_hold_cycles);
		ui->event = true;
		return true;
	}

	if (ch == 'r') {
		altaid_hw_panel_press_key(hw, 8, now_tick, key_hold_cycles);
		ui->event = true;
		return true;
	}
	if (ch == 'm') {
		altaid_hw_panel_press_key(hw, 9, now_tick, key_hold_cycles);
		ui->event = true;
		return true;
	}
	if (ch == 'n') {
		altaid_hw_panel_press_key(hw, 10, now_tick, key_hold_cycles);
		ui->event = true;
		return true;
	}
	if (ch == 'N') {
		/* Chord: D7 + NEXT */
		altaid_hw_panel_press_key(hw, 7, now_tick, key_hold_cycles);
		altaid_hw_panel_press_key(hw, 10, now_tick, key_hold_cycles);
		ui->event = true;
		return true;
	}

	if (ch == 'p') {
		ui_toggle_panel(ui);
		return true;
	}
	if (ch == 'c') {
		ui_toggle_panel_compact(ui);
		return true;
	}
	if (ch == 'u') {
		ui_toggle_ui_mode(ui);
		return true;
	}
	if (ch == 'i') {
		ui_toggle_serial_ro(ui);
		return true;
	}
	if (ch == 't') {
		ui_toggle_pty_input(ui);
		return true;
	}
	if (ch == 'd') {
		panel_dump(hw);
		ui->event = true;
		return true;
	}
	if (ch == 'h' || ch == '?') {
		ui->help_requested = true;
		ui->help_direct = direct_help;
		ui->event = true;
		return true;
	}
	if (ch == 'q') {
		ui->quit = true;
		return true;
	}

	return false;
}

static void ui_handle_prefixed(UI *ui, SerialDev *ser, AltaidHW *hw, int ch,
uint64_t now_tick, uint64_t key_hold_cycles)
{
	if (ch == KEY_CTRL('R')) {
		ui_request_reset(ui);
		return;
	}
	if (ch == 's') {
		ui->req_state_save = true;
		ui->event = true;
		return;
	}
	if (ch == 'l') {
		ui->req_state_load = true;
		ui->event = true;
		return;
	}
	if (ch == 'f') {
		prompt_begin(ui, UI_PROMPT_STATE_FILE, "STATE", ui->state_path);
		return;
	}
	if (ch == 'b') {
		ui->req_ram_save = true;
		ui->event = true;
		return;
	}
	if (ch == 'g') {
		ui->req_ram_load = true;
		ui->event = true;
		return;
	}
	if (ch == 'M') {
		prompt_begin(ui, UI_PROMPT_RAM_FILE, "RAM", ui->ram_path);
		return;
	}
	if (ch == 'a' || ch == 'A') {
		prompt_begin(ui, UI_PROMPT_CASS_FILE, "CASS", ui->cass_path);
		return;
	}
	if (ch == 'P') {
		ui->req_cass_play = true;
		ui->event = true;
		return;
	}
	if (ch == 'R') {
		ui->req_cass_rec = true;
		ui->event = true;
		return;
	}
	if (ch == 'K') {
		ui->req_cass_stop = true;
		ui->event = true;
		return;
	}
	if (ch == 'W') {
		ui->req_cass_rewind = true;
		ui->event = true;
		return;
	}
	if (ch == 'J') {
		ui->req_cass_ff = true;
		ui->event = true;
		return;
	}
	if (ch == 'V') {
		ui->req_cass_save = true;
		ui->event = true;
		return;
	}

	if (ch == 'i' || ch == 'I') {
		ui_toggle_serial_ro(ui);
		return;
	}
	if (ch == 't' || ch == 'T') {
		ui_toggle_pty_input(ui);
		return;
	}

	(void)ser;
	(void)ui_handle_panel_key(ui, hw, ch, now_tick, key_hold_cycles, false);
}

static void ui_handle_normal_char(UI *ui, SerialDev *ser, int ch)
{
	if (!ser)
		return;
	if (ui && ui->serial_ro)
		return;

	if (ch == '\n')
		ch = '\r';
	serial_host_enqueue(ser, (uint8_t)ch);
	ui->event = true;
}


void ui_poll(UI *ui, SerialDev *ser, AltaidHW *hw, uint64_t now_tick,
uint64_t key_hold_cycles)
{
	uint8_t buf[64];
	ssize_t n;
	bool direct_panel;

	if (!ui || !hw) return;

	direct_panel = (ui->pty_mode && !ui->pty_input);
	altaid_hw_panel_tick(hw, now_tick);

	for (;;) {
		n = read(STDIN_FILENO, buf, sizeof(buf));
		if (n <= 0) break;
		for (ssize_t i = 0; i < n; i++) {
			int ch = buf[i];

			if (ch == KEY_CTRL('C')) {
				ui->quit = true;
				return;
			}

			if (ui->prompt_active) {
				prompt_handle_char(ui, ch);
				continue;
			}

			if (ch == KEY_CTRL('P')) {
				if (ui->panel_prefix) {
					ui->panel_prefix = false;
					ui_toggle_serial_ro(ui);
					continue;
				}
				ui->panel_prefix = true;
				continue;
			}

			if (ui->panel_prefix) {
				ui->panel_prefix = false;
				ui_handle_prefixed(ui, ser, hw, ch, now_tick,
				key_hold_cycles);
				continue;
			}

			if (direct_panel) {
				(void)ui_handle_panel_key(ui, hw, ch, now_tick,
				key_hold_cycles, true);
				continue;
			}

			ui_handle_normal_char(ui, ser, ch);
		}
	}
}
