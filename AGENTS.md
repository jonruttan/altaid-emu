# AGENTS.md — Codex operating contract for this repo

This repo is maintained in small, reviewable slices. Optimize for simplicity,
low maintenance overhead, and spec-truthfulness.

## Non-negotiables

- Repo-local rules only. Do not rely on prior chat context.
- Do not browse the web unless the user explicitly asks.
- One slice at a time: smallest high-value change that can be reviewed.
- Prefer removing over adding. Avoid new tools/docs unless they reduce confusion,
  prevent regressions, or simplify contribution.
- Do not modify submodule contents or bump submodule revisions unless explicitly asked.

## Required reading order (do this first for any task)

1) docs/spec/workflow.md
2) docs/spec/project_spec_current.md
3) docs/spec/project_spec_v1.md
Then consult as-needed:
- docs/spec/design_philosophy.md
- docs/spec/project_decisions.md
- docs/spec/style.md
- docs/architecture.md (when moving responsibilities/boundaries)

## Slice execution rules

For each slice:

- Determine v1 items Done/Planned based on repo evidence.
- Choose the next smallest high-value slice.
- Implement exactly one slice.
- Update specs per docs/spec/workflow.md (current spec is the source of truth).
- If the slice is motivated by (or introduces) a principle/constraint, update
  docs/spec/design_philosophy.md and/or docs/spec/project_decisions.md (ADR-style).
- All new or modified code MUST include unit tests unless the slice explicitly
  documents why tests are not practical.

## Testing rules

- Always run: `make test-unit && make test-e2e` (or the repo’s documented test target).
- Do not break test harness invariants:
  - `*.spec.c` MUST NOT define `main()` (test-runner provides it).
  - Preserve semantics of `TEST_RUNNER` / `WRAPPER`.
  - Do not remove/rename `make test` / `make test-wrapped` without an explicit request.

## Design / code principles to enforce

- Specs describe expectations, not implementation.
- Single source of truth for procedures: avoid duplicating multi-step guidance.
- Fix root causes at the source; keep integration glue thin.
- Prefer explicit context over mutable globals; avoid introducing new globals.
  One narrow global pointer is allowed only when required by OS callbacks.
- Isolate side effects; keep parsing/validation side-effect-free.
- Route user-visible output through explicit `FILE *` streams owned by the instance.
- Keep shared types in neutral headers; name types by their true scope.

## Makefile / CI constraints

- Treat the current Makefile interface as the user-facing API.
- Do not change Makefile or CI unless:
  - there is a real bug preventing the intended workflow, or
  - the user explicitly requests a change.
- If a Makefile/CI change is necessary, keep it minimal and justify it in the slice.

## Output expectations (when requested)

- If producing a slice artifact, follow docs/spec/workflow.md for naming and structure.
- Provide a clear summary of what changed, what specs were updated, and test results.
