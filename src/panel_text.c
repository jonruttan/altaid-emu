#include "panel_text.h"
#include "altaid_hw.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct panel_text_snapshot {
	uint16_t	addr;
	uint8_t		data;
	uint8_t		stat;
	uint8_t		ram_bank;
	uint8_t		rom_half;
	uint8_t		timer_en;
	bool		pty_mode;
	bool		pty_input;
	char		pty_name[64];
	bool		have_pty_name;
};

static bool g_started;
static bool g_compact;
static FILE *g_out; /* defaults to stderr */
static enum panel_text_emit_mode g_emit_mode = PANEL_TEXT_EMIT_BURST;
static bool g_have_last;
static struct panel_text_snapshot g_last;

void panel_text_set_output(FILE *out)
{
	g_out = out ? out : stderr;
}

static FILE *out_stream(void)
{
	return g_out ? g_out : stderr;
}

void panel_text_set_compact(bool enable)
{
	g_compact = enable;
}

void panel_text_set_emit_mode(enum panel_text_emit_mode mode)
{
	g_emit_mode = mode;
	g_have_last = false;
	memset(&g_last, 0, sizeof(g_last));
}

void panel_text_begin(void)
{
	if (g_started)
		return;
	g_started = true;
	fprintf(out_stream(), "Altaid 8800 Front Panel (text mode)\n");
	fprintf(out_stream(), "(Use --ui for a colored, refreshable panel)\n\n");
	fflush(out_stream());
}

void panel_text_end(void)
{
	if (!g_started)
		return;
	g_started = false;
}

static void snapshot_from(const AltaidHW *hw, const char *pty_name,
			 bool pty_mode, bool pty_input,
			 struct panel_text_snapshot *snap)
{
	memset(snap, 0, sizeof(*snap));
	snap->addr = altaid_hw_panel_addr16(hw);
	snap->data = altaid_hw_panel_data8(hw);
	snap->stat = altaid_hw_panel_stat4(hw);
	snap->ram_bank = (uint8_t)hw->ram_bank;
	snap->rom_half = (uint8_t)hw->rom_half;
	snap->timer_en = (uint8_t)hw->timer_en;
	snap->pty_mode = pty_mode;
	snap->pty_input = pty_input;
	if (pty_name && *pty_name) {
		snap->have_pty_name = true;
		strncpy(snap->pty_name, pty_name, sizeof(snap->pty_name) - 1);
		snap->pty_name[sizeof(snap->pty_name) - 1] = '\0';
	}
}

static bool snapshot_equal(const struct panel_text_snapshot *a,
			  const struct panel_text_snapshot *b)
{
	if (a->addr != b->addr)
		return false;
	if (a->data != b->data)
		return false;
	if (a->stat != b->stat)
		return false;
	if (a->ram_bank != b->ram_bank)
		return false;
	if (a->rom_half != b->rom_half)
		return false;
	if (a->timer_en != b->timer_en)
		return false;
	if (a->pty_mode != b->pty_mode)
		return false;
	if (a->pty_input != b->pty_input)
		return false;
	if (a->have_pty_name != b->have_pty_name)
		return false;
	if (a->have_pty_name && strcmp(a->pty_name, b->pty_name) != 0)
		return false;
	return true;
}

static void bits16(uint16_t v)
{
	int i;

	for (i = 15; i >= 0; i--)
		fputc((v & (1u << i)) ? '1' : '0', out_stream());
}

static void bits8(uint8_t v)
{
	int i;

	for (i = 7; i >= 0; i--)
		fputc((v & (1u << i)) ? '1' : '0', out_stream());
}

static void bits4(uint8_t v)
{
	int i;

	for (i = 3; i >= 0; i--)
		fputc((v & (1u << i)) ? '1' : '0', out_stream());
}

void panel_text_render(const AltaidHW *hw, const char *pty_name, bool pty_mode,
			bool pty_input, uint64_t tick, uint32_t cpu_hz,
			uint32_t baud)
{
	struct panel_text_snapshot cur;
	const char *mode;

	if (!g_started)
		panel_text_begin();

	if (!hw)
		return;

	snapshot_from(hw, pty_name, pty_mode, pty_input, &cur);

	if (g_emit_mode == PANEL_TEXT_EMIT_CHANGE && g_have_last &&
	    snapshot_equal(&cur, &g_last))
		return;

	g_last = cur;
	g_have_last = true;

	mode = pty_mode ? (pty_input ? "PTY-IN" : "PTY-RO") : "STDIO";

	if (g_compact) {
		fprintf(out_stream(),
			"[%s] CPU=%uHz Baud=%u Tick=%llu  ADDR=%04X  DATA=%02X  STAT=%X  "
			"BANK=%u ROMH=%u TMR=%u%s%s\n",
			mode,
			(unsigned)cpu_hz, (unsigned)baud,
			(unsigned long long)tick,
			cur.addr, cur.data, cur.stat,
			(unsigned)hw->ram_bank, (unsigned)hw->rom_half,
			(unsigned)hw->timer_en,
			cur.have_pty_name ? "  PTY=" : "",
			cur.have_pty_name ? cur.pty_name : "");
		fflush(out_stream());
		return;
	}

	fprintf(out_stream(),
		"[%s] CPU=%uHz Baud=%u Tick=%llu  ADDR=%04X  DATA=%02X  STAT=%X%s%s\n",
		mode,
		(unsigned)cpu_hz, (unsigned)baud,
		(unsigned long long)tick,
		cur.addr, cur.data, cur.stat,
		cur.have_pty_name ? "  PTY=" : "",
		cur.have_pty_name ? cur.pty_name : "");

	fprintf(out_stream(), " A15..A0 : ");
	bits16(cur.addr);
	fprintf(out_stream(), "\n");
	fprintf(out_stream(), "  D7..D0 : ");
	bits8(cur.data);
	fprintf(out_stream(), "\n");
	fprintf(out_stream(), "  S3..S0 : ");
	bits4(cur.stat);
	fprintf(out_stream(), "\n");

	fprintf(out_stream(),
		"Keys: D0..D7=%c%c%c%c%c%c%c%c  RUN=%c MODE=%c NEXT=%c\n",
		hw->fp_key_down[0] ? '1' : '0',
		hw->fp_key_down[1] ? '1' : '0',
		hw->fp_key_down[2] ? '1' : '0',
		hw->fp_key_down[3] ? '1' : '0',
		hw->fp_key_down[4] ? '1' : '0',
		hw->fp_key_down[5] ? '1' : '0',
		hw->fp_key_down[6] ? '1' : '0',
		hw->fp_key_down[7] ? '1' : '0',
		hw->fp_key_down[8] ? '1' : '0',
		hw->fp_key_down[9] ? '1' : '0',
		hw->fp_key_down[10] ? '1' : '0');

	fprintf(out_stream(),
		"RAM_BANK=%u ROM_HALF=%u ROM_LO=%u ROM_HI=%u TIMER=%u\n\n",
		(unsigned)hw->ram_bank, (unsigned)hw->rom_half,
		(unsigned)hw->rom_low_mapped, (unsigned)hw->rom_hi_mapped,
		(unsigned)hw->timer_en);

	fflush(out_stream());
}
