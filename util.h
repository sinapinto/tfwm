/* See LICENSE file for copyright and license details. */
#ifndef UTIL_H
#define UTIL_H

#ifdef DEBUG
# define PRINTF(...)    do { printf(__VA_ARGS__); } while(0)
#else
# define PRINTF(...)
#endif

#define CLEANMASK(mask) (mask & ~(numlockmask|XCB_MOD_MASK_LOCK))
#define LENGTH(X)       (sizeof(X)/sizeof(*X))
#define MAX(X, Y)       ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y)       ((X) < (Y) ? (X) : (Y))
#define WIDTH(C)        ((C)->w + 2 * BORDER_WIDTH)
#define ISVISIBLE(C)    ((C)->ws == selws || (C)->isfixed)

void warn(const char *fmt, ...);
void err(const char *fmt, ...);

#endif

