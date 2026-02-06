/*
 * stateio.spec.c
 *
 * Unit tests for state I/O helpers (header validation only).
 */

#include "cassette.c"
#include "stateio.c"

#include "test-runner.h"

#include <stdio.h>
#include <string.h>

static void write_u32le_buf(unsigned char *dst, uint32_t v)
{
	dst[0] = (unsigned char)(v & 0xffu);
	dst[1] = (unsigned char)((v >> 8) & 0xffu);
	dst[2] = (unsigned char)((v >> 16) & 0xffu);
	dst[3] = (unsigned char)((v >> 24) & 0xffu);
}

static FILE *mem_open(unsigned char *buf, size_t len)
{
	return fmemopen(buf, len, "rb");
}

static char *test_state_header_bad_magic(void)
{
	unsigned char buf[8 + 16];
	uint32_t ver = 0;
	uint32_t rom = 0;
	uint32_t cpu = 0;
	uint32_t baud = 0;
	FILE *f;
	bool ok;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, "BADSIGN!", 8);

	f = mem_open(buf, sizeof(buf));
	if (!f)
		return "fmemopen() failed";

	ok = read_header(f, k_state_magic, &ver, &rom, &cpu, &baud);
	fclose(f);

	_it_should(
		"reject bad state magic",
		false == ok
	);

	return NULL;
}

static char *test_state_header_bad_version(void)
{
	unsigned char buf[8 + 16];
	uint32_t ver = 0;
	uint32_t rom = 0;
	uint32_t cpu = 0;
	uint32_t baud = 0;
	FILE *f;
	bool ok;
	bool version_ok;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, k_state_magic, 8);
	write_u32le_buf(buf + 8, STATEIO_VER + 1u);
	write_u32le_buf(buf + 12, 0x11111111u);
	write_u32le_buf(buf + 16, 2000000u);
	write_u32le_buf(buf + 20, 9600u);

	f = mem_open(buf, sizeof(buf));
	if (!f)
		return "fmemopen() failed";

	ok = read_header(f, k_state_magic, &ver, &rom, &cpu, &baud);
	fclose(f);

	version_ok = ok && (ver == STATEIO_VER);

	_it_should(
		"detect unsupported state version",
		false == version_ok
	);

	return NULL;
}

static char *test_ram_header_bad_magic(void)
{
	unsigned char buf[8 + 16];
	uint32_t ver = 0;
	uint32_t rom = 0;
	uint32_t cpu = 0;
	uint32_t baud = 0;
	FILE *f;
	bool ok;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, "BADDRAM!", 8);

	f = mem_open(buf, sizeof(buf));
	if (!f)
		return "fmemopen() failed";

	ok = read_header(f, k_ram_magic, &ver, &rom, &cpu, &baud);
	fclose(f);

	_it_should(
		"reject bad ram magic",
		false == ok
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_state_header_bad_magic);
	_run_test(test_state_header_bad_version);
	_run_test(test_ram_header_bad_magic);

	return NULL;
}
