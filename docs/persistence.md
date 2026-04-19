# Persistence (State / RAM)

The emulator can save and restore:

- **Machine state**: CPU + devices + RAM + timing (ROM content is **not** saved).
- **RAM**: raw bytes, loadable at any bank/address, or the full 8×64 KiB set
  (512 KiB).

Intended for quick "save state" workflows, debugging, and injecting test
programs into RAM at startup.

## CLI

Three repeatable flags, driven by a single spec grammar:

- `--load <spec>` applies the spec before execution starts.
- `--save <spec>` applies the spec on clean exit.
- `--default <spec>` seeds the path used by Ctrl-P save/load commands.

### Specs

- `state:<file>` — full CPU + devices + RAM snapshot.
- `ram:<file>` — full 512 KiB RAM image.
- `ram@<addr>:<file>` — raw blob, bank 0, starting at `<addr>`.
- `ram@<bank>.<addr>:<file>` — raw blob, specific bank (0..7), at `<addr>`.

`<addr>` and `<bank>` accept decimal or hex (`0x100`).

Partial RAM form is load-only; `--save ram@...` is rejected at CLI parse.
`--save ram:<file>` always writes the full 512 KiB.

### Examples

```sh
# Inject a 28-byte test program at bank 0, address 0x0100:
altaid-emu altaid06.rom --load ram@0x100:tests/bank_get.bin --ui

# Round-trip a full RAM dump:
altaid-emu altaid06.rom --save ram:snap.ram --run-ms 1000
altaid-emu altaid06.rom --load ram:snap.ram

# Pre-load a saved state and also splat a patch at bank 2, 0x2000:
altaid-emu altaid06.rom --load state:boot.state --load ram@2.0x2000:patch.bin
```

Multiple `--load` / `--save` flags may appear.  They are applied in the order
given; later specs overwrite overlapping bytes.

## Ctrl-P commands

All commands below are entered as `Ctrl-P` then the key:

### State

- `s`: save machine state to the current state file
- `l`: load machine state from the current state file
- `f`: set the state filename (interactive prompt)

### RAM

- `b`: save RAM to the current RAM file (always full 512 KiB)
- `g`: load RAM from the current RAM file (honors the default spec's
  bank/addr, so a partial `--default ram@<bank>.<addr>:<file>` makes the
  load target that offset)
- `M`: set the RAM filename (interactive prompt)

## File formats

- **State** files are self-describing (magic `ALTAIDST` + u32 version), so
  incompatible formats are rejected cleanly.
- **RAM** files are raw bytes, no header.  A partial `--load ram@<bank>.<addr>`
  reads the file size from disk and places the bytes starting at that flat
  offset; `--save ram:<file>` always writes exactly 512 KiB.
