/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_STATEIO_H
#define ALTAID_EMU_STATEIO_H

#include "emu_core.h"

#include <stdbool.h>

/*
 * Persistence helpers.
 *
 * These routines serialize/deserialize emulator state in a stable, endian-safe
 * binary format.
 *
 * - "state" = CPU + devices + RAM + timing (ROM content is NOT saved).
 * - "ram"   = RAM banks only.
 *
 * State files are validated against the currently loaded ROM (hash), CPU Hz,
 * and baud rate, to avoid restoring an incompatible snapshot.
 */

bool stateio_save_state(const struct EmuCore *core, const char *path,
			char *err, unsigned err_cap);
bool stateio_load_state(struct EmuCore *core, const char *path,
			char *err, unsigned err_cap);

bool stateio_save_ram(const struct EmuCore *core, const char *path,
			char *err, unsigned err_cap);
bool stateio_load_ram(struct EmuCore *core, const char *path,
			char *err, unsigned err_cap);

/* Hash of the currently loaded 64 KiB ROM image, for compatibility checks. */
uint32_t stateio_rom_hash32(const struct EmuCore *core);

#endif /* ALTAID_EMU_STATEIO_H */
