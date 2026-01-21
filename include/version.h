/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_VERSION_H
#define ALTAID_VERSION_H

/*
 * Emulator version.
 *
 * Keep this separate from CLI parsing so both the CLI and any library-style
 * callers can report a consistent build version.
 */
const char *altaid_emu_version(void);

#endif /* ALTAID_VERSION_H */
