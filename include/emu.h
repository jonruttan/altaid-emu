/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_H
#define ALTAID_EMU_H

#include <signal.h>


#include "emu_core.h"
#include "emu_host.h"

/* Convenience wrapper used by the CLI app. */
struct Emu {
	struct EmuCore	core;
	struct EmuHost	host;
};

int emu_init(struct Emu *emu, const struct Config *cfg);
void emu_shutdown(struct Emu *emu);

void emu_reset(struct Emu *emu);

int emu_run(struct Emu *emu, volatile sig_atomic_t *stop_flag,
volatile sig_atomic_t *winch_flag);

#endif /* ALTAID_EMU_H */
