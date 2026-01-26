# Design philosophy

This document captures enduring principles for maintaining this repository.
It is intended to be used by humans and tooling (including AI) to keep changes
aligned with the project's direction.

## How to use this document

- When a slice is motivated by a principle, keep this document accurate.
- Prefer stable statements that will remain true over time.
- Other docs may paraphrase these ideas, but this file is the canonical source.

## Principles

### Keep changes small and reviewable

**Intent:** Make progress in small slices with clear, reviewable value.

**Rationale:** Small, testable increments reduce regressions and make it easier to
keep specs aligned with reality.

**Do:**
- Implement exactly one slice at a time.
- Update specs/docs in the same slice when behavior changes.

**Don't:**
- Bundle unrelated refactors into feature changes.

---

### Specs describe expectations, not implementation

**Intent:** Specs define what the project should do, independent of code layout.

**Rationale:** Implementation will evolve; expectations should stay stable.

**Do:**
- Write specs as observable behavior and user-visible outcomes.

**Don't:**
- Encode internal architecture decisions into specs unless required to meet a
  user-visible contract.

---

### Single source of truth for procedures

**Intent:** Avoid maintaining the same multi-step guidance in multiple places.

**Rationale:** Repeated procedures drift and create stale/conflicting guidance.

**See also:** DEC-0021 in `docs/spec/project_decisions.md`.

**Do:**
- Keep the full procedure in one canonical place.
- Elsewhere, paraphrase briefly and point to the canonical place.

**Don't:**
- Copy/paste full step-by-step recipes into multiple documents.

**Note:** Build output (e.g., Makefile errors) may print short hints, but should
not become a second troubleshooting manual.

---

### Fix root causes at the source

**Intent:** Prefer upstream fixes over local wrappers/workarounds.

**Rationale:** Wrappers accrete, hide the real problem, and increase maintenance
costs.

**Do:**
- Fix issues in the component that owns the behavior.
- Keep integration layers thin and unsurprising.

**Don't:**
- Add glue code that papers over a dependency bug unless the upstream fix is
  impossible or explicitly requested.

---

### Prefer explicit context over mutable globals

**Intent:** Keep state and dependencies explicit so the code remains testable,
composable, and adaptable.

**Rationale:** Implicit global state couples unrelated modules, makes unit tests
fragile, and blocks running multiple emulator instances in one process.

**Do:**
- Store runtime state (logging, UI/panel state, routing flags) in a per-instance
  context (for example `EmuHost`, `UI`, or a dedicated context struct).
- Pass the context into functions that need it.
- Keep side effects at the edges: rendering/logging/IO should be driven by
  explicit streams or handles.

**Don't:**
- Add new mutable globals for convenience.

**Exception:** A single global pointer MAY be used when required by OS callbacks
(for example signal handlers), but it should remain narrow and well-documented.

---

### Isolate side effects and make outputs swappable

**Intent:** Side-effect-free code is easier to test; side effects should be
isolated and easy to redirect.

**Rationale:** If modules write to process-global stdout/stderr directly, tests
must intercept global state and the TUI can be corrupted.

**Do:**
- Route user-visible output through explicit `FILE *` streams owned by the
  instance (or a logger/panel object).
- Keep parsing/validation logic free of side effects.

**Don't:**
- Mix argument parsing, IO, and emulator execution in one function without
  clear boundaries.

---

### Improve upstream test tools rather than adding ad-hoc harness code

**Intent:** Keep the emulator repo focused on emulator behavior.

**Rationale:** Repo-local output-capture and system-call interception code tends
to drift, duplicates effort across projects, and violates separation of
concerns.

**Do:**
- Enhance upstream test helpers (e.g., file/memory capture) when tests need new
  capabilities.

**Don't:**
- Add one-off harness code inside the emulator project to paper over missing
  test-runner functionality.

---

### Keep shared types in neutral headers

**Intent:** Avoid smearing cross-cutting types into subsystem headers.

**Rationale:** When a shared type lives in a narrow subsystem header (for
example, CLI), unrelated code must include that subsystem just to access the
type. This increases coupling, makes tests more fragile, and blurs ownership.

**Do:**
- Place widely-used structs/enums in a neutral header owned by the layer that
  consumes them (for example, emulator/host configuration).
- Keep subsystem headers focused on their own APIs.

**Don't:**
- Define emulator-wide configuration types in a CLI header.

---

### Name types by their true scope

**Intent:** Avoid misleading names that cause architecture drift.

**Rationale:** If a type contains host/UI/integration settings as well as emulator
core settings, calling it "Emu*" smears responsibilities and invites misuse.

**Do:**
- Name cross-layer types by the layer that owns/consumes them (e.g., application
  run configuration).

**Don't:**
- Use narrow names for mixed-scope data structures.

## Changelog

- 2026-01-21: Initial draft.
