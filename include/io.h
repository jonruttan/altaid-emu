/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_IO_H
#define ALTAID_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#endif /* ALTAID_IO_H */
