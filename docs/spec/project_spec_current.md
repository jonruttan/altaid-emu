---
title: Altaid 8800 Emulator — Current Project Spec (Authoritative)
spec_version: 0.1
last_updated: 2026-01-20
status: living-document
---

# Purpose

This document is the **single source of truth** for what this repository’s emulator and tooling **currently** do and promise to users today.

- The **v1.0 aspirations** live in `docs/spec/project_spec_v1.md` (roadmap/target behavior).
- When the repo changes behavior, update **this** document first, then adjust any relevant docs/README snippets.
- Verification lives in `tests/unit/*.spec.c` (preferred), `tests/e2e/*.spec.c`,
  and/or the README “golden path” examples.

- **Normative language**: “MUST/SHOULD/MAY” is intentional.
- **Scope**: emulator behavior + CLI/TUI/panel + cassette + persistence + build/release expectations.
- **Non-goals**: This is not a design doc; it is a behavior contract.

**Project philosophy:** keep the project small and shippable.

**Maintenance workflow:** see `docs/spec/workflow.md` for the slice checklist and update rules. Avoid adding new tools/docs unless they directly reduce user confusion, prevent regressions, or simplify contribution.
**Commit messages:** follow `docs/spec/commit-guidelines.md` when preparing commits.

# Product overview

`altaid-emu` is a userland emulator for the **Altaid 8800** (Intel 8080-class machine).

The emulator has two user interaction modes:

1. **Text mode** (default): serial I/O is regular terminal text; optional **panel snapshots** are printed as text.
2. **Full-screen TUI mode** (`--ui`): split screen with a toggleable **panel** (top) and a **serial terminal** (bottom) plus a **statusline**.

# Build and runtime invariants

## Build

- The project MUST build clean with `-Wall -Wextra -Werror` on GCC and Clang where available.
- The default build MUST be freestanding from non-system dependencies (no third-party libs).
- `make clean && make` MUST produce `./altaid-emu`.

## Host platform support

- The project MUST support **Linux** and **macOS** as first-class host environments.
- The project MAY work on Windows environments, but it MUST NOT require Windows-specific code or special-case behavior to be considered “complete”.
  - If Windows support is desired, the preferred approach is running under a Unix-like compatibility layer rather than adding Windows-only paths.


## Determinism and fidelity

- The emulation core MUST be deterministic with respect to its inputs (ROM, serial input stream, cassette image, initial state/RAM image).
- The emulation core MUST NOT directly depend on host wall-clock time, host scheduling, or terminal rendering.
- Host-side “real-time pacing” MAY be enabled, but it MUST be optional and MUST NOT alter the internal tick-based emulation state.

# CLI contract

## Required positional arguments

- `altaid-emu ROMFILE`
  - `ROMFILE` MUST point to a 64KiB ROM image.

## Common options

These options MUST exist and MUST remain stable unless explicitly deprecated in `docs/`:

- `--ui` : enable full-screen TUI mode **only if** output is an interactive TTY.
- `--panel` : start with panel visible (TUI) / enable text snapshots at startup (text mode).
- `--pty` : run in PTY mode (serial I/O connected to a PTY; local terminal becomes view/control UI).
- `--pty-input` : in PTY mode, allow local keyboard input to also feed the emulated serial input.

## Terminal sizing

- TUI mode MUST size itself from the best available terminal size probe.
- If the probe fails, default MUST be `80x25`.
- `--term-rows` and `--term-cols` MUST override the probed size and MUST take precedence.

## Persistence (state + RAM)

### Default filenames

- `--state-file <path>` sets the default file used by Ctrl-P save/load commands.
- `--ram-file <path>` sets the default RAM file used by Ctrl-P save/load commands.

### Startup/exit automation

- `--state-load <path>` loads machine state before starting execution.
- `--state-save <path>` saves machine state on clean exit.
- `--ram-load <path>` loads RAM banks before starting execution.
- `--ram-save <path>` saves RAM banks on clean exit.

### Semantics

- “Machine state” MUST include CPU registers/flags, RAM banks, relevant device state (serial RX/TX state, cassette transport state if attached), and emulator tick counters required to resume deterministically.
- “RAM only” MUST include all emulated RAM banks and MUST NOT overwrite ROM.

## Cassette

- `--cass <path>` attaches a cassette image.
- `--cass-play` starts playback at tick 0.
- `--cass-rec` starts recording at tick 0.

# Runtime controls (Ctrl-P commands)

The emulator provides a “panel prefix” key chord. When the prefix is seen, the next key is interpreted as a command.

## Panel and UI

- `Ctrl-P p` : toggle panel visibility (upper section)
- `Ctrl-P u` : toggle full-screen UI mode (if interactive TTY)
- `Ctrl-P i` : toggle serial input read-only
- `Ctrl-P t` : toggle PTY local keyboard input (PTY mode)
- `Ctrl-P c` : toggle text panel compact/verbose (text mode)
- `Ctrl-P d` : emit a one-shot panel snapshot (text mode)
- `Ctrl-P h` or `Ctrl-P ?` : show help
- `Ctrl-P Ctrl-R` : reset emulated machine
- `Ctrl-P q` : quit emulator

## Persistence

- `Ctrl-P s` : save full machine state to current state filename
- `Ctrl-P l` : load full machine state from current state filename
- `Ctrl-P f` : prompt to set state filename

- `Ctrl-P b` : save RAM banks to current RAM filename
- `Ctrl-P g` : load RAM banks from current RAM filename
- `Ctrl-P M` : prompt to set RAM filename

## Cassette transport

- `Ctrl-P a` : prompt to set/attach cassette filename
- `Ctrl-P P` : play
- `Ctrl-P R` : record
- `Ctrl-P K` : stop
- `Ctrl-P W` : rewind
- `Ctrl-P J` : fast-forward 10s
- `Ctrl-P V` : save tape image now

# TUI behavior contract

- When TUI mode is active:
  - The **statusline MUST always be visible**.
  - The panel and serial areas MUST NOT overlap.
  - Help text MUST render into the serial area (not “behind” it).
  - All output in TUI mode MUST be routed through the TUI renderer (no direct stdout/stderr writes that could corrupt the screen).
  - By default, the panel frame SHOULD use Unicode box-drawing characters (e.g., ┌─┐ │ └─┘) when rendering in a UTF-8 capable terminal.
  - `--ascii` MUST force ASCII-safe rendering for all TUI glyphs, including panel borders.

- If `--pty` is active:
  - The serial area MUST be read-only by default.
  - `--pty-input` MAY enable local input injection into the emulated serial input.

# Text mode panel behavior contract

- Text mode panel output MUST be line-delimited and MUST NOT share a line with serial output.
- Default behavior SHOULD be:
  - print a panel snapshot at startup (if `--panel`)
  - print a panel snapshot after each “serial burst” (as defined by a short idle gap)
- An optional “print on change” mode MAY be provided.

# Acceptance tests (manual)

These are *user-facing* checks that SHOULD pass on every release candidate:

1. `./altaid-emu ROM --panel` shows a panel snapshot, then prints ROM serial output, then prints updated snapshots at prompts.
2. `./altaid-emu ROM --ui` opens a stable TUI with statusline always visible.
3. `--term-rows/--term-cols` overrides probed sizing in TUI.
4. Ctrl-P help displays fully in TUI and text mode.
5. Ctrl-P tape commands work with an attached cassette image.
6. State save/load round-trips deterministically (load -> same behavior, including tick counters if shown).
7. RAM save/load round-trips (RAM changes visible after load).
8. PTY mode toggles local input with `--pty-input` or Ctrl-P t.
