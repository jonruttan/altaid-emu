/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_STATEIO_H
#define ALTAID_EMU_STATEIO_H

#include "emu_core.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Persistence helpers.
 *
 * - "state" = CPU + devices + RAM + timing (ROM content is NOT saved).
 *             Self-describing with magic + version for format evolution.
 * - "ram"   = raw bytes.  Save writes the full 512 KiB of RAM; load reads
 *             a file of any size into the given bank at the given offset.
 */

bool stateio_save_state(const struct EmuCore *core, const char *path,
			char *err, unsigned err_cap);
bool stateio_load_state(struct EmuCore *core, const char *path,
			char *err, unsigned err_cap);

bool stateio_save_ram(const struct EmuCore *core, const char *path,
			char *err, unsigned err_cap);
bool stateio_load_ram(struct EmuCore *core, const char *path,
			uint32_t flat_offset,
			char *err, unsigned err_cap);

#endif /* ALTAID_EMU_STATEIO_H */
