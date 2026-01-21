# Persistence (State / RAM)

The emulator can save and restore:

- **Machine state**: CPU + devices + RAM + timing (ROM content is **not** saved).
- **RAM banks**: the full 8×64 KiB RAM set (512 KiB).

These are intended for quick "save state" workflows and for debugging.

## CLI

- `--state-file <file>` sets the default state filename used by Ctrl-P commands.
- `--state-load <file>` loads a full machine state at startup.
- `--state-save <file>` saves a full machine state on exit.

- `--ram-file <file>` sets the default RAM filename used by Ctrl-P commands.
- `--ram-load <file>` loads RAM banks at startup.
- `--ram-save <file>` saves RAM banks on exit.

## Ctrl-P commands

All commands below are entered as `Ctrl-P` then the key:

### State

- `s`: save machine state to the current state file
- `l`: load machine state from the current state file
- `f`: set the state filename (interactive prompt)

### RAM

- `b`: save RAM banks to the current RAM file
- `g`: load RAM banks from the current RAM file
- `M`: set the RAM filename (interactive prompt)

## Compatibility checks

State/RAM files include a small header with CPU frequency, baud rate, and a hash of the loaded ROM image.

- A state/RAM file will be rejected if it does not match the current ROM or configuration.
