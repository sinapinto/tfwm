/* See LICENSE file for copyright and license details. */
#ifndef MAIN_H
#define MAIN_H

#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <libsn/sn-launcher.h>
#include <stdbool.h>

#define FREE(X)                                                                \
    do {                                                                       \
        if (X) {                                                               \
            free(X);                                                           \
            X = NULL;                                                          \
        }                                                                      \
    } while (0)

#define CLEANMASK(mask) (mask & ~(numlockmask | XCB_MOD_MASK_LOCK))
#define LENGTH(X)       (int)(sizeof(X) / sizeof(X)[0])
#define ISVISIBLE(C)    ((C)->ws == selws)

#define KEY_MAX 65
#define RULE_MAX 2
#define BUTTON_MAX 2

enum action {
    MoveDown,
    MoveRight,
    MoveUp,
    MoveLeft,
    GrowHeight,
    GrowWidth,
    ShrinkHeight,
    ShrinkWidth,
    GrowBoth,
    ShrinkBoth,
    Center,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Top,
    Left,
    Bottom,
    Right,
    MouseMove,
    MouseResize,
    MaxVertical,
    MaxHorizontal,
    LastWorkspace,
    PrevWorkspace,
    NextWorkspace,
    PrevWindow,
    NextWindow
};

typedef union {
    char *com;
    enum action i;
} Arg;

typedef struct {
    const char *class;
    unsigned int workspace;
    bool *fullscreen;
    bool border;
} Rule;

typedef struct Key {
    unsigned int mod;
    xcb_keysym_t keysym;
    void (*func)(const Arg *);
    Arg arg;
} Key;

#define MAX_STATE 6
enum {
    EWMH_MAXIMIZED_VERT = (1 << 0),
    EWMH_MAXIMIZED_HORZ = (1 << 1),
    EWMH_STICKY = (1 << 2),
    EWMH_FULLSCREEN = (1 << 3),
    EWMH_DEMANDS_ATTENTION = (1 << 4),
    EWMH_ABOVE = (1 << 5)
};

typedef struct Client Client;
struct Client {
    xcb_rectangle_t geom;
    xcb_rectangle_t old_geom;
    xcb_size_hints_t size_hints;
    int32_t wm_hints;
    uint32_t ewmh_flags;
    bool noborder;
    bool can_focus;
    bool can_delete;
    xcb_window_t frame;
    Client *next;
    Client *snext;
    /* TAILQ_ENTRY(clients) next; */
    /* TAILQ_ENTRY(stack) snext; */
    xcb_window_t win;
    unsigned int ws;
    uint8_t ignore_unmap;
};

void quit(const Arg *arg);
void restart(const Arg *arg);

extern xcb_connection_t *conn;
extern xcb_screen_t *screen;
extern SnDisplay *sndisplay;
extern int scrno;
extern xcb_ewmh_connection_t *ewmh;
extern xcb_atom_t WM_DELETE_WINDOW;
extern xcb_atom_t WM_TAKE_FOCUS;
extern xcb_atom_t WM_PROTOCOLS;
extern xcb_timestamp_t last_timestamp;
extern Client *clients;
extern Client *sel;
extern Client *stack;

#endif /* MAIN_H */
