/* SPDX-License-Identifier: MIT */

#include "runloop_time.h"

#include "io.h"
#include "timeutil.h"

#include <stdint.h>

void runloop_realtime_throttle(const struct EmuHost *host,
			       const struct EmuCore *core)
{
	uint32_t now_wall;
	uint32_t emu_usec;
	uint32_t wall_elapsed;
	uint32_t delta;

	if (!host->cfg.realtime) return;

	now_wall = monotonic_usec();
	wall_elapsed = now_wall - host->wall_start_usec;
	emu_usec = emu_tick_to_usec(core->ser.tick - host->emu_start_tick,
				    core->cfg.cpu_hz);
	if (emu_usec <= wall_elapsed) return;

	delta = emu_usec - wall_elapsed;
	sleep_or_wait_input_usec(delta, host->cfg.use_pty, host->pty_fd,
		host->cfg.headless);
}
