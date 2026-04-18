# Fidelity Notes / Known Limitations

This emulator is designed to run ALTAID05-style ROMs and software that expect an **Altair-ish** 8080 environment with bit-banged serial and a multiplexed front panel.

## Where fidelity matters most

- **8080 instruction timing (t-states):** the ROM bit-bangs a UART. Small timing errors can break serial.
- **EI behavior:** EI takes effect after the next instruction (8080 quirk).
- **I/O port behavior:** especially `IN 0x40` and `OUT 0xC0` multiplexing.
- **Interrupt timing:** this emulator optionally vectors an interrupt on RX start-bit detection (see below).

## RX interrupt model

The ALTAID05 ROM family uses interrupt-driven serial. In this emulator:

- When the serial decoder detects the **start bit** (TX falling edge -> low), it sets a pending RX interrupt.
- If `INTE` is set, the main loop services it as `RST 7`.

If you want to run software that expects a different interrupt policy, this should be made configurable.

## Front-panel key hold time

The physical panel uses a momentary switch matrix that the ROM samples through
its multiplexed row scan (`OUT 0xC0` row select + `IN 0x40` column read). The
ROM debounces with a per-key counter that advances **only on the iteration of
`FP_MAT_SCAN` whose `FP_SW_PTR` currently points at that key**. Since
`FP_SW_PTR` cycles through all 11 keys, a given key is sampled for debounce
once every 11 `FP_MAT_SCAN` calls. The flag fires when that counter first
reaches 8 (`CPI 08H` at `FPMS_DOWN`), meaning the key must be held continuously
for at least `8 * 11 = 88` `FP_MAT_SCAN` iterations.

In measured runs a `FP_MAT_SCAN` cycle takes roughly 2 ms on a 2 MHz CPU, so
the hard threshold is **≈ 180 ms**. When the ROM is busy with serial I/O the
scan slows further, widening the window needed for reliable debounce.

The emulator holds a pressed key asserted for `--hold <ms>` (default
**300 ms**) and auto-releases on the next CPU tick past the deadline. The
default provides headroom above the 180 ms threshold so presses register
reliably even while the ROM is servicing RX/TX interrupts.

Do **not** shorten `--hold` below ~200 ms without checking: at exactly the
boundary (e.g. 150 ms, our previous default), presses become intermittent
because alignment of the press against the `FP_SW_PTR` cycle determines
whether the counter reaches 8.

## Cassette

Cassette is modeled as a **digital level stream**. There is no attempt to decode real audio.

## Not implemented

- S-100 bus devices / disk controllers
- CP/M-style BIOS/BDOS device set
- Precise analog effects (cassette audio, line noise, etc.)

See also: `docs/port-map.md`, `docs/memory-map.md`, `docs/cassette.md`.
