# Altaid 8800 (ALTAID05) Port Map

This emulator models the I/O behavior expected by the **ALTAID05** ROM family.

## Conventions

- Front-panel switch nibble is **active-low** (pressed key reads as 0).
- Serial TX/RX and cassette input idle **high**.

## IN 0x40 — INPUT_PORT

Returned bits (1 = high):

- **b0..b3**: switch column nibble for the currently selected scan row (active-low)
- **b5**: TIMER input level (idle high)
- **b6**: CASSETTE input level (digital level from jack comparator)
- **b7**: RXDATA (bit-serial UART input)

All other bits read as 1.

### Switch matrix rows

The ROM selects a “scan row” using `OUT 0xC0` bits 4..6. When the ROM reads `IN 0x40`, bits 0..3 return the switch nibble for that row.

- **Row 4**: DATA keys D0..D3 map to bits 0..3
- **Row 5**: DATA keys D4..D7 map to bits 0..3
- **Row 6**: RUN/MODE/NEXT map to bits 0..2

## OUT 0xC0 — OUTPUT_PORT

Written bits:

- **b0..b3**: LED column nibble for the currently selected row
- **b4..b6**: scan row select (0..7)
- **b7**: TXDATA (bit-serial UART output)

Behavior:

- When a value is written, the emulator stores the low nibble into `led_row_nibble[scan_row]` (for scan rows 0..6).
- The current scan row is used both for the LED matrix and for determining which switch row is returned on the next `IN 0x40` read.

## Banking and control latches (OUT)

These are modeled as simple 1-bit (bit0) latches:

- `OUT 0x41` **ROM_LOW**: write **0** maps ROM into `0x0000–0x7FFF`; write nonzero maps RAM
- `OUT 0x40` **ROM_HI**: write nonzero maps ROM into `0x8000–0xBFFF`; write 0 maps RAM
- `OUT 0x45` **B15**: ROM “half” select (bit0)
- `OUT 0x42` **B16**, `OUT 0x47` **B17**, `OUT 0x43` **B18**: RAM bank bits (bit0) selecting 1 of 8 x 64 KiB banks
- `OUT 0x46` **TIMER**: enables timer source (bit0)
- `OUT 0x44` **CASSETTE**: cassette output latch (bit0)

See also: `docs/memory-map.md` and `include/altaid_hw.h`.
