# Workflow

This repo is maintained in **small, reviewable slices**.

The goal is to keep the project **small and shippable**.
Avoid adding new tools or documents unless they directly reduce user confusion,
prevent regressions, or simplify contribution.

## Where things live

- `docs/` : documentation for humans (usage + contributor notes).
  - User docs: memory/port map, cassette, persistence, troubleshooting.
  - Contributor docs: `docs/style.md`, `docs/architecture.md`, `docs/release.md`.
- `docs/spec/` : product specs (current + v1) and workflow for maintaining them.

## Specs and single source of truth

- `docs/spec/project_spec_current.md` is the **single source of truth** for what
  the repo **currently** promises and does.
- `docs/spec/project_spec_v1.md` is the **v1.0 target** (aspirational roadmap).

If a slice changes user-visible behavior, update **current spec** in the same
slice.

## Slice checklist

For each slice:

- Keep changes small and focused.
- If you change behavior or defaults, update **at least one** of:
  - `docs/spec/project_spec_current.md`
  - relevant subsystem docs (`docs/port-map.md`, `docs/memory-map.md`,
    `docs/cassette.md`, `docs/persistence.md`, `docs/fidelity.md`)
  - `README.md` (if it affects “how to run it”)
- If you complete a v1.0 target, reflect it in **current spec**, and update the
  v1 document to mark/adjust the item.
- If you change module boundaries or responsibilities, update
  `docs/architecture.md`.
- All new or modified code MUST include unit tests, unless the slice explicitly
  documents why tests are not practical for that change.
- Run tests before shipping the slice:
  - `make tests`

Optional: run tests with an instrumentation wrapper (for example valgrind or
lldb):

- `make tests-wrapped`

## Release notes

Release notes live in `CHANGELOG.md`.

To keep maintenance overhead low and avoid stale noise:

- Update `CHANGELOG.md` **only when preparing a release** (see
  `docs/release.md`).
- Keep entries **user-visible** and curated (flags, behavior, formats, notable
  fixes).
- Do **not** add filler like “see git history”. If you can't summarize it for
  users, leave it out.

## Tarball naming

When producing slice tarballs, use:

- `altaid-emu-sliceNN-<brief-description>.tar.xz`
- `NN` is zero-padded for natural sorting (e.g., `slice01`, `slice02`, ...).
- Keep the description short, lowercase, hyphenated.

## Continuation prompt (tarball or repo link)

Use this as the first message in a new chat when resuming work:

```text
You are resuming development of this repo using repo-local rules only.

Input:
- I have provided either (a) an uploaded repo tarball, or (b) a GitHub repo link.

Rules:
1) Do not rely on prior chat history. Derive all context from the repo.
2) Do not browse the web unless I explicitly ask.
3) Work in one small slice at a time.

First read (in order):
- docs/spec/workflow.md
- docs/spec/project_spec_current.md
- docs/spec/project_spec_v1.md

Then:
- Determine what v1 items are Done/Planned based on repo evidence.
- Choose the next smallest high-value slice.
- Implement exactly one slice.
- Update specs per docs/spec/workflow.md.
- Ensure `make tests` passes (or report failures).
- Output a tarball named: altaid-emu-sliceNN-<brief-description>.tar.xz
```
