# Tests

This project uses the **jonruttan/test-runner** harness (as a git submodule)
to run C "spec" tests.

## Setup

From a git checkout:

```sh
git submodule update --init --recursive
```

## Run

```sh
make test-all
```

Note: `tests/e2e/smoke.spec.c` is an end-to-end smoke test. It uses
`system()` to run `./altaid-emu --help` and may build the binary if it is
missing.

To run with an instrumentation wrapper (for example valgrind or lldb):

```sh
make test-wrapped-all
```

You can override the wrapper explicitly:

```sh
WRAPPER='valgrind --leak-check=full' make test-wrapped
WRAPPER='lldb --batch -o run --' make test-wrapped
```

You can also run a single spec file:

```sh
TESTS=tests/unit/timeutil.spec.c make test
```
