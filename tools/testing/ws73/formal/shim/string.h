/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef WS73_FORMAL_STRING_H
#define WS73_FORMAL_STRING_H
#include <stddef.h>
/* Keep CBMC's builtin memset model independent of host multilib headers. */
void *memset(void *ptr, int value, size_t size);
#endif
