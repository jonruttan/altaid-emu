/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_IO_SYS_H
#define ALTAID_IO_SYS_H

/*
 * System I/O indirection.
 *
 * Production code uses real host syscalls.
 *
 * Unit tests may override `ALTAID_IO_READ` / `ALTAID_IO_WRITE` before
 * including any `.c` that includes this header (typically via
 * `#include "io.c"` or `#include "runloop.c"` in a spec file).
 */

#include <sys/types.h>
#include <unistd.h>

#ifndef ALTAID_IO_READ
#define ALTAID_IO_READ  read
#endif

#ifndef ALTAID_IO_WRITE
#define ALTAID_IO_WRITE write
#endif

#endif /* ALTAID_IO_SYS_H */
