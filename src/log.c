/* SPDX-License-Identifier: MIT */

#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static FILE *g_logf;
static bool g_quiet;
static bool g_flush_each_write = true;

void log_set_quiet(bool quiet)
{
	g_quiet = quiet;
}

int log_open(const char *path, bool quiet, bool flush_each_write)
{
	log_set_quiet(quiet);
	g_flush_each_write = flush_each_write;

	if (!path)
	return 0;

	g_logf = fopen(path, "a");
	if (!g_logf) {
		fprintf(stderr, "Failed to open log file '%s': %s\n",
		path, strerror(errno));
		return -1;
	}

	/* Default behavior preserves pre-existing flush-on-write semantics. */
	if (g_flush_each_write)
	setvbuf(g_logf, NULL, _IONBF, 0);
	else
	setvbuf(g_logf, NULL, _IOLBF, 0);

	return 0;
}

void log_close(void)
{
	if (g_logf) {
		fclose(g_logf);
		g_logf = NULL;
	}
}

void log_vprintf(const char *fmt, va_list ap)
{
	if (g_quiet)
	return;

	if (g_logf) {
		vfprintf(g_logf, fmt, ap);
		if (g_flush_each_write)
		fflush(g_logf);
		return;
	}

	vfprintf(stderr, fmt, ap);
}

void log_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vprintf(fmt, ap);
	va_end(ap);
}
