/*
 * altaid_hw.spec.c
 *
 * Tests for altaid_hw.c. Includes altaid_hw.c directly so that file-static
 * helpers (e.g. panel_switch_nibble_for_row) are visible to the specs.
 */

#include "altaid_hw.c"

#include "test-runner.h"

#include <stdarg.h>

/*
 * Stub log_printf to satisfy altaid_hw.c's diagnostic path without linking
 * log.c. Tests never enable the debug flag, so this is never called in
 * practice; the stub only resolves the symbol at link time.
 */
void log_printf(const char *fmt, ...)
{
	(void)fmt;
}

static char *test_altaid_hw_recompute_ram_bank(void)
{
	return NULL;
}

static char *test_altaid_hw_panel_latch_if_complete(void)
{
	return NULL;
}

static char *test_altaid_hw_panel_switch_nibble_for_row(void)
{
	AltaidHW hw;

	altaid_hw_init(&hw);

	_it_should(
		"report all rows unpressed (0x0F) after init",
		0x0Fu == panel_switch_nibble_for_row(&hw, 4)
		&& 0x0Fu == panel_switch_nibble_for_row(&hw, 5)
		&& 0x0Fu == panel_switch_nibble_for_row(&hw, 6)
	);

	_it_should(
		"report unrelated rows as 0x0F",
		0x0Fu == panel_switch_nibble_for_row(&hw, 0)
		&& 0x0Fu == panel_switch_nibble_for_row(&hw, 3)
		&& 0x0Fu == panel_switch_nibble_for_row(&hw, 7)
	);

	/* D0 pressed -> row 4 bit0 low (active-low) */
	hw.fp_key_down[0] = true;
	_it_should(
		"row 4 drops bit0 when D0 is pressed",
		0x0Eu == panel_switch_nibble_for_row(&hw, 4)
		&& 0x0Fu == panel_switch_nibble_for_row(&hw, 5)
		&& 0x0Fu == panel_switch_nibble_for_row(&hw, 6)
	);

	/* D0 + D3 pressed -> row 4 bits 0 and 3 low */
	hw.fp_key_down[3] = true;
	_it_should(
		"row 4 drops multiple bits for multi-press",
		0x06u == panel_switch_nibble_for_row(&hw, 4)
	);

	/* D5 pressed -> row 5 bit1 low (key index 5 -> 5-4 == 1) */
	hw.fp_key_down[5] = true;
	_it_should(
		"row 5 drops bit(key-4) for D4..D7",
		0x0Du == panel_switch_nibble_for_row(&hw, 5)
	);

	/* RUN (8), MODE (9), NEXT (10) on row 6 */
	altaid_hw_init(&hw);
	hw.fp_key_down[8] = true;
	_it_should(
		"row 6 bit0 is RUN",
		0x0Eu == panel_switch_nibble_for_row(&hw, 6)
	);
	hw.fp_key_down[9] = true;
	_it_should(
		"row 6 bit1 is MODE",
		0x0Cu == panel_switch_nibble_for_row(&hw, 6)
	);
	hw.fp_key_down[10] = true;
	_it_should(
		"row 6 bit2 is NEXT",
		0x08u == panel_switch_nibble_for_row(&hw, 6)
	);

	return NULL;
}

static char *test_altaid_hw_altaid_hw_init(void)
{
	AltaidHW hw;
	bool all_keys_released = true;
	int i;

	/* Fill with noise to verify init zeroes relevant fields. */
	memset(&hw, 0xA5, sizeof(hw));

	altaid_hw_init(&hw);

	for (i = 0; i < 11; i++) {
		if (hw.fp_key_down[i] || hw.fp_key_until[i] != 0) {
			all_keys_released = false;
			break;
		}
	}

	_it_should(
		"clear front-panel key state on init",
		true == all_keys_released
	);

	_it_should(
		"start with ROM low mapped and ROM hi unmapped",
		true == hw.rom_low_mapped
		&& false == hw.rom_hi_mapped
	);

	_it_should(
		"idle serial lines high on init",
		true == hw.tx_line
		&& true == hw.rx_level
	);

	return NULL;
}

static char *test_altaid_hw_altaid_hw_reset_runtime(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_load_rom64k(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_mem_read(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_mem_write(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_io_in(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_io_out(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_press_key(void)
{
	AltaidHW hw;

	altaid_hw_init(&hw);

	altaid_hw_panel_press_key(&hw, 3, 1000ull, 500ull);
	_it_should(
		"set key_down and key_until on press",
		true == hw.fp_key_down[3]
		&& 1500ull == hw.fp_key_until[3]
	);

	_it_should(
		"leave other keys untouched on press",
		false == hw.fp_key_down[0]
		&& false == hw.fp_key_down[2]
		&& false == hw.fp_key_down[4]
		&& false == hw.fp_key_down[10]
	);

	/* hold_cycles of 0 must be coerced to 1 so the key still has a deadline. */
	altaid_hw_panel_press_key(&hw, 7, 2000ull, 0ull);
	_it_should(
		"coerce zero hold_cycles to 1",
		true == hw.fp_key_down[7]
		&& 2001ull == hw.fp_key_until[7]
	);

	/* Out-of-range key index is ignored; no OOB write. */
	altaid_hw_panel_press_key(&hw, 11, 3000ull, 100ull);
	altaid_hw_panel_press_key(&hw, 255, 3000ull, 100ull);
	_it_should(
		"ignore out-of-range key index",
		true == hw.fp_key_down[3]
		&& true == hw.fp_key_down[7]
	);

	/* Repeated press on same key resets the deadline forward. */
	altaid_hw_panel_press_key(&hw, 3, 5000ull, 200ull);
	_it_should(
		"re-press extends key_until deadline",
		true == hw.fp_key_down[3]
		&& 5200ull == hw.fp_key_until[3]
	);

	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_tick(void)
{
	AltaidHW hw;

	altaid_hw_init(&hw);

	altaid_hw_panel_press_key(&hw, 2, 1000ull, 500ull); /* until 1500 */
	altaid_hw_panel_press_key(&hw, 8, 1000ull, 300ull); /* until 1300 */

	altaid_hw_panel_tick(&hw, 1299ull);
	_it_should(
		"hold keys pressed before their deadline",
		true == hw.fp_key_down[2]
		&& true == hw.fp_key_down[8]
	);

	altaid_hw_panel_tick(&hw, 1300ull);
	_it_should(
		"release key at exactly its deadline",
		true == hw.fp_key_down[2]
		&& false == hw.fp_key_down[8]
	);

	altaid_hw_panel_tick(&hw, 1500ull);
	_it_should(
		"release remaining key at its deadline",
		false == hw.fp_key_down[2]
		&& false == hw.fp_key_down[8]
	);

	/* Idempotent: tick past deadlines again without changing state. */
	altaid_hw_panel_tick(&hw, 9999ull);
	_it_should(
		"tick is idempotent once all keys are released",
		false == hw.fp_key_down[2]
		&& false == hw.fp_key_down[8]
	);

	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_addr16(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_data8(void)
{
	return NULL;
}

static char *test_altaid_hw_altaid_hw_panel_stat4(void)
{
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_altaid_hw_recompute_ram_bank);
	_run_test(test_altaid_hw_panel_latch_if_complete);
	_run_test(test_altaid_hw_panel_switch_nibble_for_row);
	_run_test(test_altaid_hw_altaid_hw_init);
	_run_test(test_altaid_hw_altaid_hw_reset_runtime);
	_run_test(test_altaid_hw_altaid_hw_load_rom64k);
	_run_test(test_altaid_hw_altaid_mem_read);
	_run_test(test_altaid_hw_altaid_mem_write);
	_run_test(test_altaid_hw_altaid_io_in);
	_run_test(test_altaid_hw_altaid_io_out);
	_run_test(test_altaid_hw_altaid_hw_panel_press_key);
	_run_test(test_altaid_hw_altaid_hw_panel_tick);
	_run_test(test_altaid_hw_altaid_hw_panel_addr16);
	_run_test(test_altaid_hw_altaid_hw_panel_data8);
	_run_test(test_altaid_hw_altaid_hw_panel_stat4);
	return NULL;
}
