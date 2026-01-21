# This Makefile targets GNU make.

CC ?= cc
WERROR ?= -Werror
CFLAGS ?= -std=c99 -O2 -Wall -Wextra $(WERROR)

INCLUDES = -I./include -I./src

PLATFORM_LIBS =

# Stable ordering keeps incremental builds predictable across hosts.
SRC_ALL := $(shell find src -type f -name '*.c' -print | LC_ALL=C sort)
LIB_SRCS := $(filter-out src/main.c,$(SRC_ALL))

OBJS = $(SRC_ALL:.c=.o)

TEST_RUNNER ?= tests/test-runner/test-runner.sh
TESTS ?= $(wildcard tests/src/*.spec.c)

all: altaid-emu

altaid-emu: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(PLATFORM_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) altaid-emu

distclean: clean

dist:
	./tools/dist.sh

test:
	@if [ ! -f "$(TEST_RUNNER)" ]; then \
		echo "ERROR: test runner not found at $(TEST_RUNNER)"; \
		echo "Hint: git submodule update --init --recursive"; \
		exit 1; \
	fi
	@CFLAGS="$(CFLAGS) -g -Og -I./src -I./include -DTESTS" sh "$(TEST_RUNNER)" $(TESTS)

test-quick:
	@RUNNER=command $(MAKE) test

tests: tests
tests-quick: tests-quick

.PHONY: all clean distclean dist tests test tests-quick test-quick
