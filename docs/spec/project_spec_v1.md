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
- The **single source of truth for current behavior** is: `docs/spec/project_spec_current.md`.
- When a v1.0 target is implemented (or intentionally descoped), update `docs/spec/project_spec_current.md` to reflect reality.


# Spec governance

V1-GOV-001 (MUST; Status: Planned): `docs/spec/project_spec_current.md` MUST remain the authoritative description of shipped behavior.
V1-GOV-002 (MUST; Status: Planned): This document MUST NOT be treated as authoritative for current behavior.
V1-GOV-003 (SHOULD; Status: Planned): Each v1.0 requirement SHOULD be worded as a user-visible contract and avoid implementation details.
V1-GOV-004 (SHOULD; Status: Planned): When a v1.0 requirement is implemented, it SHOULD be copied (or refactored) into the current spec and covered by a black-box check, preferably a test under `tests/unit/*.spec.c` or `tests/e2e/*.spec.c`.

# v1.0 scope

The v1.0 release targets a credible public emulator experience for the Altaid 8800 concept (8080 + panel + memory/I-O board with serial and cassette), with emphasis on:

- Smooth first-run experience
- Repeatable serial and cassette workflows
- A stable, readable UI (text mode and TUI)
- Basic confidence in CPU correctness
- Clear docs and release hygiene

# Build, packaging, and release hygiene

V1-BLD-001 (MUST; Status: Done): The project MUST build cleanly with `-Wall -Wextra -Werror` on GCC and Clang.
V1-BLD-002 (MUST; Status: Done): The repository MUST NOT track build artifacts (e.g., `*.o`, generated binaries).
V1-BLD-003 (MUST; Status: Done): `make clean && make` MUST produce a runnable `./altaid-emu`.
V1-BLD-004 (SHOULD; Status: Done): CI MUST build on Linux and macOS and run at least one runtime smoke test.
V1-BLD-005 (SHOULD; Status: Done): A `make dist` or `tools/dist.sh` workflow SHOULD produce a source release tarball that contains no generated files.

# Testing coverage

V1-TST-001 (SHOULD; Status: Planned): The repo SHOULD include unit and end-to-end tests covering core subsystems (CPU, serial, cassette, persistence, UI).
V1-TST-002 (SHOULD; Status: Planned): The testing rollout MUST be tracked as a phased, sliceable plan so progress can resume across sessions without losing context.

## Test rollout plan (sliceable)

Each slice should deliver one small, reviewable test increment (one unit test file or one e2e check), and record completion in this plan.

### Phase 1: Deterministic unit tests (no filesystem, no wall-clock)

- V1-TST-101 (Done): Serial init defaults + tick math.
- V1-TST-102 (Done): Serial RX queue behavior (enqueue/level timing).
- V1-TST-103 (Done): Serial TX decode (start bit -> emitted byte).
- V1-TST-104 (Done): Cassette lifecycle (init/stop/status) without file I/O.
- V1-TST-105 (Done): State I/O header validation (bad magic/version) using in-memory helpers.

### Phase 2: Focused CLI e2e checks (fast, deterministic)

- V1-TST-201 (Done): `--help` exits 0 (smoke baseline).
- V1-TST-202 (Done): `--version` exits 0.
- V1-TST-203 (Done): Invalid flag combinations exit non-zero with usage (e.g., `--cass-play` without `--cass`).

### Phase 3: Subsystem-by-subsystem expansion

- V1-TST-301 (Done): CPU opcode micro-tests (small fixed set).
- V1-TST-302 (Done): Persistence round-trip (state + RAM) via temp files.
- V1-TST-303 (Done): Cassette record/play round-trip with known transcript.
- V1-TST-304 (Done): UI routing invariants (stdout/stderr separation).

# First-run experience

V1-FRX-001 (MUST; Status: Planned): A user MUST be able to verify the emulator runs without requiring proprietary ROMs.
V1-FRX-002 (MUST; Status: Planned): The repo MUST provide one of:
- an open "diagnostic ROM stub" (small program) OR
- a test-runner-driven diagnostic path (built as a test binary, not as app runtime code)
that can display a banner, exercise basic CPU execution, and demonstrate panel/serial output.
V1-FRX-003 (SHOULD; Status: Planned): CI SHOULD boot the diagnostic path and assert a known transcript line (smoke test).

# ROM handling and legality

V1-ROM-001 (MUST; Status: Planned): Documentation MUST state that ROMs are not distributed and users must supply their own.
V1-ROM-002 (MUST; Status: Planned): Documentation MUST state the ROM size requirement(s) and expected mapping profile(s).
V1-ROM-003 (SHOULD; Status: Planned): Documentation SHOULD include "known-good" ROM fingerprints (hashes) for community verification without distributing ROMs.

# CPU correctness and confidence

V1-CPU-001 (MUST; Status: Planned): The emulator MUST implement the full Intel 8080 instruction set.
V1-CPU-002 (MUST; Status: Planned): Flags behavior MUST match the 8080 specification.
V1-CPU-003 (SHOULD; Status: Planned): The repo SHOULD include an automated CPU regression test (e.g., exerciser binary + expected pass/fail transcript) runnable in CI.
V1-CPU-004 (SHOULD; Status: Planned): A `--trace-cpu` or equivalent debug facility SHOULD exist for reporting bugs (at minimum: PC/opcode/logging with optional rate limiting).

# Hardware model and memory map

V1-HW-001 (MUST; Status: Planned): The memory map and I/O port map MUST be documented in `docs/`.
V1-HW-002 (MUST; Status: Planned): The emulator MUST support the ROM + RAM + bank/overlay behavior required to boot the primary supported ROM profile.
V1-HW-003 (SHOULD; Status: Planned): The emulator SHOULD support selectable hardware profiles (e.g., `--hw-profile <name>`) if multiple ROM families exist.
V1-HW-004 (SHOULD; Status: Planned): Power-on/reset behavior SHOULD be documented (which latches reset, what RAM contents are, what bank/overlay bits do).

# Front panel UX

V1-PNL-001 (MUST; Status: Planned): The emulator MUST provide a usable front-panel experience (LED state display + key input).
V1-PNL-002 (MUST; Status: Planned): Key input MUST support momentary presses with a configurable duration.
V1-PNL-003 (SHOULD; Status: Planned): The panel display SHOULD be available in both a compact text snapshot mode and an always-on TUI mode.
V1-PNL-004 (SHOULD; Status: Planned): The panel SHOULD provide discoverable help describing key mappings.

# TUI rendering and Unicode

V1-TUI-001 (MUST; Status: Planned): In full-screen TUI mode, the statusline MUST remain visible at all times.
V1-TUI-002 (MUST; Status: Planned): The panel region and serial region MUST NOT overlap for terminals >= 80x25.
V1-TUI-003 (MUST; Status: Planned): Help text MUST render into the serial region (no hidden output behind the panel).

V1-TUI-010 (MUST; Status: Planned): TUI SHOULD auto-detect UTF-8 capability (locale/terminal) and prefer Unicode box-drawing borders when supported. (Default Unicode borders + `--ascii` fallback are already part of the current behavior contract.)
V1-TUI-011 (SHOULD; Status: Planned): A `--unicode` flag SHOULD force Unicode rendering even if detection is inconclusive.
V1-TUI-012 (SHOULD; Status: Planned): Other optional Unicode niceties MAY be used when available (e.g., heavier borders for the panel frame, ellipsis glyph, arrows), but MUST have an ASCII fallback under `--ascii`.

# Serial I/O and host integration

V1-SER-001 (MUST; Status: Planned): The emulator MUST support interactive serial I/O to the host terminal (non-PTY mode).
V1-SER-002 (MUST; Status: Planned): The emulator MUST support PTY mode for integrating with terminal programs.
V1-SER-003 (SHOULD; Status: Planned): Documentation SHOULD include a "golden path" for connecting to the PTY and loading software over serial.

V1-SER-010 (SHOULD; Status: Planned): The repo SHOULD include a small serial sending utility in `tools/` that supports:
- configurable per-character and per-line pacing
- CR/LF translation modes
- sending to a PTY path
and is documented with examples.

# Cassette

V1-CAS-001 (MUST; Status: Planned): The emulator MUST support cassette playback and recording for the supported ROM profile.
V1-CAS-002 (MUST; Status: Planned): Detaching a cassette while recording MUST auto-stop and save before detach.
V1-CAS-003 (MUST; Status: Planned): The cassette file format MUST be versioned and specified as a stable external contract.
V1-CAS-004 (MUST; Status: Planned): Multi-byte cassette file fields MUST use an explicit endianness (little-endian preferred) and MUST NOT depend on host ABI/packing.
V1-CAS-005 (SHOULD; Status: Planned): Tools SHOULD exist to inspect or summarize a cassette image (header + duration stats) for debugging.

# Persistence (state + RAM)

V1-SAV-001 (MUST; Status: Planned): Full machine state save/load MUST exist.
V1-SAV-002 (MUST; Status: Planned): RAM-only save/load MUST exist.
V1-SAV-003 (MUST; Status: Planned): Full state load MUST fully reset host scheduling/timing so the restored session resumes consistently.
V1-SAV-004 (MUST; Status: Planned): RAM-only load MUST preserve CPU/device state and host scheduling.
V1-SAV-005 (SHOULD; Status: Planned): State/RAM file formats SHOULD be versioned, endian-stable, and include ROM identity verification.

# Determinism and replay

V1-DET-001 (MUST; Status: Planned): The emulation core MUST be deterministic with respect to its inputs.
V1-DET-002 (SHOULD; Status: Planned): A "session capture" mode SHOULD exist that records host serial input bytes with tick timestamps and records cassette attach/play/record actions.
V1-DET-003 (SHOULD; Status: Planned): A "session replay" mode SHOULD exist that replays a captured session deterministically to reproduce issues.

# Diagnostics and reporting

V1-DIAG-001 (SHOULD; Status: Planned): `--version` SHOULD include build version and key defaults (hz, baud) for bug reports.
V1-DIAG-002 (SHOULD; Status: Planned): There SHOULD be an option to dump current machine state summary (registers + bank/overlay bits + tick counters) without exiting.
V1-DIAG-003 (SHOULD; Status: Planned): Logging SHOULD be configurable and MUST avoid hot-path overhead unless explicitly enabled.

# Documentation

V1-DOC-001 (MUST; Status: Done): README MUST include:
- build instructions
- the ROM requirement and legality note
- a minimal run command for text mode, TUI mode, and PTY mode
- troubleshooting for common first-run failures (including: missing compiler/toolchain, missing test-runner submodule, "no output", and terminal sizing)

V1-DOC-002 (SHOULD; Status: Planned): README SHOULD include screenshots or an animated GIF of the panel/TUI.
V1-DOC-003 (SHOULD; Status: Planned): `docs/` SHOULD include a compatibility envelope describing what is and is not modeled.
V1-DOC-004 (SHOULD; Status: Planned): `docs/` SHOULD include the serial and cassette workflows as step-by-step recipes.

V1-DOC-005 (SHOULD; Status: Planned): The repo SHOULD provide a concise build troubleshooting section (in README and/or `docs/troubleshooting.md`) that includes copy/paste fixes for:
- missing compiler / SDK headers
- missing or incompatible `make`
- missing test-runner submodule
- common build errors and how to report them (include `./altaid-emu --version` output)


V1-DOC-007 (SHOULD; Status: Done): The repo SHOULD document the release process (version bump, changelog update, tags, and `make dist`) in `docs/spec/release.md`.

V1-DOC-006 (SHOULD; Status: Done): The repo SHOULD maintain a small `CHANGELOG.md` containing user-visible release notes, updated when releases are cut.
