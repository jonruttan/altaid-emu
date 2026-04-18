/*
 * panel_text.spec.c
 *
 * Snapshot tests for the text-mode front-panel renderer. These render a
 * controlled AltaidHW into an fmemopen-backed buffer and compare against
 * the expected output byte-for-byte, so any change to the rendering
 * surface (field order, formatting, units) shows up as a test failure.
 */

/*
 * fmemopen is POSIX 2008 (_POSIX_C_SOURCE >= 200809L). Define these
 * feature-test macros BEFORE any system header is pulled in — on Linux
 * glibc the features mask is latched by the first #include.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "altaid_hw.c"
#include "panel_text.c"

#include "test-runner.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Stub log_printf: altaid_hw.c's debug path is never enabled in tests. */
void log_printf(const char *fmt, ...)
{
	(void)fmt;
}

/*
 * Reset file-static state so each test starts from a known baseline.
 * panel_text.c lazily prints a banner on the first render call, which
 * the tests want included in the snapshot, so g_started is cleared too.
 */
static void panel_text_reset_for_test(FILE *out)
{
	panel_text_end();
	panel_text_set_emit_mode(PANEL_TEXT_EMIT_BURST);
	panel_text_set_compact(true);
	panel_text_set_output(out);
}

static void hw_with_latched(AltaidHW *hw, uint16_t addr, uint8_t data, uint8_t stat)
{
	memset(hw, 0, sizeof(*hw));
	hw->panel_latched_valid = true;
	hw->panel_latched_addr = addr;
	hw->panel_latched_data = data;
	hw->panel_latched_stat = stat;
}

static char *test_panel_text_compact_stdio_snapshot(void)
{
	AltaidHW hw;
	char buf[1024];
	FILE *fout;
	const char *expected =
		"Altaid 8800 Front Panel (text mode)\n"
		"(Use --ui for a colored, refreshable panel)\n"
		"\n"
		"[STDIO] CPU=2000000Hz Baud=9600 Tick=42  "
		"ADDR=ABCD  DATA=5A  STAT=3  BANK=0 ROMH=0 TMR=0\n";

	hw_with_latched(&hw, 0xABCDu, 0x5Au, 0x3u);

	memset(buf, 0, sizeof(buf));
	fout = fmemopen(buf, sizeof(buf), "w");
	if (!fout)
		return "fmemopen() failed";

	panel_text_reset_for_test(fout);
	panel_text_render(&hw, NULL, false, false, 42u, 2000000u, 9600u);
	fclose(fout);

	_it_should(
		"compact STDIO render matches expected line",
		0 == strcmp(buf, expected)
	);

	return NULL;
}

static char *test_panel_text_compact_pty_snapshot(void)
{
	AltaidHW hw;
	char buf[1024];
	FILE *fout;
	const char *expected =
		"Altaid 8800 Front Panel (text mode)\n"
		"(Use --ui for a colored, refreshable panel)\n"
		"\n"
		"[PTY-RO] CPU=2000000Hz Baud=9600 Tick=100  "
		"ADDR=1234  DATA=AB  STAT=0  BANK=0 ROMH=0 TMR=0"
		"  PTY=/dev/ttys123\n";

	hw_with_latched(&hw, 0x1234u, 0xABu, 0x0u);

	memset(buf, 0, sizeof(buf));
	fout = fmemopen(buf, sizeof(buf), "w");
	if (!fout)
		return "fmemopen() failed";

	panel_text_reset_for_test(fout);
	panel_text_render(&hw, "/dev/ttys123", true, false, 100u, 2000000u, 9600u);
	fclose(fout);

	_it_should(
		"PTY mode names show PTY path",
		0 == strcmp(buf, expected)
	);

	return NULL;
}

static char *test_panel_text_verbose_snapshot(void)
{
	AltaidHW hw;
	char buf[2048];
	FILE *fout;
	const char *expected =
		"Altaid 8800 Front Panel (text mode)\n"
		"(Use --ui for a colored, refreshable panel)\n"
		"\n"
		"[STDIO] CPU=2000000Hz Baud=9600 Tick=7  "
		"ADDR=00FF  DATA=01  STAT=2\n"
		" A15..A0 : 0000000011111111\n"
		"  D7..D0 : 00000001\n"
		"  S3..S0 : 0010\n"
		"Keys: D0..D7=00000000  RUN=0 MODE=0 NEXT=0\n"
		"RAM_BANK=0 ROM_HALF=0 ROM_LO=0 ROM_HI=0 TIMER=0\n"
		"\n";

	hw_with_latched(&hw, 0x00FFu, 0x01u, 0x2u);

	memset(buf, 0, sizeof(buf));
	fout = fmemopen(buf, sizeof(buf), "w");
	if (!fout)
		return "fmemopen() failed";

	panel_text_reset_for_test(fout);
	panel_text_set_compact(false);
	panel_text_render(&hw, NULL, false, false, 7u, 2000000u, 9600u);
	fclose(fout);

	_it_should(
		"verbose render emits header + bit rows + keys + banks",
		0 == strcmp(buf, expected)
	);

	return NULL;
}

static char *test_panel_text_change_mode_suppresses_duplicates(void)
{
	AltaidHW hw;
	char buf[2048];
	FILE *fout;
	size_t after_first;

	hw_with_latched(&hw, 0xAAAAu, 0x55u, 0x1u);

	memset(buf, 0, sizeof(buf));
	fout = fmemopen(buf, sizeof(buf), "w");
	if (!fout)
		return "fmemopen() failed";

	panel_text_reset_for_test(fout);
	panel_text_set_emit_mode(PANEL_TEXT_EMIT_CHANGE);

	panel_text_render(&hw, NULL, false, false, 0u, 2000000u, 9600u);
	fflush(fout);
	after_first = strlen(buf);

	panel_text_render(&hw, NULL, false, false, 1u, 2000000u, 9600u);
	fflush(fout);

	fclose(fout);

	_it_should(
		"change-mode second identical render emits nothing",
		after_first > 0
		&& strlen(buf) == after_first
	);

	return NULL;
}

static char *test_panel_text_bits_helpers(void)
{
	char buf[64];
	FILE *fout;

	memset(buf, 0, sizeof(buf));
	fout = fmemopen(buf, sizeof(buf), "w");
	if (!fout)
		return "fmemopen() failed";
	panel_text_set_output(fout);

	bits16(0xA5A5u);
	fflush(fout);
	_it_should(
		"bits16 writes 16 MSB-first bits",
		0 == strcmp(buf, "1010010110100101")
	);

	rewind(fout);
	memset(buf, 0, sizeof(buf));
	bits8(0x5Au);
	fflush(fout);
	_it_should(
		"bits8 writes 8 MSB-first bits",
		0 == strcmp(buf, "01011010")
	);

	rewind(fout);
	memset(buf, 0, sizeof(buf));
	bits4(0xCu);
	fflush(fout);
	_it_should(
		"bits4 writes 4 MSB-first bits",
		0 == strcmp(buf, "1100")
	);

	fclose(fout);
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_panel_text_compact_stdio_snapshot);
	_run_test(test_panel_text_compact_pty_snapshot);
	_run_test(test_panel_text_verbose_snapshot);
	_run_test(test_panel_text_change_mode_suppresses_duplicates);
	_run_test(test_panel_text_bits_helpers);
	return NULL;
}
