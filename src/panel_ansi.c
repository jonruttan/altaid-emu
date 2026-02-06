#include "panel_ansi.h"
#include "altaid_hw.h"
#include "io.h"

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <sys/ioctl.h>

/* fileno() is POSIX; declare for strict C99 builds. */
extern int fileno(FILE *stream);

/*
* wcwidth() is a POSIX extension and may not be declared when building with
* strict C99 feature macros. A compatible declaration is fine even if the
* header already declares it.
*/
extern int wcwidth(wchar_t wc);

/*
* ANSI front-panel renderer.
*
* Important behavior:
*   - In refresh mode (live panel), we use the alternate screen buffer so we
*     don't nuke the user's scrollback or the content already on screen.
*   - Rendering is done into a single buffer and written in one shot to reduce
*     "tearing" (partial frames) during frequent refresh.
*/

static bool g_active = false;
static FILE *g_out = NULL; /* defaults to stderr */
static bool g_ascii = false;
static bool g_refresh = false;
static bool g_alt = false;
static bool g_alt_enable = true;
static bool g_split = false;
static bool g_size_override = false;
static int  g_override_rows = 0;
static int  g_override_cols = 0;
static bool g_layout_ready = false;
static int  g_term_rows = 0;
static int  g_term_cols = 0;
static int  g_panel_cols = 0;       /* total columns including borders */
static int  g_panel_inner_cols = 0; /* visible columns inside bordered_line */

static bool g_panel_visible = false;
static bool g_serial_ro = false;
static bool g_statusline = true;
static bool g_status_override_set;
static char g_status_override[512];

/* ----- TUI serial view (deterministic redraw) ----- */

/*
 * Keep a small ring of recent serial lines and re-render them in the serial
 * region on each refresh. This avoids terminal scroll-region quirks and
 * cross-stream interleaving that can corrupt the panel.
 */
enum {
	SERIAL_RING_LINES = 512,
	SERIAL_LINE_CAP = 1024,
};

static char g_ser_lines[SERIAL_RING_LINES][SERIAL_LINE_CAP];
static unsigned g_ser_head = 0; /* next insert */
static unsigned g_ser_count = 0;
static char g_ser_cur[SERIAL_LINE_CAP];
static size_t g_ser_cur_len = 0;
static bool g_ser_prev_cr = false;

static bool g_panel_effective = true;
static int  g_serial_top = 1;
static int  g_serial_bottom = 1;
static int  g_status_row = 0;

static bool g_last_panel_effective = true;
static int  g_last_serial_top = 1;
static int  g_last_serial_bottom = 1;
static int  g_last_status_row = 0;

void panel_ansi_set_output(FILE *out)
{
	g_out = out ? out : stderr;
}

static FILE *out_stream(void)
{
	return g_out ? g_out : stderr;
}

static int out_fd(void)
{
	FILE *o = out_stream();
	int fd = fileno(o);
	return (fd >= 0) ? fd : STDERR_FILENO;
}

static void term_write_full(const void *buf, size_t len)
{
	if (!panel_ansi_is_tty() || !g_refresh)
		return;
	(void)write_full(out_fd(), buf, len);
}

/* Fixed number of lines rendered by panel_ansi_render(). */
enum { PANEL_LINES = 17 };

static int clamp_int(int v, int lo, int hi)
{
	if (v < lo) return lo;
	if (v > hi) return hi;
	return v;
}

static bool env_int(const char *name, int *out)
{
	const char *v = getenv(name);
	if (!v || !*v) return false;
	char *end = NULL;
	long n = strtol(v, &end, 10);
	if (end == v) return false;
	if (n <= 0 || n > 10000) return false;
	*out = (int)n;
	return true;
}

static void probe_term_size(int *out_rows, int *out_cols)
{
	int rows = 0;
	int cols = 0;
	struct winsize ws;

	memset(&ws, 0, sizeof(ws));
	/* Prefer the output fd we render to. */
	if (ioctl(out_fd(), TIOCGWINSZ, &ws) == 0) {
		if (ws.ws_row > 0)
			rows = (int)ws.ws_row;
		if (ws.ws_col > 0)
			cols = (int)ws.ws_col;
	}
	/* Fallback: try the other standard stream if it's a tty. */
	if ((rows <= 0 || cols <= 0) && isatty(STDOUT_FILENO)) {
		memset(&ws, 0, sizeof(ws));
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
			if (rows <= 0 && ws.ws_row > 0)
				rows = (int)ws.ws_row;
			if (cols <= 0 && ws.ws_col > 0)
				cols = (int)ws.ws_col;
		}
	}
	if ((rows <= 0 || cols <= 0) && isatty(STDERR_FILENO)) {
		memset(&ws, 0, sizeof(ws));
		if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0) {
			if (rows <= 0 && ws.ws_row > 0)
				rows = (int)ws.ws_row;
			if (cols <= 0 && ws.ws_col > 0)
				cols = (int)ws.ws_col;
		}
	}

	/* Fallback: environment variables. */
	if (rows <= 0)
		(void)env_int("LINES", &rows);
	if (cols <= 0)
		(void)env_int("COLUMNS", &cols);

	/* Final fallback: reasonable default. */
	if (rows <= 0)
		rows = 25;
	if (cols <= 0)
		cols = 80;

	*out_rows = rows;
	*out_cols = cols;
}

static void recompute_layout(void)
{
	int probe_rows = 0;
	int probe_cols = 0;
	int rows;
	int cols;

	/* Ensure locale is initialized so wcwidth() works for UTF-8 glyphs. */
	(void)setlocale(LC_CTYPE, "");

	probe_term_size(&probe_rows, &probe_cols);
	rows = probe_rows;
	cols = probe_cols;

	/* CLI overrides take precedence per-dimension (0 means: probe that dimension). */
	if (g_size_override) {
		if (g_override_rows > 0)
			rows = g_override_rows;
		if (g_override_cols > 0)
			cols = g_override_cols;
	}

	if (rows <= 0)
		rows = 25;
	if (cols <= 0)
		cols = 80;

	g_term_rows = rows;
	g_term_cols = cols;
	g_layout_ready = true;

	/* Statusline occupies the last row when enabled. */
	g_status_row = g_statusline ? g_term_rows : 0;

	/* Determine whether the panel can be shown without overlapping serial. */
	g_panel_effective = g_panel_visible;
	if (g_panel_effective) {
		int min_rows = PANEL_LINES + 1; /* panel + >=1 serial */
		if (g_statusline)
			min_rows += 1;
		if (g_term_rows < min_rows)
			g_panel_effective = false;
	}

	g_serial_top = g_panel_effective ? (PANEL_LINES + 1) : 1;
	g_serial_bottom = g_term_rows - (g_statusline ? 1 : 0);
	if (g_serial_bottom < g_serial_top) {
		/* Too small even without panel; collapse to one usable row. */
		g_panel_effective = false;
		g_serial_top = 1;
		g_serial_bottom = g_term_rows;
		if (g_statusline && g_serial_bottom > 1)
			g_serial_bottom = g_term_rows - 1;
	}

	/* Make sure the panel fits within the terminal width to avoid wrapping. */
	g_panel_cols = clamp_int(g_term_cols, 40, 300);
	g_panel_inner_cols = g_panel_cols - 4; /* "| " + inner + " |" */
	if (g_panel_inner_cols < 10)
		g_panel_inner_cols = 10;
}

static void apply_split_region(void)
{
	/*
	 * Historic versions used a terminal scrolling region to keep the panel
	 * and serial output separated. In practice, terminal+tmux combinations can
	 * be fragile when the program mixes stdout/stderr or stdio/write().
	 *
	 * We now do a deterministic redraw: keep a serial line buffer and paint the
	 * serial region explicitly during panel_ansi_render().
	 */
	recompute_layout();
}

static void ser_commit_line(void)
{
	memcpy(g_ser_lines[g_ser_head], g_ser_cur, g_ser_cur_len);
	g_ser_lines[g_ser_head][g_ser_cur_len] = '\0';
	g_ser_head = (g_ser_head + 1u) % SERIAL_RING_LINES;
	if (g_ser_count < SERIAL_RING_LINES)
		g_ser_count++;
	g_ser_cur_len = 0;
	g_ser_cur[0] = '\0';
}

void panel_ansi_serial_reset(void)
{
	g_ser_head = 0;
	g_ser_count = 0;
	g_ser_cur_len = 0;
	g_ser_cur[0] = '\0';
	g_ser_prev_cr = false;
	for (unsigned i = 0; i < SERIAL_RING_LINES; i++)
		g_ser_lines[i][0] = '\0';
}

void panel_ansi_serial_feed(const uint8_t *buf, size_t len)
{
	if (!buf || !len)
		return;

	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)buf[i];

		if (c == '\r') {
			ser_commit_line();
			g_ser_prev_cr = true;
			continue;
		}
		if (c == '\n') {
			if (g_ser_prev_cr) {
				g_ser_prev_cr = false;
				continue;
			}
			ser_commit_line();
			continue;
		}
		g_ser_prev_cr = false;

		if (c == '\b' || c == 127) {
			if (g_ser_cur_len > 0)
				g_ser_cur_len--;
			g_ser_cur[g_ser_cur_len] = '\0';
			continue;
		}
		if (c == '\t') {
			int spaces = 8 - ((int)g_ser_cur_len % 8);
			while (spaces-- > 0 && g_ser_cur_len + 1 < SERIAL_LINE_CAP) {
				g_ser_cur[g_ser_cur_len++] = ' ';
				g_ser_cur[g_ser_cur_len] = '\0';
			}
			continue;
		}
		if (c < 0x20) {
			/* Drop other control chars in the on-screen view. */
			continue;
		}

		if (g_ser_cur_len + 1 < SERIAL_LINE_CAP) {
			g_ser_cur[g_ser_cur_len++] = (char)c;
			g_ser_cur[g_ser_cur_len] = '\0';
		}
	}
}

bool panel_ansi_is_tty(void)
{
	return isatty(out_fd()) != 0;
}

static void buf_append(char *buf, size_t cap, size_t *len, const char *fmt, ...)
{
	if (*len >= cap) return;
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf + *len, cap - *len, fmt, ap);
	va_end(ap);
	if (n < 0) return;
	size_t nn = (size_t)n;
	if (nn >= cap - *len) {
		*len = cap - 1;
		buf[*len] = '\0';
		return;
	}
	*len += nn;
}

/* ----- Boxed line helpers (handle ANSI sequences when padding) ----- */

static bool is_csi_final(unsigned char c)
{
	return (c >= '@' && c <= '~');
}

/*
* Append up to max_cols visible columns from s.
*
* - CSI sequences do not count toward width.
* - UTF-8 text width is measured using wcwidth().
*/
static void buf_append_visible(char *buf, size_t cap, size_t *len, const char *s, int max_cols, int *out_cols)
{
	int cols = 0;
	mbstate_t st;
	memset(&st, 0, sizeof(st));

	for (size_t i=0; s[i] && cols < max_cols; ) {
		unsigned char c = (unsigned char)s[i];
		if (c == 0x1B && s[i+1] == '[') {
			/* CSI sequence */
			size_t j = i + 2;
			while (s[j] && !is_csi_final((unsigned char)s[j])) j++;
			if (s[j]) j++; /* include final */
			buf_append(buf, cap, len, "%.*s", (int)(j - i), s + i);
			i = j;
			continue;
		}

		/* Decode a single UTF-8 sequence (or fall back to a byte). */
		wchar_t wc;
		size_t n = mbrtowc(&wc, s + i, MB_CUR_MAX, &st);
		if (n == (size_t)-1 || n == (size_t)-2 || n == 0) {
			/* Invalid/incomplete sequence: treat as single byte. */
			memset(&st, 0, sizeof(st));
			if (cols + 1 > max_cols) break;
			buf_append(buf, cap, len, "%c", (char)c);
			cols += 1;
			i += 1;
			continue;
		}

		int w = wcwidth(wc);
		if (w < 0) w = 1;
		if (cols + w > max_cols) break;
		buf_append(buf, cap, len, "%.*s", (int)n, s + i);
		cols += w;
		i += n;
	}
	if (out_cols) *out_cols = cols;
}

static const char *vbar_l(void)
{
	return g_ascii ? "| " : "\xE2\x94\x82 "; /* │ */
}

static const char *vbar_r(void)
{
	return g_ascii ? " |\r\n" : " \xE2\x94\x82\r\n"; /* │ */
}

static void bordered_line(char *buf, size_t cap, size_t *len,
			 const char *content)
{
	if (g_panel_inner_cols <= 0) recompute_layout();
	buf_append(buf, cap, len, "%s", vbar_l());
	int cols = 0;
	buf_append_visible(buf, cap, len, content, g_panel_inner_cols, &cols);
	/* If we truncated something that might have styling, reset SGR. */
	if ((int)strlen(content) > 0 && cols >= g_panel_inner_cols) buf_append(buf, cap, len, "\x1b[0m");
	for (int i=cols; i<g_panel_inner_cols; i++) buf_append(buf, cap, len, " ");
	buf_append(buf, cap, len, "%s", vbar_r());
}

static void border_hline(char *buf, size_t cap, size_t *len,
			 const char *l, const char *h, const char *r)
{
	if (g_panel_cols <= 0)
		recompute_layout();

	buf_append(buf, cap, len, "%s", l);
	for (int i = 0; i < g_panel_cols - 2; i++)
		buf_append(buf, cap, len, "%s", h);
	buf_append(buf, cap, len, "%s\r\n", r);
}

static void border_top(char *buf, size_t cap, size_t *len)
{
	if (g_ascii)
		border_hline(buf, cap, len, "+", "-", "+");
	else
		border_hline(buf, cap, len,
			"\xE2\x94\x8C", "\xE2\x94\x80", "\xE2\x94\x90");
	/* ┌ ─ ┐ */
}

static void border_mid(char *buf, size_t cap, size_t *len)
{
	if (g_ascii)
		border_hline(buf, cap, len, "+", "-", "+");
	else
		border_hline(buf, cap, len,
			"\xE2\x94\x9C", "\xE2\x94\x80", "\xE2\x94\xA4");
	/* ├ ─ ┤ */
}

static void border_bottom(char *buf, size_t cap, size_t *len)
{
	if (g_ascii)
		border_hline(buf, cap, len, "+", "-", "+");
	else
		border_hline(buf, cap, len,
			"\xE2\x94\x94", "\xE2\x94\x80", "\xE2\x94\x98");
	/* └ ─ ┘ */
}

void panel_ansi_begin(void)
{
	if (g_active) return;
	g_active = true;
	panel_ansi_serial_reset();

	if (panel_ansi_is_tty() && g_refresh) {
		/*
		* Use alternate screen so we don't destroy what's already on screen.
		* Hide cursor while active.
		*/
		if (g_alt_enable) {
			g_alt = true;
			term_write_full("\x1b[?1049h\x1b[H\x1b[2J\x1b[?25l",
				strlen("\x1b[?1049h\x1b[H\x1b[2J\x1b[?25l"));
			apply_split_region();
		} else {
			/* non-destructive mode: do not enter alt screen */
			g_alt = false;
			term_write_full("\x1b[?25l", strlen("\x1b[?25l"));
			apply_split_region();
		}
	} else {
		g_alt = false;
	}
}

void panel_ansi_end(void)
{
	if (!g_active) return;

	if (panel_ansi_is_tty() && g_refresh) {
		/* Reset attributes, show cursor, leave alternate screen. */
		if (g_alt) {
			term_write_full("\x1b[0m\x1b[?25h\x1b[?1049l\r\n",
				strlen("\x1b[0m\x1b[?25h\x1b[?1049l\r\n"));
		} else {
			term_write_full("\x1b[0m\x1b[?25h\r\n",
				strlen("\x1b[0m\x1b[?25h\r\n"));
		}
	}

	g_active = false;
	g_alt = false;
}

void panel_ansi_set_refresh(bool enable)
{
	g_refresh = enable;
}

void panel_ansi_set_ascii(bool enable)
{
	g_ascii = enable;
}

void panel_ansi_set_split(bool enable)
{
	g_split = enable;
}

void panel_ansi_set_panel_visible(bool enable)
{
	g_panel_visible = enable;
}

void panel_ansi_set_serial_ro(bool enable)
{
	g_serial_ro = enable;
}

void panel_ansi_set_statusline(bool enable)
{
	g_statusline = enable;
}

void panel_ansi_set_status_override(const char *s)
{
	if (!s) {
		g_status_override_set = false;
		g_status_override[0] = '\0';
		return;
	}
	g_status_override_set = true;
	strncpy(g_status_override, s, sizeof(g_status_override));
	g_status_override[sizeof(g_status_override) - 1] = '\0';
}

void panel_ansi_clear_status_override(void)
{
	panel_ansi_set_status_override(NULL);
}

void panel_ansi_set_altscreen(bool enable)
{
	g_alt_enable = enable;
}

void panel_ansi_set_term_size_override(bool enable, int rows, int cols)
{
	g_size_override = enable;
	if (enable) {
		/* 0 means: probe that dimension. */
		g_override_rows = (rows > 0) ? rows : 0;
		g_override_cols = (cols > 0) ? cols : 0;
	} else {
		g_override_rows = 0;
		g_override_cols = 0;
	}
	/* Force layout recompute on next render/region apply. */
	g_term_rows = 0;
	g_term_cols = 0;
	g_layout_ready = false;

	g_panel_cols = 0;
	g_panel_inner_cols = 0;
}

void panel_ansi_handle_resize(void)
{
	if (!g_active) return;
	g_term_rows = 0;
	g_term_cols = 0;
	g_panel_cols = 0;
	g_panel_inner_cols = 0;
	g_layout_ready = false;

	if (panel_ansi_is_tty() && g_refresh) {
		if (g_alt) {
			/* Redraw from a clean slate in alt screen. */
			term_write_full("\x1b[H\x1b[2J\x1b[?25l",
				strlen("\x1b[H\x1b[2J\x1b[?25l"));
			apply_split_region();
		}
		if (!g_alt && g_split) {
			/* Re-apply region in non-alt refresh too. */
			apply_split_region();
		}
	}
}

void panel_ansi_goto_serial(void)
{
	if (!g_active || !g_split)
		return;
	if (!g_layout_ready)
		recompute_layout();
	if (g_serial_bottom < g_serial_top)
		return;
	char seq[32];
	int n = snprintf(seq, sizeof(seq), "\x1b[%d;1H", g_serial_bottom);
	if (n > 0)
		term_write_full(seq, (size_t)n);
}

/* ---------- Rendering helpers (buffered) ---------- */

static void led(char *buf, size_t cap, size_t *len, bool on)
{
	const char *on_g  = g_ascii ? "*" : "\xE2\x97\x8F"; /* ● */
	const char *off_g = g_ascii ? "." : "\xC2\xB7";     /* · */
	if (on) buf_append(buf, cap, len, "\x1b[91m%s\x1b[0m", on_g);
	else   buf_append(buf, cap, len, "\x1b[90m%s\x1b[0m", off_g);
}

static void led_bits16(char *buf, size_t cap, size_t *len, uint16_t v)
{
	for (int i=15;i>=0;i--) {
		led(buf, cap, len, (v & (1u<<i)) != 0);
		if (i==12 || i==8 || i==4) buf_append(buf, cap, len, " ");
	}
}

static void led_bits8(char *buf, size_t cap, size_t *len, uint8_t v)
{
	for (int i=7;i>=0;i--) {
		led(buf, cap, len, (v & (1u<<i)) != 0);
		if (i==4) buf_append(buf, cap, len, " ");
	}
}

static void led_bits4(char *buf, size_t cap, size_t *len, uint8_t v)
{
	for (int i=3;i>=0;i--) led(buf, cap, len, (v & (1u<<i)) != 0);
}

static void button(char *buf, size_t cap, size_t *len, const char *label, bool pressed)
{
	if (pressed) buf_append(buf, cap, len, "\x1b[7m[%-4s]\x1b[0m", label);
	else        buf_append(buf, cap, len, "[%-4s]", label);
}

static void data_buttons(char *buf, size_t cap, size_t *len, const AltaidHW* hw)
{
	for (int i=0;i<8;i++) {
		char lb[3];
		lb[0] = 'D';
		lb[1] = (char)('0' + i);
		lb[2] = '\0';
		if (hw->fp_key_down[i]) buf_append(buf, cap, len, "\x1b[7m[%s]\x1b[0m ", lb);
		else                   buf_append(buf, cap, len, "[%s] ", lb);
	}
}

static const char *ser_line_from_end(unsigned idx_from_end)
{
	unsigned k;
	unsigned pos;

	if (idx_from_end == 0)
		return g_ser_cur;
	k = idx_from_end - 1u;
	if (k >= g_ser_count)
		return "";
	pos = (g_ser_head + SERIAL_RING_LINES - 1u - k) % SERIAL_RING_LINES;
	return g_ser_lines[pos];
}

void panel_ansi_render(const AltaidHW *hw, const char *pty_name,
bool pty_mode, bool pty_input, uint64_t tick,
uint32_t cpu_hz, uint32_t baud)
{
	bool layout_changed;
	int panel_clear_lines;

	(void)pty_input;

	if (!g_active)
		panel_ansi_begin();

	recompute_layout();
	layout_changed = (g_panel_effective != g_last_panel_effective) ||
		(g_serial_top != g_last_serial_top) ||
		(g_serial_bottom != g_last_serial_bottom) ||
		(g_status_row != g_last_status_row);

	if (layout_changed && panel_ansi_is_tty() && g_refresh) {
		if (g_alt) {
			term_write_full("\x1b[H\x1b[2J\x1b[?25l",
				strlen("\x1b[H\x1b[2J\x1b[?25l"));
			apply_split_region();
		} else {
			/*
			 * In non-alt mode, avoid a full-screen clear (preserve scrollback).
			 * Instead, clear only the UI-owned rows.
			 */
			panel_clear_lines = PANEL_LINES;
			for (int r = 1; r <= panel_clear_lines; r++) {
				char seq[32];
				int nseq = snprintf(seq, sizeof(seq),
					"\x1b[%d;1H\x1b[2K", r);
				if (nseq > 0)
					term_write_full(seq, (size_t)nseq);
			}
			if (g_statusline) {
				char seq[32];
				int nseq = snprintf(seq, sizeof(seq),
					"\x1b[%d;1H\x1b[2K", g_term_rows);
				if (nseq > 0)
					term_write_full(seq, (size_t)nseq);
			}
			apply_split_region();
		}
	}

	g_last_panel_effective = g_panel_effective;
	g_last_serial_top = g_serial_top;
	g_last_serial_bottom = g_serial_bottom;
	g_last_status_row = g_status_row;

	/* Build a frame into a buffer and write it in one shot. */
	char out[16384];
	size_t n = 0;
	out[0] = '\0';

	if (panel_ansi_is_tty() && g_refresh) {
		if (g_panel_effective)
			buf_append(out, sizeof(out), &n, "\x1b[H");
		else
			buf_append(out, sizeof(out), &n, "\x1b[%d;1H", g_serial_bottom);
	}

	if (g_panel_effective) {
		uint16_t a = altaid_hw_panel_addr16(hw);
		uint8_t  d = altaid_hw_panel_data8(hw);
		uint8_t  s = altaid_hw_panel_stat4(hw);

		border_top(out, sizeof(out), &n);
		bordered_line(out, sizeof(out), &n,
			"Altaid 8800 - Panel (Ctrl-P h help)");

		{
			char l[256];
			if (pty_mode)
				snprintf(l, sizeof(l), "PTY %s (read-only)",
					(pty_name && pty_name[0]) ? pty_name : "(none)");
			else
				snprintf(l, sizeof(l), "PTY (disabled)");
			bordered_line(out, sizeof(out), &n, l);
		}

		{
			char l[256];
			snprintf(l, sizeof(l), "CPU %u Hz   Baud %u   Tick %llu",
				(unsigned)cpu_hz, (unsigned)baud,
				(unsigned long long)tick);
			bordered_line(out, sizeof(out), &n, l);
		}
		border_mid(out, sizeof(out), &n);

		{
			char l[512];
			size_t ln = 0;
			l[0] = '\0';
			buf_append(l, sizeof(l), &ln, "ADDR %04X | ", a);
			led_bits16(l, sizeof(l), &ln, a);
			bordered_line(out, sizeof(out), &n, l);
		}
		{
			char l[512];
			size_t ln = 0;
			l[0] = '\0';
			buf_append(l, sizeof(l), &ln, "DATA %02X | ", d);
			led_bits8(l, sizeof(l), &ln, d);
			bordered_line(out, sizeof(out), &n, l);
		}
		{
			char l[512];
			size_t ln = 0;
			l[0] = '\0';
			buf_append(l, sizeof(l), &ln, "STAT %X | ", (unsigned)(s & 0x0F));
			led_bits4(l, sizeof(l), &ln, (uint8_t)(s & 0x0F));
			bordered_line(out, sizeof(out), &n, l);
		}

		border_mid(out, sizeof(out), &n);
		{
			char l[1024];
			size_t ln = 0;
			l[0] = '\0';
			buf_append(l, sizeof(l), &ln, "DATA KEYS: ");
			data_buttons(l, sizeof(l), &ln, hw);
			bordered_line(out, sizeof(out), &n, l);
		}
		{
			char l[1024];
			size_t ln = 0;
			l[0] = '\0';
			buf_append(l, sizeof(l), &ln, "CONTROL : ");
			button(l, sizeof(l), &ln, "RUN", hw->fp_key_down[8]);
			buf_append(l, sizeof(l), &ln, " ");
			button(l, sizeof(l), &ln, "MODE", hw->fp_key_down[9]);
			buf_append(l, sizeof(l), &ln, " ");
			button(l, sizeof(l), &ln, "NEXT", hw->fp_key_down[10]);
			buf_append(l, sizeof(l), &ln, "  (N=NEXT+D7 back)");
			bordered_line(out, sizeof(out), &n, l);
		}

		border_mid(out, sizeof(out), &n);
		{
			char l[512];
			snprintf(l, sizeof(l),
				"RAM bank %u  ROM half %u  ROM@0000 %s  ROM@8000 %s  TIMER %s",
				(unsigned)hw->ram_bank,
				(unsigned)hw->rom_half,
				hw->rom_low_mapped ? "ON" : "off",
				hw->rom_hi_mapped ? "ON" : "off",
				hw->timer_en ? "ON" : "off");
			bordered_line(out, sizeof(out), &n, l);
		}
		border_mid(out, sizeof(out), &n);
		bordered_line(out, sizeof(out), &n,
			"(p) panel  (i) serial ro  (u) ui  (d) dump  (q) quit");
		border_bottom(out, sizeof(out), &n);

		if (panel_ansi_is_tty() && g_refresh) {
			/* Never leave cursor in the panel area in split mode. */
			buf_append(out, sizeof(out), &n,
				"\x1b[0m\x1b[%d;1H", g_serial_bottom);
		}
	}

	/*
	 * Serial area: render from our line buffer. Clear each row and paint the
	 * visible portion, leaving the cursor in the serial region.
	 */
	if (panel_ansi_is_tty() && g_refresh &&
		g_serial_bottom >= g_serial_top) {
		int cols = (g_term_cols > 0) ? g_term_cols : 80;
		for (int row = g_serial_top; row <= g_serial_bottom; row++) {
			unsigned idx = (unsigned)(g_serial_bottom - row);
			int used_cols = 0;
			const char *line = ser_line_from_end(idx);

			buf_append(out, sizeof(out), &n,
				"\x1b[%d;1H\x1b[2K", row);
			buf_append_visible(out, sizeof(out), &n,
				line, cols, &used_cols);
		}
		buf_append(out, sizeof(out), &n,
			"\x1b[%d;1H", g_serial_bottom);
	}

	if (g_statusline && panel_ansi_is_tty() && g_refresh) {
		char st[512];
		int used_cols = 0;
		int cols = (g_term_cols > 0) ? g_term_cols : 80;
		const char *pstate = g_panel_visible ?
			(g_panel_effective ? "ON" : "SMALL") : "OFF";

		if (g_status_override_set) {
			strncpy(st, g_status_override, sizeof(st));
			st[sizeof(st) - 1] = '\0';
		} else {
			snprintf(st, sizeof(st),
				"Panel:%s  Serial:%s  PTY:%s  Term:%dx%d  Ctrl-P h help",
				pstate,
				g_serial_ro ? "RO" : "RW",
				pty_mode ? "ON" : "OFF",
				g_term_rows, g_term_cols);
		}

		/*
		 * Draw a persistent statusline on the last row.
		 *
		 * Avoid cursor save/restore (ESC 7/8) because it interacts badly with
		 * some terminals + split-region scrolling. We explicitly move back to the
		 * serial area afterwards.
		 */
		buf_append(out, sizeof(out), &n,
			"\x1b[%d;1H\x1b[2K\x1b[7m", g_status_row);
		buf_append_visible(out, sizeof(out), &n, st, cols, &used_cols);
		for (int i = used_cols; i < cols; i++)
			buf_append(out, sizeof(out), &n, " ");
		buf_append(out, sizeof(out), &n, "\x1b[0m");

		/* Always leave the cursor inside the serial area in split mode. */
		if (g_split && g_serial_bottom >= g_serial_top)
			buf_append(out, sizeof(out), &n,
				"\x1b[%d;1H", g_serial_bottom);
	}

	term_write_full(out, n);
}
