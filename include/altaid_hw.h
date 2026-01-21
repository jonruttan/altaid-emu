/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_ALTAID_HW_H
#define ALTAID_EMU_ALTAID_HW_H

#include <stdint.h>
#include <stdbool.h>

#include "i8080.h"

/* Altaid 8800 port map (ALTAID05 family)
*
* OUT 0xC0 OUTPUT_PORT: multiplexed front panel + TXDATA
*   bits 0..3: OUT0..OUT3 (panel column nibble)
*   bits 4..6: OUT4..OUT6 (panel row select)
*   bit 7    : TXDATA (bit-serial UART) -- idle high
*
* IN  0x40 INPUT_PORT:
*   bits 0..3: IN0..IN3 (panel switch column nibble, active-low)
*   bit 5    : TIMER_IN (active-low pulse)
*   bit 6    : CASSETTE_IN (digital level from audio jack comparator)
*   bit 7    : RXDATA (bit-serial UART)
*
* Banking / control latches (OUT):
*   0x40 ROM_HI   : write nonzero => map 16K ROM into 0x8000-0xBFFF; write zero => RAM
*   0x41 ROM_LOW  : write zero => map ROM into 0x0000-0x7FFF; write nonzero => RAM
*   0x45 B15      : ROM bank half select (bit0)
*   0x42 B16, 0x47 B17, 0x43 B18: RAM bank bits (bit0) -> select 1 of 8 x 64K blocks
*   0x46 TIMER    : enable timer source (bit0)
*   0x44 CASSETTE : cassette output latch (bit0)
*/

enum {
	ALTAID_PORT_INPUT      = 0x40,
	ALTAID_PORT_ROM_HI     = 0x40,
	ALTAID_PORT_ROM_LOW    = 0x41,
	ALTAID_PORT_B16        = 0x42,
	ALTAID_PORT_B18        = 0x43,
	ALTAID_PORT_CASSETTE   = 0x44,
	ALTAID_PORT_B15        = 0x45,
	ALTAID_PORT_TIMER      = 0x46,
	ALTAID_PORT_B17        = 0x47,

	ALTAID_PORT_OUTPUT     = 0xC0,
};

typedef struct {
	/* memory */
	uint8_t rom[2][0x8000];          /* 64K ROM image split into 2 x 32K halves */
	uint8_t ram[8][0x10000];         /* 512K RAM as 8 x 64K banks */

	/* RAM bank select (A16..A18) */
	uint8_t ram_a16, ram_a17, ram_a18;
	uint8_t ram_bank;                /* 0..7 */

	/* ROM controls */
	uint8_t rom_half;                /* 0..1 (B15) */
	bool    rom_low_mapped;           /* true => reads from ROM at 0x0000-0x7FFF */
	bool    rom_hi_mapped;            /* true => reads from ROM at 0x8000-0xBFFF */

	/* last output port value */
	uint8_t out_c0;

	/* bit-serial lines */
	bool tx_line;                     /* derived from out_c0 bit7 */
	bool rx_level;                    /* driven by SerialDev */

	/* timer input */
	bool timer_en;
	bool timer_level;                 /* 1 idle, 0 pulse */

	/* cassette I/O (digital line model)
	*
	* In ALTAID05 the ROM drives the cassette output by writing 0/1 to
	* OUT 0x44 (CASSETTE). The MIO schematic routes that latch output to the
	* audio I/O jack circuitry.
	*
	* The cassette input is a digital level on INPUT_PORT bit 6.
	*/
	bool cassette_out_level;           /* driven by OUT 0x44 (bit0) */
	bool cassette_out_dirty;           /* set when OUT 0x44 written */
	bool cassette_in_level;            /* sampled into IN 0x40 bit6 */

	/* --- multiplexed front panel model ---
	*
	* The panel is a 7-row x 4-column LED matrix and a 3-row x 4-column
	* switch matrix sharing the same row select (OUT4..OUT6).
	*
	* Row indices (matching FP_LED_MAT order in the ALTAID05 ROM):
	*   0: A11..A8
	*   1: A15..A12
	*   2: A3..A0
	*   3: A7..A4
	*   4: D3..D0
	*   5: D7..D4
	*   6: status nibble (ALO, AHI, DATA, RUN)
	*/
	uint8_t scan_row;                 /* 0..7 from OUT4..OUT6 */
	uint8_t led_row_nibble[7];        /* last-driven nibble for each LED row */
	uint8_t led_row_mask;             /* rows updated since last latch */

	/*
	 * Latched, stable decoded panel state.
	 *
	 * The ROM multiplexes LED rows; sampling led_row_nibble[] mid-scan can
	 * produce mixed-nibble transient values. We latch a decoded snapshot once
	 * we've observed all 7 rows updated, so UI/panel reads are stable.
	 */
	bool     panel_latched_valid;
	uint32_t panel_latched_seq;       /* increments per complete latch */
	uint16_t panel_latched_addr;
	uint8_t  panel_latched_data;
	uint8_t  panel_latched_stat;

	/* key state for the 11 front-panel keys used by the ROM
	*   0..7: DATA keys D0..D7
	*   8: RUN
	*   9: MODE
	*   10: NEXT
	*/
	bool     fp_key_down[11];
	uint64_t fp_key_until[11];        /* auto-release time in CPU ticks */

} AltaidHW;

/* init/load */
void altaid_hw_init(AltaidHW* hw);
/* Reset CPU-visible hardware state to power-on defaults, preserving ROM and RAM contents. */
void altaid_hw_reset_runtime(AltaidHW* hw);
bool altaid_hw_load_rom64k(AltaidHW* hw, const char *path);

/* 8080 bus handlers */
uint8_t altaid_mem_read(I8080Bus* bus, uint16_t addr);
void    altaid_mem_write(I8080Bus* bus, uint16_t addr, uint8_t v);
uint8_t altaid_io_in(I8080Bus* bus, uint8_t port);
void    altaid_io_out(I8080Bus* bus, uint8_t port, uint8_t v);

/* helpers */
static inline uint8_t altaid_hw_tx_level(const AltaidHW* hw) { return (uint8_t)(hw->tx_line ? 1 : 0); }

/* front-panel helpers (decoded from led_row_nibble[]) */
uint16_t altaid_hw_panel_addr16(const AltaidHW* hw);
uint8_t  altaid_hw_panel_data8(const AltaidHW* hw);
uint8_t  altaid_hw_panel_stat4(const AltaidHW* hw);

/* front-panel switch helpers */
void altaid_hw_panel_press_key(AltaidHW* hw, uint8_t key_index, uint64_t now_tick, uint64_t hold_cycles);
void altaid_hw_panel_tick(AltaidHW* hw, uint64_t now_tick);


#endif /* ALTAID_EMU_ALTAID_HW_H */
