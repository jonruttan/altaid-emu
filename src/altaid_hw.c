#include "altaid_hw.h"
#include <stdio.h>
#include <string.h>

static inline AltaidHW* HW(I8080Bus* bus) { return (AltaidHW*)bus->user; }

static void recompute_ram_bank(AltaidHW* hw)
{
	hw->ram_bank = (uint8_t)((hw->ram_a18 ? 4 : 0) | (hw->ram_a17 ? 2 : 0) | (hw->ram_a16 ? 1 : 0));
}

static void panel_latch_if_complete(AltaidHW *hw)
{
	uint16_t a;
	uint8_t d;
	uint8_t s;

	if ((hw->led_row_mask & 0x7Fu) != 0x7Fu)
		return;

	/* Rows: 0=A11..A8, 1=A15..A12, 2=A3..A0, 3=A7..A4 */
	a = 0;
	a |= (uint16_t)((hw->led_row_nibble[1] & 0x0Fu) << 12);
	a |= (uint16_t)((hw->led_row_nibble[0] & 0x0Fu) << 8);
	a |= (uint16_t)((hw->led_row_nibble[3] & 0x0Fu) << 4);
	a |= (uint16_t)((hw->led_row_nibble[2] & 0x0Fu) << 0);

	/* Rows: 4=D3..D0, 5=D7..D4 */
	d = (uint8_t)(((hw->led_row_nibble[5] & 0x0Fu) << 4) |
		     (hw->led_row_nibble[4] & 0x0Fu));

	/* Row 6: status nibble */
	s = (uint8_t)(hw->led_row_nibble[6] & 0x0Fu);

	hw->panel_latched_addr = a;
	hw->panel_latched_data = d;
	hw->panel_latched_stat = s;
	hw->panel_latched_valid = true;
	hw->panel_latched_seq++;
	hw->led_row_mask = 0;
}

/*
* Switch matrix is active-low with pull-ups. The ROM treats keys as momentary.
*
* Mapping derived from altaid05.asm debounce logic:
*   row 4: bits0..3 => D0..D3
*   row 5: bits0..3 => D4..D7
*   row 6: bit0=RUN, bit1=MODE, bit2=NEXT, bit3=unused (1)
*/
static uint8_t panel_switch_nibble_for_row(const AltaidHW* hw, uint8_t row)
{
	uint8_t nib = 0x0F;

	if (row == 4) {
		for (int i=0;i<4;i++) {
			if (hw->fp_key_down[i]) nib = (uint8_t)(nib & (uint8_t)~(1u<<i));
		}
	} else if (row == 5) {
		for (int i=0;i<4;i++) {
			if (hw->fp_key_down[4+i]) nib = (uint8_t)(nib & (uint8_t)~(1u<<i));
		}
	} else if (row == 6) {
		if (hw->fp_key_down[8])  nib = (uint8_t)(nib & (uint8_t)~1u);
		if (hw->fp_key_down[9])  nib = (uint8_t)(nib & (uint8_t)~2u);
		if (hw->fp_key_down[10]) nib = (uint8_t)(nib & (uint8_t)~4u);
	}

	return nib;
}

void altaid_hw_init(AltaidHW* hw)
{
	memset(hw, 0, sizeof(*hw));

	/* power-on defaults (altaid05.asm): output latches cleared ->
	* ROM_LOW enabled (active-low), ROM_HI disabled, ROM bank 0, RAM bank 0
	*/
	hw->rom_half = 0;
	hw->ram_a16 = hw->ram_a17 = hw->ram_a18 = 0;
	recompute_ram_bank(hw);

	hw->rom_low_mapped = true;  /* latch starts at 0 => enable ROM at 0000 */
	hw->rom_hi_mapped = false;

	hw->out_c0 = 0;
	hw->tx_line = true;         /* idle high */
	hw->rx_level = true;

	hw->timer_en = false;
	hw->timer_level = true;

	hw->cassette_out_level = false;
	hw->cassette_out_dirty = false;
	hw->cassette_in_level = true; /* idle high */

	hw->scan_row = 0;
	for (int i=0;i<7;i++)
		hw->led_row_nibble[i] = 0;
	hw->led_row_mask = 0;
	hw->panel_latched_valid = false;
	hw->panel_latched_seq = 0;
	hw->panel_latched_addr = 0;
	hw->panel_latched_data = 0;
	hw->panel_latched_stat = 0;

	for (int i=0;i<11;i++) {
		hw->fp_key_down[i] = false;
		hw->fp_key_until[i] = 0;
	}
}

void altaid_hw_reset_runtime(AltaidHW* hw)
{
	/*
	* Reset runtime state to power-on defaults while preserving ROM and RAM.
	*
	* This models a hardware RESET that reinitializes latches and I/O state,
	* but does not clear SRAM contents.
	*/
	hw->rom_half = 0;
	hw->ram_a16 = hw->ram_a17 = hw->ram_a18 = 0;
	recompute_ram_bank(hw);

	hw->rom_low_mapped = true;
	hw->rom_hi_mapped = false;

	hw->out_c0 = 0;
	hw->tx_line = true;  /* idle high */
	hw->rx_level = true;

	hw->timer_en = false;
	hw->timer_level = true;

	hw->cassette_out_level = false;
	hw->cassette_out_dirty = false;
	hw->cassette_in_level = true;

	hw->scan_row = 0;
	for (int i=0;i<7;i++)
		hw->led_row_nibble[i] = 0;
	hw->led_row_mask = 0;
	hw->panel_latched_valid = false;
	hw->panel_latched_seq = 0;
	hw->panel_latched_addr = 0;
	hw->panel_latched_data = 0;
	hw->panel_latched_stat = 0;

	for (int i=0;i<11;i++) {
		hw->fp_key_down[i] = false;
		hw->fp_key_until[i] = 0;
	}
}

bool altaid_hw_load_rom64k(AltaidHW* hw, const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Failed to open ROM: %s\n", path);
		return false;
	}
	uint8_t buf[0x10000];
	size_t n = fread(buf, 1, sizeof(buf), f);
	fclose(f);
	if (n != sizeof(buf)) {
		fprintf(stderr, "ROM must be exactly 64K (got %zu bytes)\n", n);
		return false;
	}
	memcpy(hw->rom[0], buf + 0x0000, 0x8000);
	memcpy(hw->rom[1], buf + 0x8000, 0x8000);
	return true;
}

uint8_t altaid_mem_read(I8080Bus* bus, uint16_t addr)
{
	AltaidHW* hw = HW(bus);

	/* Low 32K: optionally ROM */
	if (addr < 0x8000) {
		if (hw->rom_low_mapped) return hw->rom[hw->rom_half][addr];
		return hw->ram[hw->ram_bank][addr];
	}

	/* Upper 32K: ROM_HI maps only first 16K (0x8000-0xBFFF) */
	if (addr < 0xC000) {
		if (hw->rom_hi_mapped) return hw->rom[hw->rom_half][(uint16_t)(addr - 0x8000)];
		return hw->ram[hw->ram_bank][addr];
	}

	/* Top 16K always RAM */
	return hw->ram[hw->ram_bank][addr];
}

void altaid_mem_write(I8080Bus* bus, uint16_t addr, uint8_t v)
{
	AltaidHW* hw = HW(bus);

	/* Shadow ROM: writes always go to RAM, even if reads are coming from ROM. */
	hw->ram[hw->ram_bank][addr] = v;
}

uint8_t altaid_io_in(I8080Bus* bus, uint8_t port)
{
	AltaidHW* hw = HW(bus);

	/* INPUT_PORT shares address with ROM_HI latch; only reads come here. */
	if (port == ALTAID_PORT_INPUT) {
		uint8_t v = 0xFF;

		/* switch columns for current scan row */
		uint8_t row = (uint8_t)(hw->scan_row & 7u);
		uint8_t sw = panel_switch_nibble_for_row(hw, row);
		v = (uint8_t)((v & 0xF0u) | (sw & 0x0Fu));

		/* bit5: timer */
		if (hw->timer_level) v |= 0x20u; else v &= (uint8_t)~0x20u;

		/* bit6: cassette input */
		if (hw->cassette_in_level) v |= 0x40u; else v &= (uint8_t)~0x40u;

		/* bit7: RX */
		if (hw->rx_level) v |= 0x80u; else v &= (uint8_t)~0x80u;

		return v;
	}

	return 0xFF;
}

void altaid_io_out(I8080Bus* bus, uint8_t port, uint8_t v)
{
	AltaidHW* hw = HW(bus);

	switch (port) {
	case ALTAID_PORT_OUTPUT: {
			hw->out_c0 = v;

			/* TXDATA is bit7 */
			hw->tx_line = ((v & 0x80u) != 0);

			/* scan_row is OUT4..OUT6 (bits4..6) */
			hw->scan_row = (uint8_t)((v >> 4) & 7u);

			/* latch LED nibble for the selected row */
			if (hw->scan_row < 7u) {
				hw->led_row_nibble[hw->scan_row] = (uint8_t)(v & 0x0Fu);
				hw->led_row_mask |= (uint8_t)(1u << (hw->scan_row & 7u));
				panel_latch_if_complete(hw);
			}
		} break;

	case ALTAID_PORT_ROM_HI:
		hw->rom_hi_mapped = (v != 0);
		break;

	case ALTAID_PORT_ROM_LOW:
		hw->rom_low_mapped = (v == 0);
		break;

	case ALTAID_PORT_B15:
		hw->rom_half = (uint8_t)(v & 1u);
		break;

	case ALTAID_PORT_B16:
		hw->ram_a16 = (uint8_t)(v & 1u);
		recompute_ram_bank(hw);
		break;

	case ALTAID_PORT_B17:
		hw->ram_a17 = (uint8_t)(v & 1u);
		recompute_ram_bank(hw);
		break;

	case ALTAID_PORT_B18:
		hw->ram_a18 = (uint8_t)(v & 1u);
		recompute_ram_bank(hw);
		break;

	case ALTAID_PORT_TIMER:
		hw->timer_en = ((v & 1u) != 0);
		break;

	case ALTAID_PORT_CASSETTE:
		/* cassette out latch: ROM writes 0/1 (bit0) */
		{
			bool new_level = ((v & 1u) != 0);
			if (new_level != hw->cassette_out_level) {
				hw->cassette_out_level = new_level;
				hw->cassette_out_dirty = true;
			}
		}
		break;

	default:
		/* ignore unknown ports */
		break;
	}
}

void altaid_hw_panel_press_key(AltaidHW* hw, uint8_t key_index, uint64_t now_tick, uint64_t hold_cycles)
{
	if (key_index >= 11u) return;
	if (hold_cycles == 0) hold_cycles = 1;

	hw->fp_key_down[key_index] = true;
	hw->fp_key_until[key_index] = now_tick + hold_cycles;
}

void altaid_hw_panel_tick(AltaidHW* hw, uint64_t now_tick)
{
	for (int i=0;i<11;i++) {
		if (hw->fp_key_down[i] && now_tick >= hw->fp_key_until[i]) {
			hw->fp_key_down[i] = false;
		}
	}
}

uint16_t altaid_hw_panel_addr16(const AltaidHW* hw)
{
	uint16_t a;

	if (hw->panel_latched_valid)
		return hw->panel_latched_addr;

	/* Fallback: decode current nibble rows (may be mid-scan). */
	a = 0;
	a |= (uint16_t)((hw->led_row_nibble[1] & 0x0Fu) << 12);
	a |= (uint16_t)((hw->led_row_nibble[0] & 0x0Fu) << 8);
	a |= (uint16_t)((hw->led_row_nibble[3] & 0x0Fu) << 4);
	a |= (uint16_t)((hw->led_row_nibble[2] & 0x0Fu) << 0);
	return a;
}

uint8_t altaid_hw_panel_data8(const AltaidHW* hw)
{
	if (hw->panel_latched_valid)
		return hw->panel_latched_data;
	return (uint8_t)(((hw->led_row_nibble[5] & 0x0Fu) << 4) |
			 (hw->led_row_nibble[4] & 0x0Fu));
}

uint8_t altaid_hw_panel_stat4(const AltaidHW* hw)
{
	if (hw->panel_latched_valid)
		return hw->panel_latched_stat;
	return (uint8_t)(hw->led_row_nibble[6] & 0x0Fu);
}
