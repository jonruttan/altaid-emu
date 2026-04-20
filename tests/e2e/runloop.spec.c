/* SPDX-License-Identifier: MIT */

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

#include <stdbool.h>
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

static int make_temp_blob(char *out, size_t cap, const unsigned char *data,
			  size_t len)
{
	int fd;
	ssize_t written;

	if (!out || cap < 32)
		return -1;
	snprintf(out, cap, "/tmp/altaid-e2e-blob-XXXXXX");
	fd = mkstemp(out);
	if (fd < 0)
		return -1;
	written = write(fd, data, len);
	close(fd);
	return (written == (ssize_t)len) ? 0 : -1;
}

static char *test_load_ram_at_offset_and_save_round_trip(void)
{
	unsigned char blob[32];
	unsigned char ram[524288];
	char rom_path[64];
	char blob_path[64];
	char ram_path[64];
	char cmd[1024];
	FILE *f;
	int rc;
	int i;
	bool bytes_match;
	bool surroundings_zero;

	/* Distinct pattern so we can see it survives the round-trip. */
	for (i = 0; i < (int)sizeof(blob); i++)
		blob[i] = (unsigned char)(0xA0 + i);

	if (make_temp_rom(rom_path, sizeof(rom_path)) != 0)
		return "failed to create temp ROM";
	if (make_temp_blob(blob_path, sizeof(blob_path), blob, sizeof(blob)) != 0) {
		unlink(rom_path);
		return "failed to create temp blob";
	}
	snprintf(ram_path, sizeof(ram_path), "/tmp/altaid-e2e-ram-out-%d", (int)getpid());

	/* Load 32 bytes at bank 2 addr 0x1234, run briefly, save full RAM on exit. */
	snprintf(cmd, sizeof(cmd),
		"./altaid-emu %s --headless --turbo --run-ms 1 "
		"--load ram@2.0x1234:%s --save ram:%s </dev/null",
		rom_path, blob_path, ram_path);

	rc = helper_system_status(cmd);
	if (rc != 0) {
		unlink(rom_path);
		unlink(blob_path);
		unlink(ram_path);
		return "emu returned non-zero";
	}

	f = fopen(ram_path, "rb");
	if (!f) {
		unlink(rom_path);
		unlink(blob_path);
		return "saved RAM file missing";
	}
	if (fread(ram, 1, sizeof(ram), f) != sizeof(ram)) {
		fclose(f);
		unlink(rom_path);
		unlink(blob_path);
		unlink(ram_path);
		return "saved RAM file wrong size";
	}
	fclose(f);

	/* Bank 2 at 0x1234 is flat offset 2*65536 + 0x1234 = 0x21234. */
	bytes_match = (0 == memcmp(ram + 0x21234, blob, sizeof(blob)));
	/* Bank 2 just before and after should still be zero
	 * (nothing else wrote there). */
	surroundings_zero = (0 == ram[0x21234 - 1])
			 && (0 == ram[0x21234 + sizeof(blob)]);

	unlink(rom_path);
	unlink(blob_path);
	unlink(ram_path);

	_it_should(
		"--load ram@bank.addr:blob lands at the expected flat offset",
		bytes_match
	);
	_it_should(
		"--load does not overwrite bytes outside the blob",
		surroundings_zero
	);

	return NULL;
}

static char *test_load_bad_spec_fails_at_startup(void)
{
	char rom_path[64];
	char cmd[512];
	int rc;

	if (make_temp_rom(rom_path, sizeof(rom_path)) != 0)
		return "failed to create temp ROM";

	snprintf(cmd, sizeof(cmd),
		"./altaid-emu %s --headless --turbo --run-ms 1 "
		"--load garbage </dev/null 2>/dev/null",
		rom_path);

	rc = helper_system_status(cmd);
	unlink(rom_path);

	_it_should(
		"malformed --load spec fails at CLI parse",
		0 != rc
	);

	return NULL;
}

static char *test_stdin_ctrl_p_chord_passes_through_emu(void)
{
	char rom_path[64];
	char cmd[512];
	int rc;

	if (make_temp_rom(rom_path, sizeof(rom_path)) != 0)
		return "failed to create temp ROM";

	/*
	 * Pipe Ctrl-P + 'r' plus a few serial bytes.  The zero-ROM has
	 * no observable behavior in response, but the binary must accept
	 * the stream and exit cleanly -- guards against regressions in
	 * the parser plumbing.
	 */
	snprintf(cmd, sizeof(cmd),
		"printf '\\x10rhello' | ./altaid-emu %s "
		"--headless --turbo --run-ms 50",
		rom_path);

	rc = helper_system_status(cmd);
	unlink(rom_path);

	_it_should(
		"Ctrl-P chord on stdin is accepted and emu exits cleanly",
		0 == rc
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_runloop_run_ms_exits_cleanly);
	_run_test(test_runloop_run_ms_zero_treated_as_unlimited);
	_run_test(test_load_ram_at_offset_and_save_round_trip);
	_run_test(test_load_bad_spec_fails_at_startup);
	_run_test(test_stdin_ctrl_p_chord_passes_through_emu);
	return NULL;
}
