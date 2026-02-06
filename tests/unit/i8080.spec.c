/*
 * i8080.spec.c
 *
 * Unit tests for a small opcode micro-test set.
 */

#include "i8080.c"

#include "test-runner.h"

#include <string.h>

typedef struct {
	uint8_t mem[65536];
	uint8_t last_out_port;
	uint8_t last_out_val;
	bool out_seen;
} TestBus;

static uint8_t test_mem_read(I8080Bus *bus, uint16_t addr)
{
	TestBus *tb = bus->user;
	return tb->mem[addr];
}

static void test_mem_write(I8080Bus *bus, uint16_t addr, uint8_t v)
{
	TestBus *tb = bus->user;
	tb->mem[addr] = v;
}

static uint8_t test_io_in(I8080Bus *bus, uint8_t port)
{
	(void)bus;
	(void)port;
	return 0;
}

static void test_io_out(I8080Bus *bus, uint8_t port, uint8_t v)
{
	TestBus *tb = bus->user;
	tb->last_out_port = port;
	tb->last_out_val = v;
	tb->out_seen = true;
}

static void init_bus(I8080Bus *bus, TestBus *tb)
{
	memset(tb, 0, sizeof(*tb));
	memset(bus, 0, sizeof(*bus));
	bus->user = tb;
	bus->mem_read = test_mem_read;
	bus->mem_write = test_mem_write;
	bus->io_in = test_io_in;
	bus->io_out = test_io_out;
}

static char *test_nop_increments_pc(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	tb.mem[0x0000] = 0x00; /* NOP */

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"nop increments pc",
		1u == cpu.pc
	);

	return NULL;
}

static char *test_mvi_b_sets_register(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	tb.mem[0x0000] = 0x06; /* MVI B, d8 */
	tb.mem[0x0001] = 0x42;

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"mvi b loads immediate",
		0x42 == cpu.b
		&& 2u == cpu.pc
	);

	return NULL;
}

static char *test_inr_preserves_cy(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	cpu.b = 0x0f;
	cpu.cy = true;
	tb.mem[0x0000] = 0x04; /* INR B */

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"inr sets flags and preserves cy",
		0x10 == cpu.b
		&& true == cpu.ac
		&& false == cpu.z
		&& false == cpu.s
		&& false == cpu.p
		&& true == cpu.cy
	);

	return NULL;
}

static char *test_dcr_sets_flags(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	cpu.b = 0x10;
	cpu.cy = false;
	tb.mem[0x0000] = 0x05; /* DCR B */

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"dcr updates flags",
		0x0f == cpu.b
		&& true == cpu.ac
		&& false == cpu.z
		&& false == cpu.s
		&& true == cpu.p
		&& false == cpu.cy
	);

	return NULL;
}

static char *test_add_b_updates_accumulator(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	cpu.a = 0x10;
	cpu.b = 0x22;
	tb.mem[0x0000] = 0x80; /* ADD B */

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"add b updates a and flags",
		0x32 == cpu.a
		&& false == cpu.cy
		&& false == cpu.ac
		&& false == cpu.z
		&& false == cpu.s
		&& false == cpu.p
	);

	return NULL;
}

static char *test_mvi_m_writes_memory(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	cpu.h = 0x20;
	cpu.l = 0x00;
	tb.mem[0x0000] = 0x36; /* MVI M, d8 */
	tb.mem[0x0001] = 0x5a;

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"mvi m writes to memory at hl",
		0x5a == tb.mem[0x2000]
	);

	return NULL;
}

static char *test_jmp_sets_pc(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	tb.mem[0x0000] = 0xc3; /* JMP 0x1234 */
	tb.mem[0x0001] = 0x34;
	tb.mem[0x0002] = 0x12;

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"jmp loads target pc",
		0x1234 == cpu.pc
	);

	return NULL;
}

static char *test_hlt_sets_halted(void)
{
	I8080 cpu;
	I8080Bus bus;
	TestBus tb;

	i8080_reset(&cpu);
	init_bus(&bus, &tb);

	tb.mem[0x0000] = 0x76; /* HLT */

	(void)i8080_step(&cpu, &bus);

	_it_should(
		"hlt sets halted flag",
		true == cpu.halted
	);

	return NULL;
}

static char *run_tests(void)
{
	_run_test(test_nop_increments_pc);
	_run_test(test_mvi_b_sets_register);
	_run_test(test_inr_preserves_cy);
	_run_test(test_dcr_sets_flags);
	_run_test(test_add_b_updates_accumulator);
	_run_test(test_mvi_m_writes_memory);
	_run_test(test_jmp_sets_pc);
	_run_test(test_hlt_sets_halted);

	return NULL;
}
