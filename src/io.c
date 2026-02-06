/* SPDX-License-Identifier: MIT */

/* For select() in strict C99 builds. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "io.h"

#include "timeutil.h"

#include <errno.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

int write_full(int fd, const void *buf, size_t len)
{
	const uint8_t *p;
	size_t left;

	if (fd < 0) {
		return -1;
	}

	if (!buf || len == 0) {
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
			sleep_usec(1000u);
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

	if (!headless) {
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
