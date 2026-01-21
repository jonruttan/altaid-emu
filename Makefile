# This Makefile is intentionally written to work with both BSD make (macOS
# /usr/bin/make) and GNU make. We avoid non-portable make conditionals and
# compute small platform-specific flags inside the shell recipes.

CC ?= cc
WERROR ?= -Werror
CFLAGS ?= -std=c99 -O2 -Wall -Wextra $(WERROR)

INCLUDES = -I./include

PLATFORM_LIBS =

SRCS = \
  src/main.c \
  src/cli.c \
  src/version.c \
  src/emu.c \
  src/emu_core.c \
  src/emu_host.c \
  src/runloop.c \
  src/timeutil.c \
  src/log.c \
  src/ui.c \
  src/panel_ansi.c \
  src/panel_text.c \
  src/hostpty.c \
  src/serial.c \
  src/altaid_hw.c \
  src/cassette.c \
  src/stateio.c \
  src/i8080.c

OBJS = $(SRCS:.c=.o)

all: altaid-emu

altaid-emu: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(PLATFORM_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) altaid-emu

distclean: clean
	rm -rf tools/__pycache__
	find . -name '*.pyc' -type f -exec rm -f {} +

dist:
	./tools/dist.sh

# Tests
#
# This project uses jonruttan/test-runner as a git submodule at
# tests/test-runner. The test runner is intentionally not vendored.
#
# Usage examples:
#   make tests
#   TESTS=tests/src/foo.spec.c make tests
#   RUNNER=tests/test-runner/test-runner.sh make tests

tests:
	@runner="$(RUNNER)"; \
	[ -n "$$runner" ] || runner="tests/test-runner/test-runner.sh"; \
	if [ ! -x "$$runner" ]; then \
		echo "ERROR: test runner not found at $$runner"; \
		echo "Hint: git submodule update --init --recursive"; \
		exit 1; \
	fi; \
	tests="$(TESTS)"; \
	if [ -z "$$tests" ]; then \
		set -- tests/src/*.spec.c; \
		if [ "$$1" = "tests/src/*.spec.c" ]; then \
			echo "ERROR: no tests found (tests/src/*.spec.c)"; \
			exit 1; \
		fi; \
		tests="$$@"; \
	fi; \
	CFLAGS="$(CFLAGS) -O0 -g -DTESTS" sh "$$runner" $$tests

test: tests

.PHONY: all clean distclean dist tests test
