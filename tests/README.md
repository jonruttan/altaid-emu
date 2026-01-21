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
make tests
```

To run with an instrumentation wrapper (for example valgrind or lldb):

```sh
make tests-wrapped
```

You can override the wrapper explicitly:

```sh
WRAPPER='valgrind --leak-check=full' make tests-wrapped
WRAPPER='lldb --batch -o run --' make tests-wrapped
```

You can also run a single spec file:

```sh
TESTS=tests/src/smoke.spec.c make tests
```
