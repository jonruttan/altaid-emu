/*
 * runloop.spec.c
 *
 * End-to-end integration: exercise the full runloop against a synthesized
 * 64 KiB all-NOP ROM and verify the --run-ms flag causes a clean,
 * deterministic exit. Intended as a guardrail for restructuring work in
 * src/runloop.c so that the public emu_host_runloop() surface stays
 * callable and terminating.
 *
 * Uses a synthesized zero-filled ROM rather than altaid05.rom because
 * the real ROM is not distributed with the repo.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "test-runner.h"
#include "test-helper-system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_zero_rom(const char *path)
{
	FILE *f;
	unsigned char zero = 0;
	size_t i;

	f = fopen(path, "wb");
	if (!f)
		return -1;
	for (i = 0; i < 65536; i++) {
		if (fwrite(&zero, 1, 1, f) != 1) {
			fclose(f);
			return -1;
		}
	}
	return fclose(f);
}

static int make_temp_rom(char *out, size_t cap)
{
	int fd;

	if (!out || cap < 32)
		return -1;
	snprintf(out, cap, "/tmp/altaid-e2e-rom-XXXXXX");
	fd = mkstemp(out);
	if (fd < 0)
		return -1;
	close(fd);
	return write_zero_rom(out);
}

static char *test_runloop_run_ms_exits_cleanly(void)
{
	char rom_path[64];
	char cmd[512];
	int rc;

	if (make_temp_rom(rom_path, sizeof(rom_path)) != 0)
		return "failed to create temp ROM";

	/*
	 * Short --run-ms keeps the test fast. --turbo bypasses wall-clock
	 * throttling so CPU time converges to the deadline quickly.
	 * </dev/null ensures stdin_poll_input sees EOF without blocking.
	 */
	snprintf(cmd, sizeof(cmd),
		"./altaid-emu %s --headless --run-ms 100 --turbo </dev/null",
		rom_path);

	rc = helper_system_status(cmd);
	unlink(rom_path);

	_it_should(
		"exit 0 after --run-ms elapses with a synthesized ROM",
		0 == rc
	);

	return NULL;
}

static char *test_runloop_run_ms_zero_treated_as_unlimited(void)
{
	char rom_path[64];
	char cmd[512];
	int rc;

	if (make_temp_rom(rom_path, sizeof(rom_path)) != 0)
		return "failed to create temp ROM";

	/*
	 * --run-ms 0 means "unlimited"; the process must then rely on the
	 * outer timeout to bound the run. Verify the binary does not exit
	 * 0 prematurely in this mode (timeout kills with status 124+128
	 * via /bin/sh signal handling, i.e. non-zero).
	 */
	snprintf(cmd, sizeof(cmd),
		"timeout 1 ./altaid-emu %s --headless --run-ms 0 --turbo </dev/null",
		rom_path);

	rc = helper_system_status(cmd);
	unlink(rom_path);

	_it_should(
		"--run-ms 0 does not self-exit (outer timeout required)",
		0 != rc
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_runloop_run_ms_exits_cleanly);
	_run_test(test_runloop_run_ms_zero_treated_as_unlimited);
	return NULL;
}
