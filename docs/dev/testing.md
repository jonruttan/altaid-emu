# Testing

This project uses the author's **test-runner** harness.

The test harness is **not vendored** into this repository. Instead, it is
referenced as a git submodule at `tests/test-runner`.

## Setup

```sh
git submodule update --init --recursive
```

## Run the test suite

```sh
make tests
```

## Run a single spec file

```sh
TESTS=tests/src/smoke.spec.c make tests
```
