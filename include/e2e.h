/*
 * e2e.h
 *
 * Small helpers for end-to-end tests.
 *
 * These helpers are intended for tests only. They wrap system(3) and
 * normalize its return value to a process-style exit status.
 */

#ifndef ALTAID_EMU_E2E_H
#define ALTAID_EMU_E2E_H

#ifdef TESTS

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/*
 * Convert system(3) return value to a conventional exit status.
 *
 * Returns:
 *  - [0..255] for normal exits
 *  - 128 + signal for signaled termination
 *  - 127 for other/unknown wait statuses
 *  - 126 for system(3) failure (cannot invoke shell)
 */
static inline int e2e_system_status(const char *cmd)
{
	int rc;

	if (!cmd)
		return 126;

	rc = system(cmd);
	if (rc == -1)
		return 126;

	if (WIFEXITED(rc))
		return WEXITSTATUS(rc);
	if (WIFSIGNALED(rc))
		return 128 + WTERMSIG(rc);

	return 127;
}

static inline int e2e_system_status_quiet(const char *cmd)
{
	char buf[1024];
	int n;

	if (!cmd)
		return 126;

	/*
	 * system() runs via /bin/sh -c; simple redirection is fine here.
	 * Keep this helper small and predictable.
	 */
	n = snprintf(buf, sizeof(buf), "%s >/dev/null 2>&1", cmd);
	if (n < 0 || (size_t)n >= sizeof(buf))
		return 126;

	return e2e_system_status(buf);
}

#endif /* TESTS */

#endif /* ALTAID_EMU_E2E_H */
