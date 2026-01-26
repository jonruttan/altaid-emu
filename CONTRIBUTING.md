# Contributing

Thanks for contributing!

## Ground rules

- Follow the slice workflow in `docs/spec/workflow.md`.
- Keep changes small and reviewable.
- Prefer portability (macOS + Linux). Avoid non-POSIX APIs unless guarded.
- When changing I/O behavior, update `docs/port-map.md` and/or `docs/memory-map.md`.

## Building

```sh
make clean && make
```

## Testing

Run the test suite:

```sh
make test-all
```

Optional: run tests under an instrumentation wrapper (for example valgrind or lldb):

```sh
make test-wrapped-all
```

If you add fidelity fixes or change behavior, please add targeted regression tests
under `tests/unit/` and `tests/e2e`.

## Style

- C99
- No dynamic allocation in hot loops unless necessary
- Keep serial/cassette timing deterministic

## Coding style

Please follow the project coding style (Linux-kernel-ish) described in
`docs/spec/style.md`.

Before submitting a PR, run:

```sh
./tools/check_style.sh
```

## Clean tree

Please do not commit generated artifacts (object files, binaries, Python caches).

CI rejects PRs that track common build artifacts (`altaid-emu`, `*.o`).
`make dist` builds the release tarball from tracked content only (`git archive`).
