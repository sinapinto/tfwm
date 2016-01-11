/* See LICENSE file for copyright and license details. */
#ifndef LOG_H
#define LOG_H

#ifdef DEBUG
#include <stdio.h>
#define PRINTF(...)                                                            \
    do {                                                                       \
        fprintf(stderr, __VA_ARGS__);                                          \
    } while (0)
#else
#define PRINTF(...)
#endif

void warn(const char *fmt, ...);
void err(const char *fmt, ...);

#endif
