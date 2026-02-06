/*
 * stateio.spec.c
 *
 * Unit tests for state I/O helpers (header validation only).
 */

#include "i8080.c"
#include "serial.c"
#include "cassette.c"
#include "altaid_hw.c"
#include "emu_core.c"
#include "stateio.c"

#include "test-runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void fill_rom(AltaidHW *hw)
{
	memset(hw->rom[0], 0x11, sizeof(hw->rom[0]));
	memset(hw->rom[1], 0x22, sizeof(hw->rom[1]));
}

static int make_temp_path(char *out, size_t cap)
{
	int fd;

	if (!out || cap < 32)
		return -1;

	snprintf(out, cap, "/tmp/altaid-test-XXXXXX");
	fd = mkstemp(out);
	if (fd < 0)
		return -1;
	close(fd);
	return 0;
}

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

static char *test_stateio_state_roundtrip(void)
{
	struct EmuCore core1;
	struct EmuCore core2;
	char path[64];
	char err[256];

	emu_core_init(&core1, 2000000u, 9600u);
	fill_rom(&core1.hw);

	core1.cpu.a = 0x12;
	core1.cpu.b = 0x34;
	core1.cpu.pc = 0x5678;
	core1.cpu.sp = 0x9abcu;
	core1.cpu.z = true;
	core1.cpu.s = true;
	core1.cpu.p = false;
	core1.cpu.cy = true;
	core1.cpu.ac = true;
	core1.cpu.inte = true;
	core1.cpu.ei_pending = true;
	core1.cpu.halted = true;

	core1.ser.tick = 123456u;
	core1.ser.rx_q[0] = 0x55u;
	core1.ser.rx_qt = 1u;

	core1.hw.ram[3][0x1234] = 0x5au;
	core1.hw.ram_bank = 3u;

	core1.cas_attached = true;
	core1.cas.attached = true;

	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";

	_it_should(
		"save state to temp file",
		true == stateio_save_state(&core1, path, err, sizeof(err))
	);

	emu_core_init(&core2, 2000000u, 9600u);
	fill_rom(&core2.hw);

	_it_should(
		"load state from temp file",
		true == stateio_load_state(&core2, path, err, sizeof(err))
	);

	_it_should(
		"round-trip key cpu and ram fields",
		0x12 == core2.cpu.a
		&& 0x34 == core2.cpu.b
		&& 0x5678 == core2.cpu.pc
		&& 0x9abcu == core2.cpu.sp
		&& true == core2.cpu.z
		&& true == core2.cpu.s
		&& false == core2.cpu.p
		&& true == core2.cpu.cy
		&& true == core2.cpu.ac
		&& true == core2.cpu.inte
		&& true == core2.cpu.ei_pending
		&& true == core2.cpu.halted
		&& 123456u == core2.ser.tick
		&& 0x55u == core2.ser.rx_q[0]
		&& 1u == core2.ser.rx_qt
		&& 0x5au == core2.hw.ram[3][0x1234]
		&& 3u == core2.hw.ram_bank
		&& true == core2.cas_attached
		&& true == core2.cas.attached
	);

	unlink(path);
	return NULL;
}

static char *test_stateio_ram_roundtrip(void)
{
	struct EmuCore core1;
	struct EmuCore core2;
	char path[64];
	char err[256];

	emu_core_init(&core1, 2000000u, 9600u);
	fill_rom(&core1.hw);
	core1.hw.ram[1][0x2000] = 0xaa;
	core1.hw.ram[7][0xff00] = 0x77;

	if (make_temp_path(path, sizeof(path)) != 0)
		return "mkstemp() failed";

	_it_should(
		"save ram to temp file",
		true == stateio_save_ram(&core1, path, err, sizeof(err))
	);

	emu_core_init(&core2, 2000000u, 9600u);
	fill_rom(&core2.hw);
	core2.cpu.a = 0x99;

	_it_should(
		"load ram from temp file",
		true == stateio_load_ram(&core2, path, err, sizeof(err))
	);

	_it_should(
		"ram round-trip preserves cpu state",
		0xaa == core2.hw.ram[1][0x2000]
		&& 0x77 == core2.hw.ram[7][0xff00]
		&& 0x99 == core2.cpu.a
	);

	unlink(path);
	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_state_header_bad_magic);
	_run_test(test_state_header_bad_version);
	_run_test(test_ram_header_bad_magic);
	_run_test(test_stateio_state_roundtrip);
	_run_test(test_stateio_ram_roundtrip);

	return NULL;
}
