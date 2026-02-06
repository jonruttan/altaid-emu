/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_TIMEUTIL_H
#define ALTAID_TIMEUTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Define TIMEUTIL_USE_CLOCK_GETTIME to use clock_gettime(CLOCK_MONOTONIC)
 * instead of gettimeofday().
 */
uint32_t monotonic_usec(void);
uint32_t emu_tick_to_usec(uint64_t tick, uint32_t hz);

void sleep_usec(uint32_t usec);

#endif /* ALTAID_TIMEUTIL_H */
