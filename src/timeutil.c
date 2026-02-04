/* SPDX-License-Identifier: MIT */

/* For clock_gettime()/nanosleep() in strict C99 builds. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "timeutil.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

uint64_t monotonic_ns(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}

	return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint64_t emu_tick_to_ns(uint64_t tick, uint32_t hz)
{
	uint64_t q;
	uint64_t r;

	/* tick is in CPU cycles (t-states). */
	if (hz == 0) {
		return 0;
	}

	/*
	* Avoid overflowing tick*1e9 in 64-bit by splitting into quotient and
	* remainder: (tick/hz)*1e9 + (tick%hz)*1e9/hz.
	*/
	q = tick / (uint64_t)hz;
	r = tick % (uint64_t)hz;

	return q * 1000000000ull + (r * 1000000000ull) / (uint64_t)hz;
}

void sleep_ns(uint64_t ns)
{
	struct timespec req;

	if (ns == 0) {
		return;
	}

	req.tv_sec = (time_t)(ns / 1000000000ull);
	req.tv_nsec = (long)(ns % 1000000000ull);

	while (nanosleep(&req, &req) == -1 && errno == EINTR)
	;
}

int write_full(int fd, const void *buf, size_t len)
{
	const uint8_t *p;
	size_t left;

	if (fd < 0) {
		return -1;
	}

	if ( ! buf || len == 0) {
		return 0;
	}

	p = buf;
	left = len;
	while (left) {
		ssize_t n = write(fd, p, left);

		if (n > 0) {
			p += (size_t)n;
			left -= (size_t)n;
			continue;
		}

		if (n < 0 && errno == EINTR) {
			continue;
		}

		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			/* Avoid a busy-spin if the fd is non-blocking. */
			sleep_ns(1000000ull);
			continue;
		}

		return -1;
	}

	return 0;
}

void sleep_or_wait_input_ns(uint64_t ns, bool use_pty, int pty_fd, bool headless)
{
	fd_set rfds;
	int maxfd = -1;
	struct timeval tv;

	if (ns == 0) {
		return;
	}

	FD_ZERO(&rfds);

	if ( ! headless) {
		FD_SET(0, &rfds);
		maxfd = 0;
	}

	if (use_pty && pty_fd >= 0) {
		FD_SET(pty_fd, &rfds);

		if (pty_fd > maxfd) {
			maxfd = pty_fd;
		}
	}

	tv.tv_sec = (time_t)(ns / 1000000000ull);
	tv.tv_usec = (suseconds_t)((ns % 1000000000ull) / 1000ull);

	if (maxfd < 0) {
		sleep_ns(ns);
		return;
	}

	/* select() wakes early on readiness or when interrupted by a signal. */
	(void)select(maxfd + 1, &rfds, NULL, NULL, &tv);
}
