/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_I8080_H
#define ALTAID_EMU_I8080_H

#include <stdbool.h>
#include <stdint.h>

typedef struct I8080Bus I8080Bus;

typedef struct {
	uint8_t		a;
	uint8_t		b;
	uint8_t		c;
	uint8_t		d;
	uint8_t		e;
	uint8_t		h;
	uint8_t		l;
	uint16_t	pc;
	uint16_t	sp;

	/* flags */
	bool		z;
	bool		s;
	bool		p;
	bool		cy;
	bool		ac;

	bool		inte;
	bool		ei_pending;
	bool		halted;
} I8080;

typedef uint8_t (*i8080_mem_read_fn)(I8080Bus *bus, uint16_t addr);
typedef void (*i8080_mem_write_fn)(I8080Bus *bus, uint16_t addr, uint8_t v);
typedef uint8_t (*i8080_io_in_fn)(I8080Bus *bus, uint8_t port);
typedef void (*i8080_io_out_fn)(I8080Bus *bus, uint8_t port, uint8_t v);

struct I8080Bus {
	void			*user;
	i8080_mem_read_fn	mem_read;
	i8080_mem_write_fn	mem_write;
	i8080_io_in_fn		io_in;
	i8080_io_out_fn		io_out;
};

void i8080_reset(I8080 *cpu);

/* Execute one instruction. Returns exact 8080 t-states for that instruction. */
int i8080_step(I8080 *cpu, I8080Bus *bus);

/* Request that EI takes effect after the next instruction (8080 behavior). */
void i8080_set_ei_pending(I8080 *cpu);

/*
* Service a maskable interrupt using RST vector (0..7).
*
* The emulator should call this when INTE is true and an interrupt is pending.
*/
void i8080_intr_service(I8080 *cpu, I8080Bus *bus, uint8_t rst_vector);

#endif /* ALTAID_EMU_I8080_H */
