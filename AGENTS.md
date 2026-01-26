# AGENTS.md — AI entrypoint for this repo

This file is the root of the AI pathway. It contains only AI-specific rules.
Shared workflow rules live in `docs/spec/workflow.md`.
For human contributors, start with `CONTRIBUTING.md`.

## AI-only rules

- Repo-local rules only. Do not rely on prior chat context.
- Do not browse the web unless the user explicitly asks.
- Do not modify submodule contents or bump submodule revisions unless explicitly asked.
- Keep responses concise; avoid duplicating shared workflow text that already
  exists in specs.

## Required reading order (start here)

1) docs/spec/workflow.md
2) docs/spec/project_spec_current.md
3) docs/spec/project_spec_v1.md

Then consult as-needed:
- docs/spec/design_philosophy.md
- docs/spec/project_decisions.md
- docs/spec/commit-guidelines.md (when preparing commits)
- docs/architecture.md (when moving responsibilities/boundaries)
