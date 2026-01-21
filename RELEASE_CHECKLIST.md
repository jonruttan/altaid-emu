# Release Checklist

Before tagging `v1.0.0`:

## Legal / licensing
- [ ] Confirm license text + copyright holder.
- [ ] Confirm no ROM images are included in the repo or release artifacts.
- [ ] Audit third-party code usage and add attribution/NOTICE if needed.

## Docs / UX
- [ ] README: verify Quick start works from a fresh clone.
- [ ] Add at least one screenshot / GIF of `-panel -ui`.
- [ ] Document I/O ports and memory map (see `docs/`).
- [ ] Document known limitations and fidelity expectations.

## Build / CI
- [ ] CI green on macOS + Linux.
- [ ] Consider adding Windows build job or document unsupported.
- [ ] `make clean && make` works with both clang and gcc.

## Testing
- [ ] Add CPU opcode regression tests (e.g. 8080 exerciser, small fixtures).
- [ ] Add device/port tests for `IN 0x40`, `OUT 0xC0`, bank switching.
- [ ] Add deterministic cassette record/play round-trip test.

## Release mechanics
- [ ] Tag `v1.0.0` and create a GitHub Release.
- [ ] Attach prebuilt binaries if desired.
- [ ] Update `CHANGELOG.md` with release notes.
