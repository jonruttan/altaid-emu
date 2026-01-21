/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_LOG_H
#define ALTAID_LOG_H

#include <stdarg.h>
#include <stdbool.h>

int log_open(const char *path, bool quiet, bool flush_each_write);
void log_close(void);
void log_printf(const char *fmt, ...);
void log_vprintf(const char *fmt, va_list ap);

/* When true, suppress non-essential messages. */
void log_set_quiet(bool quiet);

#endif /* ALTAID_LOG_H */
