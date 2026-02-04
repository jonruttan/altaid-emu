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

static uint32_t monotonic_now_usec(void)
{
#ifdef TIMEUTIL_USE_GETTIMEOFDAY
	struct timeval tv;

	if (gettimeofday(&tv, NULL) != 0) {
		return 0;
	}

	return (uint32_t)((uint64_t)tv.tv_sec * 1000000ull + (uint64_t)tv.tv_usec);
#else
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return 0;
	}

	return (uint32_t)((uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull);
#endif
}

uint32_t monotonic_usec(void)
{
	static uint32_t base_usec;
	static bool base_init = false;
	uint32_t now_usec;

	now_usec = monotonic_now_usec();
	if (!base_init && now_usec != 0) {
		base_usec = now_usec;
		base_init = true;
	}
	if (!base_init) {
		return 0;
	}

	return now_usec - base_usec;
}

uint32_t emu_tick_to_usec(uint64_t tick, uint32_t hz)
{
	uint64_t q;
	uint64_t r;

	/* tick is in CPU cycles (t-states). */
	if (hz == 0) {
		return 0;
	}

	/*
	 * Avoid overflowing tick*1e6 in 64-bit by splitting into quotient and
	 * remainder: (tick/hz)*1e6 + (tick%hz)*1e6/hz.
	 */
	q = tick / (uint64_t)hz;
	r = tick % (uint64_t)hz;

	return (uint32_t)(q * 1000000ull + (r * 1000000ull) / (uint64_t)hz);
}

void sleep_usec(uint32_t usec)
{
	struct timespec req;

	if (usec == 0) {
		return;
	}

	req.tv_sec = (time_t)(usec / 1000000u);
	req.tv_nsec = (long)((usec % 1000000u) * 1000u);

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
			sleep_usec(1000ull);
			continue;
		}

		return -1;
	}

	return 0;
}

void sleep_or_wait_input_usec(uint32_t usec, bool use_pty, int pty_fd, bool headless)
{
	fd_set rfds;
	int maxfd = -1;
	struct timeval tv;

	if (usec == 0) {
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

	tv.tv_sec = (time_t)(usec / 1000000u);
	tv.tv_usec = (suseconds_t)(usec % 1000000u);

	if (maxfd < 0) {
		sleep_usec(usec);
		return;
	}

	/* select() wakes early on readiness or when interrupted by a signal. */
	(void)select(maxfd + 1, &rfds, NULL, NULL, &tv);
}
