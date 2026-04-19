# Project decisions (ADR-style log)

This log records decisions that affect architecture, workflow, testing, docs,
and maintenance.

## Rules

- Each decision has a stable ID.
- Decisions may be superseded, but not deleted.
- Other docs may paraphrase decisions, but this file is the canonical record.

## Template

### DEC-XXXX: <short title>

- Date: YYYY-MM-DD
- Status: Accepted | Superseded
- Context:
- Decision:
- Consequences:
- References: (commits/files/issues if applicable)

---

## Decisions

### DEC-0020: Track meta project concepts explicitly

- Date: 2026-01-21
- Status: Accepted
- Context:
  - Project guidance and rationale accumulate over time and must remain coherent
    across slices and contributors.
- Decision:
  - Maintain canonical meta-spec documents in-repo:
    - `docs/spec/design_philosophy.md`
    - `docs/spec/project_decisions.md`
- Consequences:
  - Slices that introduce or rely on a new principle/constraint must update one
    or both canonical meta-spec documents.

---

### DEC-0021: Reduce duplicated procedural guidance

- Date: 2026-01-21
- Status: Accepted
- Context:
  - Repeating the same step-by-step fix in multiple places creates drift.
  - Some redundancy is unavoidable (e.g., short hints in build errors), but full
    procedures should not be duplicated.
- Decision:
  - Multi-step procedures have exactly one canonical home.
  - Other documents may paraphrase briefly and point to the canonical source.
  - Build output may emit short hints, but must not duplicate full procedures.
- Consequences:
  - Lower documentation maintenance overhead.
  - Fewer stale/conflicting instructions.
- References:
  - `docs/spec/design_philosophy.md` (Single source of truth for procedures)

---

### DEC-0023: Move emulator configuration out of the CLI header

- Date: 2026-01-23
- Status: Accepted
- Context:
  - `struct AppConfig` is consumed by host/emulator layers and is not CLI-specific.
  - When a shared type is defined in a narrow subsystem header, unrelated code
    must include that subsystem header just to access the type.
- Decision:
  - Define `struct AppConfig` and related enums in `include/app_config.h`.
  - Keep `include/cli.h` focused on CLI APIs and include `app_config.h`.
- Consequences:
  - Clearer ownership and fewer misleading dependencies.
  - Reduced coupling between CLI and non-CLI code.
---

### DEC-0024: Name mixed host+core settings as AppConfig

- Date: 2026-01-23
- Status: Accepted
- Context:
  - The primary configuration struct includes emulator-core settings (CPU/baud)
    and host/UI/integration settings (panel/UI, PTY routing, cassette file paths,
    persistence paths, logging).
  - Calling this struct `EmuConfig` would be misleading because it is not solely
    consumed by the emulator core.
- Decision:
  - Name the validated run configuration `struct AppConfig` and keep it in a
    neutral header (`include/app_config.h`).
- Consequences:
  - Clearer ownership boundaries and less semantic drift.
  - Future splits (e.g., `EmuCoreConfig` vs host/UI config) remain possible
    without renaming user-facing concepts.

---

### DEC-0025: Prefer per-instance contexts over mutable globals

- Date: 2026-01-23
- Status: Accepted
- Context:
  - The emulator is easier to test and reason about when its state and
    dependencies are explicit.
  - Mutable process-global state makes unit tests fragile and blocks running
    multiple emulator instances in one process.
- Decision:
  - New stateful subsystems (logging, UI/panel, routing) MUST store mutable
    state in a per-instance context and pass that context through APIs.
  - New code MUST avoid introducing mutable globals for convenience.
  - A single global pointer MAY be used only when required by OS callbacks
    (e.g., signal handlers), and it must remain narrow and well-documented.
- Consequences:
  - Clearer ownership boundaries and easier test isolation.
  - Future multi-instance support remains feasible.

---

### DEC-0026: Improve upstream test helpers rather than adding emulator-local harness code

- Date: 2026-01-23
- Status: Accepted
- Context:
  - The project maintains upstream testing helpers (test-runner and its helper
    headers).
  - Emulator-local output capture or system-call interception code tends to
    duplicate effort and drift from upstream.
- Decision:
  - When tests need new capabilities (stdout capture, temp files, etc.), prefer
    improving the upstream helper library rather than adding one-off harness
    code inside the emulator repository.
- Consequences:
  - Lower maintenance overhead and clearer separation of concerns.
  - Testing infrastructure improvements benefit all dependent projects.
- References:
  - `docs/spec/design_philosophy.md` (Improve upstream test tools)

### DEC-0027: Spec-based persistence flags replace six single-purpose flags

- Date: 2026-04-19
- Status: Accepted
- Context:
  - Previously six flags covered a small matrix of behaviors:
    `--state-file/--state-load/--state-save` and
    `--ram-file/--ram-load/--ram-save` (short aliases `-s/-J/-W/-M/-G/-B`).
  - The short letters were picked arbitrarily (J for state-load, W for
    state-save, G for ram-load, B for ram-save) and were hard to remember.
  - The flag matrix did not compose: there was no way to load a raw blob
    into a specific RAM bank at a specific address for test-program
    injection.
- Decision:
  - Replace the six flags with three repeatable spec-driven flags:
    `--load <spec>`, `--save <spec>`, `--default <spec>`.
  - Spec grammar: `state:<file>`, `ram:<file>`, `ram@<addr>:<file>`,
    `ram@<bank>.<addr>:<file>`.
  - Drop the `ALTAIDRM` magic/header and the cross-ROM / cpu_hz / baud
    sanity checks for RAM files: they gated legitimate cross-ROM and
    test-fixture workflows while only catching a narrow class of user
    error that resulted in garbage output, not data loss.  State files
    keep `ALTAIDST` magic + u32 version for format evolution but drop
    the same sanity checks.
- Consequences:
  - Fewer flags, composable, documented grammar.
  - `--load ram@0x8100:test.bin` unlocks scripted firmware-level testing.
  - `.ram` files produced by this version are not compatible with prior
    versions; there is no migration tool (files were never a stable
    contract).
- References:
  - `docs/persistence.md`
  - Commit c91456d

### DEC-0028: UART RX frames are gated on CPU INTE

- Date: 2026-04-19
- Status: Accepted
- Context:
  - Real hardware delivers serial bits at line rate regardless of CPU
    state; bytes that arrive during DI sections (or pre-EI boot) are
    silently lost.
  - The v0.6 ROM spends ~600K cycles with interrupts disabled during
    boot (PUT_CHAR bit-bang DI sections, CHKSUM_ROM, LOAD_CPM) before
    MAIN_MENU performs EI.  Piped stdin arriving during that window
    vanished into orphan UART frames, making scripted input unusable.
- Decision:
  - The UART only pops a byte from the host RX queue into a new line
    frame when the CPU currently has interrupts enabled.  Once a frame
    starts it plays out to completion regardless of INTE.
  - Callers set `ser->gate_inte = cpu->inte` each step from emu_core.c.
- Consequences:
  - Piped input is reliable (bytes queue until the ROM can handle
    them); matches what an operator at a terminal would naturally
    observe.
  - Not strictly faithful to real hardware's "bytes are lost if not
    serviced"; this is a deliberate host-side convenience, not a
    change to the EmuCore timing model.
- References:
  - `src/serial.c`, `src/emu_core.c`
  - `README.md` (Serial options)
  - Commit af6df67

### DEC-0029: D-switches are latched; RUN/MODE/NEXT stay momentary

- Date: 2026-04-19
- Status: Accepted
- Context:
  - The emulator previously modeled all 11 front-panel keys (D0..D7,
    RUN, MODE, NEXT) as momentary: `fp_key_down[i]` is true only
    while `now_tick < fp_key_until[i]`.
  - That's correct for RUN/MODE/NEXT (spring-loaded buttons) but
    wrong for D0..D7 (physical toggle switches).  Boot baud-select
    and the alternate ISR variants read the switch port on every
    scan during boot and need a value that persists across the
    whole read loop.
  - The TUI works around it by re-pulsing on each keystroke, which
    is fine for interactive use but cannot represent "D3 is ON
    while boot polls INPUT_PORT" in `--headless` runs.
- Decision:
  - Add `fp_switch_state[8]` alongside the existing
    `fp_key_down[11]`.  The scan-row reader ORs the two, so either
    source pulling low registers as "switch on" to the CPU.
  - New API `altaid_hw_panel_set_switch(hw, d_index, on)` drives
    the latched layer.  `altaid_hw_panel_tick` does NOT clear it;
    `altaid_hw_reset_runtime` also leaves it alone (a reset
    wouldn't un-flip a physical switch either).
  - Two new CLI flags: `--press <key>[@<ms>[:<hold_ms>]]` for
    momentary presses of any key, `--switch <Dn>=<0|1>[@<ms>]` for
    latched data-switch state.  Events are scheduled by emulated
    CPU time, dispatched from the runloop once per emu_run.
  - TUI behavior is unchanged.  Ctrl-P `3` still pulses D3; it does
    not toggle the latch.
- Consequences:
  - GET_KEY, boot baud-select, FORMAT-confirm, and alternate ISR
    variants become reachable under `--headless`.
  - Bump STATEIO_VER from 1 to 2 to cover the new field; prior
    `.state` files are rejected with the existing "unsupported
    state file version" error.
- References:
  - `src/altaid_hw.c`, `include/altaid_hw.h`
  - `src/cli.c`, `src/runloop.c`, `include/cli.h`
  - `docs/spec/project_spec_current.md` (Scripted front-panel input)
  - `docs/spec/project_spec_v1.md` (V1-PANEL-001/002)
