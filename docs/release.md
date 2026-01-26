# Release process

This document describes how to cut a release for this repo.

Philosophy: keep it **small and honest**.

- The changelog is **curated** and **user-facing**.
- We update release notes **when cutting a release**, not for every slice.

## Versioning

The version string lives in `src/version.c` as:

`#define ALTAID_EMU_VERSION "X.Y.Z"`

Guidelines:

- While `0.x.y`:
  - bump **y** for bugfixes and small improvements
  - bump **x** for notable user-facing features or behavioral changes
- After `1.0.0`, treat breaking changes as **major** bumps.

## Release checklist

### 1) Prep

- [ ] Ensure `main` is green in CI.
- [ ] Run locally:
  - [ ] `make clean && make`
  - [ ] `make test-all`

### 2) Update version + notes

- [ ] Bump `ALTAID_EMU_VERSION` in `src/version.c`.
- [ ] Update `CHANGELOG.md`:
  - [ ] move the relevant bullets from **Unreleased** into a new version header
  - [ ] keep entries user-visible (flags, behavior, formats, notable fixes)
  - [ ] do **not** add filler like “see git history”

### 3) Tag

- [ ] Commit the version + changelog update.
- [ ] Create an annotated tag:
  - [ ] `git tag -a vX.Y.Z -m "vX.Y.Z"`
- [ ] Push commit + tag.

### 4) Build a source tarball

- [ ] Run `make dist`.

Notes:

- `tools/dist.sh` builds a deterministic tarball from **tracked** content using
  `git archive`.
- The output is written to `dist/altaid-emu-X.Y.Z.tar.xz`.

### 5) Publish

- [ ] Create a GitHub Release for tag `vX.Y.Z`.
- [ ] Copy the `CHANGELOG.md` entry into the release description.
- [ ] Attach `dist/altaid-emu-X.Y.Z.tar.xz` (optional but recommended).

## Legal / ROM notes

- Do not distribute proprietary ROM images in the repo or release artifacts.
- If you publish ROM hashes for “known good” verification, include only hashes
  (no ROM bytes).
