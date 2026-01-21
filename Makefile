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

ifndef PATH_TESTS
PATH_TESTS=tests
endif

ifndef TESTS
TESTS=$(PATH_TESTS)/src/*.spec.c
endif

TEST_RUNNER ?= tests/test-runner/test-runner.sh
TESTS  ?= $(wildcard tests/src/*.spec.c)

tests test:
	@if [ ! -f "$(TEST_RUNNER)" ]; then \
		echo "ERROR: test runner not found at $(TEST_RUNNER)"; \
		echo "Hint: git submodule update --init --recursive"; \
		exit 1; \
	fi
	@CFLAGS="$(CFLAGS) -g -Og -I./src -I./include -DTESTS" sh "$(TEST_RUNNER)" $(TESTS)

tests-quick test-quick:
	@CFLAGS="$(CFLAGS) -g -Og -I./src -I./include -DTESTS" RUNNER=command sh "$(TEST_RUNNER)" $(TESTS)

.PHONY: all clean distclean dist tests test tests-quick test-quick
