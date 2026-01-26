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

### DEC-0022: Track the current slice number in workflow

- Date: 2026-01-21
- Status: Accepted
- Context:
  - Slice numbers are used for ordering and naming tarballs, and for continuity
    across chats.
- Decision:
  - Track the current slice number in `docs/spec/workflow.md`.
- Consequences:
  - A single, repo-local place to pick up where we left off.

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
