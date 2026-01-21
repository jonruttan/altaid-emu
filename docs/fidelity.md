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

## Cassette

Cassette is modeled as a **digital level stream**. There is no attempt to decode real audio.

## Not implemented

- S-100 bus devices / disk controllers
- CP/M-style BIOS/BDOS device set
- Precise analog effects (cassette audio, line noise, etc.)

See also: `docs/port-map.md`, `docs/memory-map.md`, `docs/cassette.md`.
