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

You can also run a single spec file:

```sh
TESTS=tests/src/smoke.spec.c make tests
```
