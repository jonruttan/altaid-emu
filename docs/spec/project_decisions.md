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