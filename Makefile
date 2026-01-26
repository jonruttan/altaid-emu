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
TEST_PATH ?= tests
TESTS ?= $(TEST_PATH)/unit

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

.PHONY: all clean distclean dist

test-wrapped:
	@if [ ! -f "$(TEST_RUNNER)" ]; then \
		echo "ERROR: test runner not found at $(TEST_RUNNER)"; \
		echo "Hint: git submodule update --init --recursive"; \
		echo "Alt (GitHub source archive): download test-runner main.zip and extract into: tests/test-runner/"; \
		echo "  https://github.com/jonruttan/test-runner/archive/refs/heads/main.zip"; \
		exit 1; \
	fi
	@CFLAGS="$(CFLAGS) -g -Og -I./src -I./include -DTESTS" sh "$(TEST_RUNNER)" $(TESTS)

test:
	@WRAPPER=command TESTS=$(TESTS) $(MAKE) test-wrapped

test-unit: TESTS=$(TEST_PATH)/unit
test-unit: test

test-wrapped-unit: TESTS=$(TEST_PATH)/unit
test-wrapped-unit: test-wrapped

test-e2e: TESTS=$(TEST_PATH)/e2e
test-e2e: altaid-emu test

test-wrapped-e2e: TESTS=$(TEST_PATH)/e2e
test-wrapped-e2e: altaid-emu test-wrapped

test-all: test-unit test-e2e
test-wrapped-all: test-wrapped-unit test-wrapped-e2e

.PHONY: test-wrapped test test-unit test-wrapped-unit test-e2e test-wrapped-e2e test-all test-wrapped-all
