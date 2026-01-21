/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_CORE_H
#define ALTAID_EMU_CORE_H

#include "altaid_hw.h"
#include "cassette.h"
#include "i8080.h"
#include "serial.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EMU_TXBUF_SIZE 4096

/*
* EmuCore is the deterministic emulation state:
* CPU + memory map + devices + tick-based timing.
*
* It MUST NOT touch host resources (stdio, PTYs, termios, select(), wall clock).
*/
struct EmuCoreConfig {
	uint32_t	cpu_hz;
	uint32_t	baud;
};

struct EmuCore {
	struct EmuCoreConfig	cfg;

	I8080		cpu;
	I8080Bus	bus;
	AltaidHW	hw;
	SerialDev	ser;

	Cassette	cas;
	bool		cas_attached;

	uint64_t	timer_period;
	uint64_t	next_timer_tick;

	/* Decoded TX bytes from the emulated serial bitstream. */
	uint8_t		tx_buf[EMU_TXBUF_SIZE];
	uint32_t	tx_r;
	uint32_t	tx_w;
};

void emu_core_init(struct EmuCore *core, uint32_t cpu_hz, uint32_t baud);
bool emu_core_load_rom64k(struct EmuCore *core, const char *rom_path);

/* Reset emulated machine state. Does not clear RAM contents. */
void emu_core_reset(struct EmuCore *core);

/* Run the core for at most batch_cycles worth of emulated ticks. */
void emu_core_run_batch(struct EmuCore *core, uint64_t batch_cycles);

/* Pop decoded TX bytes produced by the emulated machine. */
size_t emu_core_tx_pop(struct EmuCore *core, uint8_t *dst, size_t cap);

#endif /* ALTAID_EMU_CORE_H */
