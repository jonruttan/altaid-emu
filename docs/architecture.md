# Architecture (non-normative)

This is a brief contributor-oriented overview to help you find your way around
the code. It is **not** a behavior contract. If a slice changes module
boundaries or responsibilities, update this document in the same slice.

This emulator is intentionally split into **three layers**.

## 1) EmuCore (deterministic)

Files:
- `include/emu_core.h`
- `src/emu_core.c`

Responsibilities:
- Intel 8080 CPU (`i8080.c`)
- Altaid memory/I/O model (`altaid_hw.c`)
- Bit-level serial encode/decode (`serial.c`)
- Cassette digital-level model (`cassette.c`)
- Tick-based timing (t-states / `ser.tick`)

Rules:
- **No host I/O** (no stdio, PTYs, termios, `select()`, wall-clock time).
- Produces decoded TX bytes into a ring buffer (`emu_core_tx_pop()`).

This layer is designed to be unit-tested.

## 2) EmuHost (integration)

Files:
- `include/emu_host.h`
- `src/emu_host.c`
- `src/runloop.c`

Responsibilities:
- CLI config handling (validated `struct Config`)
- PTY setup and polling
- Serial routing to stdout/stderr/files
- UI lifecycle and rendering (text snapshots or full-screen UI)
- Real-time throttling (optional)

EmuHost calls into EmuCore in batches (`emu_core_run_batch()`), then drains TX bytes
and renders/polls input outside the instruction hot path.

## 3) UI / Presentation

Files:
- `src/ui.c`, `src/panel_ansi.c`, `src/panel_text.c`

Responsibilities:
- Keyboard mapping (Ctrl-P prefix actions)
- Rendering front panel state
- Optional full-screen UI mode (`--ui`)

UI code should not reach into host/OS primitives directly; it operates on
`SerialDev`, `AltaidHW`, and internal UI state.
