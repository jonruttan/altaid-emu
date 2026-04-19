/* SPDX-License-Identifier: MIT */

#include "emu.h"

#include "emu_core.h"
#include "emu_host.h"
#include "stateio.h"
#include "log.h"

#include <stdio.h>
#include <string.h>

int emu_init(struct Emu *emu, const struct Config *cfg)
{
	char err[256];
	if (!emu || !cfg) return -1;

	memset(emu, 0, sizeof(*emu));

	emu_core_init(&emu->core, cfg->cpu_hz, cfg->baud);
	if (!emu_core_load_rom64k(&emu->core, cfg->rom_path)) return -1;

	/* Apply --load specs in the order given; later loads overwrite overlapping
	 * regions. */
	for (unsigned i = 0; i < cfg->load_count; i++) {
		const struct IoSpec *s = &cfg->load_specs[i];
		bool ok = false;

		if (s->kind == IO_SPEC_STATE) {
			ok = stateio_load_state(&emu->core, s->path,
						err, sizeof(err));
		} else {
			uint32_t off = (uint32_t)s->bank * 0x10000u + s->addr;
			ok = stateio_load_ram(&emu->core, s->path, off,
					      err, sizeof(err));
		}
		if (!ok) {
			log_printf("load failed (%s): %s\n", s->path, err);
			return -1;
		}
	}

	if (emu_host_init(&emu->host, &emu->core, cfg) < 0) return -1;

	return 0;
}

void emu_shutdown(struct Emu *emu)
{
	if (!emu) return;
	emu_host_shutdown(&emu->host, &emu->core);
}

void emu_reset(struct Emu *emu)
{
	if (!emu) return;
	emu_core_reset(&emu->core);
	emu_host_epoch_reset(&emu->host, &emu->core);
}

int emu_run(struct Emu *emu, volatile sig_atomic_t *stop_flag,
volatile sig_atomic_t *winch_flag)
{
	return emu_host_runloop(&emu->host, &emu->core, stop_flag, winch_flag);
}
