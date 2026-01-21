# Altaid 8800 (ALTAID05) Memory Map

The emulator provides:

- **512 KiB RAM** as 8 banks × 64 KiB (`ram_bank` 0..7)
- A single **64 KiB ROM image** split into two **32 KiB halves** (`rom_half` 0..1)
- ROM overlay behavior that matches the ALTAID05 family: **writes always go to RAM** (shadow RAM under ROM), reads may come from ROM depending on latch state.

## Address space (16-bit)

All addresses are in the selected RAM bank unless ROM mapping is enabled.

### 0x0000–0x7FFF (lower 32 KiB)

- If **ROM_LOW** is enabled: reads come from ROM (`rom_half`) at `0x0000–0x7FFF`.
- If ROM_LOW is disabled: reads come from RAM.
- **Writes always go to RAM**.

### 0x8000–0xBFFF (next 16 KiB)

- If **ROM_HI** is enabled: reads come from ROM (`rom_half`) at offset `(addr - 0x8000)`.
- If ROM_HI is disabled: reads come from RAM.
- **Writes always go to RAM**.

> Note: ROM_HI maps only the first 16 KiB of the upper 32 KiB region. This matches the board/ROM behavior modeled here.

### 0xC000–0xFFFF (top 16 KiB)

Always RAM.

## Control latches

- `OUT 0x41` ROM_LOW: write 0 enables ROM mapping at 0x0000–0x7FFF
- `OUT 0x40` ROM_HI: write nonzero enables ROM mapping at 0x8000–0xBFFF
- `OUT 0x45` B15: selects ROM half 0 or 1
- `OUT 0x42/0x47/0x43` B16/B17/B18: select RAM bank 0..7

Implementation: `src/altaid_hw.c`.
