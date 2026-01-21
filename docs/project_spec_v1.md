---
title: Altaid 8800 Emulator — v1.0 Aspirational Project Spec (Roadmap)
spec_version: 1.0
last_updated: 2026-01-20
status: target-roadmap
---

# Purpose

## Host platform support

- v1.0 targets **Linux** and **macOS** as supported hosts.
- Windows support is **opportunistic**: accepted when it requires no extra code paths or maintenance.


This document defines the **v1.0 target** for the Altaid 8800 emulator.

- This is a **roadmap / aspirations** spec. Some requirements may be unimplemented.
- The **single source of truth for current behavior** is: `docs/project_spec_current.md`.
- When a v1.0 target is implemented (or intentionally descoped), update `docs/project_spec_current.md` to reflect reality.

**Project philosophy:** keep the project small and shippable. Prefer the minimum number of docs/tools required for a credible emulator and avoid “process scaffolding” that creates maintenance burden.

# Spec governance

V1-GOV-001 (MUST): `docs/project_spec_current.md` MUST remain the authoritative description of shipped behavior.
V1-GOV-002 (MUST): This document MUST NOT be treated as authoritative for current behavior.
V1-GOV-003 (SHOULD): Each v1.0 requirement SHOULD be worded as a user-visible contract and avoid implementation details.
V1-GOV-004 (SHOULD): When a v1.0 requirement is implemented, it SHOULD be copied (or refactored) into the current spec and covered by an acceptance test in `docs/acceptance_tests.md`.

# v1.0 scope

The v1.0 release targets a credible public emulator experience for the Altaid 8800 concept (8080 + panel + memory/I-O board with serial and cassette), with emphasis on:

- Smooth first-run experience
- Repeatable serial and cassette workflows
- A stable, readable UI (text mode and TUI)
- Basic confidence in CPU correctness
- Clear docs and release hygiene

# Build, packaging, and release hygiene

V1-BLD-001 (MUST): The project MUST build cleanly with `-Wall -Wextra -Werror` on GCC and Clang.
V1-BLD-002 (MUST): The repository MUST NOT track build artifacts (e.g., `*.o`, generated binaries).
V1-BLD-003 (MUST): `make clean && make` MUST produce a runnable `./altaid-emu`.
V1-BLD-004 (SHOULD): CI MUST build on Linux and macOS and run at least one runtime smoke test.
V1-BLD-005 (SHOULD): A `make dist` or `tools/dist.sh` workflow SHOULD produce a source release tarball that contains no generated files.

# First-run experience

V1-FRX-001 (MUST): A user MUST be able to verify the emulator runs without requiring proprietary ROMs.
V1-FRX-002 (MUST): The repo MUST provide one of:
- an open "diagnostic ROM stub" (small program) OR
- a test-runner-driven diagnostic path (built as a test binary, not as app runtime code)
that can display a banner, exercise basic CPU execution, and demonstrate panel/serial output.
V1-FRX-003 (SHOULD): CI SHOULD boot the diagnostic path and assert a known transcript line (smoke test).

# ROM handling and legality

V1-ROM-001 (MUST): Documentation MUST state that ROMs are not distributed and users must supply their own.
V1-ROM-002 (MUST): Documentation MUST state the ROM size requirement(s) and expected mapping profile(s).
V1-ROM-003 (SHOULD): Documentation SHOULD include "known-good" ROM fingerprints (hashes) for community verification without distributing ROMs.

# CPU correctness and confidence

V1-CPU-001 (MUST): The emulator MUST implement the full Intel 8080 instruction set.
V1-CPU-002 (MUST): Flags behavior MUST match the 8080 specification.
V1-CPU-003 (SHOULD): The repo SHOULD include an automated CPU regression test (e.g., exerciser binary + expected pass/fail transcript) runnable in CI.
V1-CPU-004 (SHOULD): A `--trace-cpu` or equivalent debug facility SHOULD exist for reporting bugs (at minimum: PC/opcode/logging with optional rate limiting).

# Hardware model and memory map

V1-HW-001 (MUST): The memory map and I/O port map MUST be documented in `docs/`.
V1-HW-002 (MUST): The emulator MUST support the ROM + RAM + bank/overlay behavior required to boot the primary supported ROM profile.
V1-HW-003 (SHOULD): The emulator SHOULD support selectable hardware profiles (e.g., `--hw-profile <name>`) if multiple ROM families exist.
V1-HW-004 (SHOULD): Power-on/reset behavior SHOULD be documented (which latches reset, what RAM contents are, what bank/overlay bits do).

# Front panel UX

V1-PNL-001 (MUST): The emulator MUST provide a usable front-panel experience (LED state display + key input).
V1-PNL-002 (MUST): Key input MUST support momentary presses with a configurable duration.
V1-PNL-003 (SHOULD): The panel display SHOULD be available in both a compact text snapshot mode and an always-on TUI mode.
V1-PNL-004 (SHOULD): The panel SHOULD provide discoverable help describing key mappings.

# TUI rendering and Unicode

V1-TUI-001 (MUST): In full-screen TUI mode, the statusline MUST remain visible at all times.
V1-TUI-002 (MUST): The panel region and serial region MUST NOT overlap for terminals >= 80x25.
V1-TUI-003 (MUST): Help text MUST render into the serial region (no hidden output behind the panel).

V1-TUI-010 (MUST): TUI SHOULD auto-detect UTF-8 capability (locale/terminal) and prefer Unicode box-drawing borders when supported. (Default Unicode borders + `--ascii` fallback are already part of the current behavior contract.)
V1-TUI-011 (SHOULD): A `--unicode` flag SHOULD force Unicode rendering even if detection is inconclusive.
V1-TUI-012 (SHOULD): Other optional Unicode niceties MAY be used when available (e.g., heavier borders for the panel frame, ellipsis glyph, arrows), but MUST have an ASCII fallback under `--ascii`.

# Serial I/O and host integration

V1-SER-001 (MUST): The emulator MUST support interactive serial I/O to the host terminal (non-PTY mode).
V1-SER-002 (MUST): The emulator MUST support PTY mode for integrating with terminal programs.
V1-SER-003 (SHOULD): Documentation SHOULD include a "golden path" for connecting to the PTY and loading software over serial.

V1-SER-010 (SHOULD): The repo SHOULD include a small serial sending utility in `tools/` that supports:
- configurable per-character and per-line pacing
- CR/LF translation modes
- sending to a PTY path
and is documented with examples.

# Cassette

V1-CAS-001 (MUST): The emulator MUST support cassette playback and recording for the supported ROM profile.
V1-CAS-002 (MUST): Detaching a cassette while recording MUST auto-stop and save before detach.
V1-CAS-003 (MUST): The cassette file format MUST be versioned and specified as a stable external contract.
V1-CAS-004 (MUST): Multi-byte cassette file fields MUST use an explicit endianness (little-endian preferred) and MUST NOT depend on host ABI/packing.
V1-CAS-005 (SHOULD): Tools SHOULD exist to inspect or summarize a cassette image (header + duration stats) for debugging.

# Persistence (state + RAM)

V1-SAV-001 (MUST): Full machine state save/load MUST exist.
V1-SAV-002 (MUST): RAM-only save/load MUST exist.
V1-SAV-003 (MUST): Full state load MUST fully reset host scheduling/timing so the restored session resumes consistently.
V1-SAV-004 (MUST): RAM-only load MUST preserve CPU/device state and host scheduling.
V1-SAV-005 (SHOULD): State/RAM file formats SHOULD be versioned, endian-stable, and include ROM identity verification.

# Determinism and replay

V1-DET-001 (MUST): The emulation core MUST be deterministic with respect to its inputs.
V1-DET-002 (SHOULD): A "session capture" mode SHOULD exist that records host serial input bytes with tick timestamps and records cassette attach/play/record actions.
V1-DET-003 (SHOULD): A "session replay" mode SHOULD exist that replays a captured session deterministically to reproduce issues.

# Diagnostics and reporting

V1-DIAG-001 (SHOULD): `--version` SHOULD include build version and key defaults (hz, baud) for bug reports.
V1-DIAG-002 (SHOULD): There SHOULD be an option to dump current machine state summary (registers + bank/overlay bits + tick counters) without exiting.
V1-DIAG-003 (SHOULD): Logging SHOULD be configurable and MUST avoid hot-path overhead unless explicitly enabled.

# Documentation

V1-DOC-001 (MUST): README MUST include:
- build instructions
- the ROM requirement and legality note
- a minimal run command for text mode, TUI mode, and PTY mode
- troubleshooting for "no output" and terminal sizing

V1-DOC-002 (SHOULD): README SHOULD include screenshots or an animated GIF of the panel/TUI.
V1-DOC-003 (SHOULD): `docs/` SHOULD include a compatibility envelope describing what is and is not modeled.
V1-DOC-004 (SHOULD): `docs/` SHOULD include the serial and cassette workflows as step-by-step recipes.
