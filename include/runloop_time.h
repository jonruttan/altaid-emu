/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_RUNLOOP_TIME_H
#define ALTAID_RUNLOOP_TIME_H

#include "emu_core.h"
#include "emu_host.h"

/*
 * If realtime mode is enabled, sleep (or wait on input) just long enough
 * to keep emulated CPU time from running ahead of wall time. No-op when
 * --turbo is in effect.
 */
void runloop_realtime_throttle(const struct EmuHost *host,
			       const struct EmuCore *core);

#endif /* ALTAID_RUNLOOP_TIME_H */
