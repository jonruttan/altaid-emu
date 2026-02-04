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

/*
 * Best-effort write() that retries short writes and EINTR.
 *
 * Returns 0 on success, -1 on error.
 */
int write_full(int fd, const void *buf, size_t len);

/*
 * Sleep for up to @us, but wake early when stdin/pty have data.
 *
 * Intended for real-time throttling: avoid busy waits while staying responsive
 * to user input.
 */
void sleep_or_wait_input_usec(uint32_t usec, bool use_pty, int pty_fd, bool headless);

#endif /* ALTAID_TIMEUTIL_H */
