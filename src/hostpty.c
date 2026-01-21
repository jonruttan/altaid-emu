/* Ensure POSIX PTY prototypes (posix_openpt/grantpt/unlockpt/ptsname). */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "hostpty.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

/* POSIX PTY API (avoids libutil/openpty dependency). */
#include <stdlib.h>

int hostpty_open(char *slave_path, size_t slave_path_cap)
{
	int master = posix_openpt(O_RDWR | O_NOCTTY);
	if (master < 0) return -1;
	if (grantpt(master) != 0 || unlockpt(master) != 0) {
		close(master);
		return -1;
	}

	if (slave_path && slave_path_cap) {
		slave_path[0] = '\0';
		const char *name = ptsname(master);
		if (name) snprintf(slave_path, slave_path_cap, "%s", name);
	}

	return master;
}

void hostpty_make_raw(int fd)
{
	struct termios t;
	if (tcgetattr(fd, &t) == 0) {
		t.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
		t.c_oflag &= ~OPOST;
		t.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
		t.c_cflag &= ~(CSIZE|PARENB);
		t.c_cflag |= CS8;
		t.c_cc[VMIN] = 0;
		t.c_cc[VTIME] = 0;
		tcsetattr(fd, TCSANOW, &t);
	}
}

void hostpty_make_raw_nonblocking(int fd)
{
	hostpty_make_raw(fd);
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
