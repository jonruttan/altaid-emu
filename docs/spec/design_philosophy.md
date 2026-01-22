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

### Slice numbers are coordination, not correctness

**Intent:** Use slice numbers to keep work ordered and easy to reference.

**Rationale:** The number is a bookkeeping tool; correctness is enforced by tests
and specs.

**Do:**
- Keep slice numbers monotonic for continuity.
- Track the current slice number in `docs/spec/workflow.md`.

**Don't:**
- Treat slice numbers as a semantic version.

## Changelog

- 2026-01-21: Initial draft.
