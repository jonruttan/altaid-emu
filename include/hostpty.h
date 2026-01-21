/* SPDX-License-Identifier: MIT */

#ifndef ALTAID_EMU_HOSTPTY_H
#define ALTAID_EMU_HOSTPTY_H

#include <stddef.h>

int hostpty_open(char *slave_path, size_t slave_path_cap);
void hostpty_make_raw(int fd);
void hostpty_make_raw_nonblocking(int fd);

#endif /* ALTAID_EMU_HOSTPTY_H */
