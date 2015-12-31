/* See LICENSE file for copyright and license details. */
#ifndef UTIL_H
#define UTIL_H

#ifdef DEBUG
# include <stdio.h>
# define PRINTF(...)  do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
# define PRINTF(...)
#endif

#define FREE(X)                                                               \
        do {                                                                  \
                if (X) {                                                      \
                        free(X);                                              \
                        X = NULL;                                             \
                }                                                             \
        } while(0)

#define ROOT_EVENT_MASK    (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY                \
                            | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT            \
                            | XCB_EVENT_MASK_BUTTON_PRESS)

#define CLIENT_EVENT_MASK  (XCB_EVENT_MASK_ENTER_WINDOW                       \
                            | XCB_EVENT_MASK_PROPERTY_CHANGE)

#define POINTER_EVENT_MASK (XCB_EVENT_MASK_BUTTON_PRESS                       \
                            | XCB_EVENT_MASK_BUTTON_RELEASE                   \
                            | XCB_EVENT_MASK_BUTTON_MOTION                    \
                            | XCB_EVENT_MASK_POINTER_MOTION)

#define FRAME_EVENT_MASK   (POINTER_EVENT_MASK                                \
                            | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY              \
                            | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)

#define CLEANMASK(mask) (mask & ~(numlockmask|XCB_MOD_MASK_LOCK))
#define LENGTH(X)       (sizeof(X)/sizeof(*X))
#define MAX(X, Y)       ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y)       ((X) < (Y) ? (X) : (Y))
#define WIDTH(C)        ((C)->geom.width + 2 * border_width)
#define ISVISIBLE(C)    ((C)->ws == selws)
#define ISFULLSCREEN(C) ((C)->ewmh_flags & EWMH_FULLSCREEN)
#define ISMAXVERT(C)    ((C)->ewmh_flags & EWMH_MAXIMIZED_VERT)
#define ISMAXHORZ(C)    ((C)->ewmh_flags & EWMH_MAXIMIZED_HORZ)
#define ISABOVE(C)      ((C)->ewmh_flags & EWMH_ABOVE)
#define GRAVITY(C)      ((C->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY) ? \
                         C->size_hints.win_gravity : XCB_GRAVITY_NORTH_WEST)
void warn(const char *fmt, ...);
void err(const char *fmt, ...);
void run_program(const char *cmd);
char *skip_leading_space(char *s);

#endif

