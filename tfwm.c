/*
* The MIT License (MIT)
*
* Copyright (c) 2015 Sina Pinto
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <signal.h>
#include <sys/wait.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <X11/keysym.h>

#define DEBUG

#ifdef DEBUG
#   define PDEBUG(...) \
        do { fprintf(stderr, "tfwm: ");fprintf(stderr, __VA_ARGS__); } while(0)
#else
#   define PDEBUG(...)
#endif

#define Mod1Mask                XCB_MOD_MASK_1
#define Mod4Mask                XCB_MOD_MASK_4
#define ShiftMask               XCB_MOD_MASK_SHIFT
#define ControlMask             XCB_MOD_MASK_CONTROL

#define CLEANMASK(mask)         ((mask & ~0x80))
#define LENGTH(X)               (sizeof(X)/sizeof(*X))
#define MAX(X, Y)               ((X) > (Y) ? (X) : (Y))

typedef union {
    const char **com;
    const int8_t i;
} Arg;

typedef struct Key {
    unsigned int mod;
    xcb_keysym_t keysym;
    void (*function)(const Arg *);
    const Arg arg;
} Key;

typedef struct Client Client;
struct Client{
    xcb_drawable_t id;
    int x, y; int width, height;
    int oldx, oldy; int oldw, oldh;
    int base_width, base_height, min_width, min_height;
    bool is_maximized;
    bool is_maximized_vert;
    bool is_centered;
    bool dontcenter;
    Client *next;
    Client *prev;
    xcb_window_t win;
};

typedef struct Workspace {
    Client *head;
    Client *current;
} Workspace;

/* globals */
static void (*handler[XCB_NO_OPERATION]) (xcb_generic_event_t *ev);
static xcb_connection_t *conn;
static xcb_screen_t *screen;
static xcb_ewmh_connection_t *ewmh;
static Client *head;
static Client *current;
static int current_workspace;
static int previous_workspace;
static int scrno;
static bool running = true;

static char *WM_ATOM_NAMES[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW",
    "WM_STATE", "_NET_SUPPORTED", "_NET_CURRENT_DESKTOP",
    "_NET_NUMBER_OF_DESKTOPS", "_NET_WM_DESKTOP", "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_STATE", "_NET_ACTIVE_WINDOW" };

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_STATE, NET_SUPPORTED,
    NET_CURRENT_DESKTOP, NET_NUMBER_OF_DESKTOPS, NET_WM_DESKTOP, NET_FULLSCREEN,
    NET_WM_STATE, NET_ACTIVE, ATOM_COUNT };

static xcb_atom_t wmatoms[ATOM_COUNT];

/* prototypes */
static void get_geom(xcb_drawable_t win, int *x, int *y, int *width, int *height);
static Client *window_to_client(xcb_window_t w);
static void resize(const Arg *arg);
static void move(const Arg *arg);
static void toggle_maximize(const Arg *arg);
static void center_win(Client *c);
static void toggle_centered(const Arg *arg);
static void setup_win(xcb_window_t w);
static void change_to_previous_workspace(const Arg *arg);
static void change_workspace(const Arg *arg);
static void save_workspace(const int i);
static void select_workspace(const int i);
static void client_to_workspace(const Arg *arg);
static void vert_max_toggle(const Arg *arg);
static void add_window_to_list(xcb_window_t w);
static void remove_window(xcb_window_t w);
static void kill_current();
static void cycle_win(const Arg *arg);
static void focus_client(struct Client *c);
static void configurerequest(xcb_generic_event_t *ev);
static void destroynotify(xcb_generic_event_t *ev);
static void unmapnotify(xcb_generic_event_t *ev);
static void maprequest(xcb_generic_event_t *ev);
static void clientmessage(xcb_generic_event_t *ev);
static void keypress(xcb_generic_event_t *ev);
static void run(void);
static void grab_keys(void);
static xcb_keysym_t xcb_get_keysym(xcb_keycode_t keycode);
static xcb_keycode_t* xcb_get_keycodes(xcb_keysym_t keysym);
static void change_border_width(xcb_window_t win, int width);
static void change_border_color(xcb_window_t win, long color);
static xcb_atom_t getatom(const char *atom_name);
static void setup();
static void quit(const Arg *arg);
static void cleanup();
static void sigchld();

#include "config.h"

static Workspace workspaces[NUM_WORKSPACES];

void
get_geom(xcb_drawable_t win, int *x, int *y, int *width, int *height) {
    xcb_get_geometry_reply_t *geom;
    geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), NULL);
    if (!geom) err(EXIT_FAILURE, "geometry reply failed");
    *x = geom->x;
    *y = geom->y;
    *width = geom->width;
    *height = geom->height;
    free(geom);
}

Client *
window_to_client(xcb_window_t w) {
    if (!w) return NULL;

    Client *c;
    for (c=head; c != NULL; c=c->next)
    {
        if (w == c->win)
            return c;
    }
    return NULL;
}

/* resize the window depending on arg->i
 * i = 0 -> increase x
 * i = 1 -> increase y
 * i = 2 -> decrease x
 * i = 3 -> decrease y
 * i = 4 -> increase x and y
 * i = 5 -> decrease x and y
 */
void
resize(const Arg *arg) {
    if (current == NULL)
        return;
    if (!current->win || current->win == screen->root)
        return;

    int step = steps[1];
    if (arg->i > 3) /* resize both x and y */
    {
        if (arg->i == 4) /* grow */
        {
            current->width += step;
            current->height += step;
        }
        else /* shrink (i == 5) */
        {
            current->height = current->height-step > 0
                            ? current->height - step
                            : current->height;
            current->width  = current->width-step > 0
                            ? current->width - step
                            : current->width;
        }
    }
    else
    {
        if (arg->i == 0)
        {
            current->height = current->height+step > 0
                            ? current->height + step
                            : current->height;
        }
        if (arg->i == 1)
        {
            current->width = current->width+step > 0
                           ? current->width + step
                           : current->width;
        }
        if (arg->i == 2)
        {
            current->height = current->height-step > 0
                            ? current->height - step
                            : current->height;
        }
        if (arg->i == 3)
        {
            current->width = current->width-step > 0
                           ? current->width - step
                           : current->width;
        }
    }
    /* if the window is centered, configure its x and y */
    if (current->is_centered)
    {
        current->x = screen->width_in_pixels - (current->width + BORDER_WIDTH*2);
        current->x /= 2;
        current->y = screen->height_in_pixels - (current->height + BORDER_WIDTH*2);
        current->y /= 2;
    }

    uint32_t values[4] = { current->x,     current->y,
                           current->width, current->height };
    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_X
            | XCB_CONFIG_WINDOW_Y
            | XCB_CONFIG_WINDOW_WIDTH
            | XCB_CONFIG_WINDOW_HEIGHT, values);

    /* unmaximize (redraw border) */
    if (current->is_maximized) current->is_maximized = false;
    if (current->is_maximized_vert) current->is_maximized_vert = false;
    change_border_width(current->win, BORDER_WIDTH);

    focus_client(current);

    xcb_flush(conn);
}

/* move the window depending on arg->i
 * i=0 -> move down
 * i=1 -> move right
 * i=2 -> move up
 * i=3 -> move left
 * note: a maximized Client that is moved will keep its maximized attribute
 * so that it can still be unmaximized  -- not the case for vert maxed */
void
move(const Arg *arg) {
    if (current == NULL)
        return;
    if (!current->win || current->win == screen->root)
        return;

    int step = steps[0];

    switch (arg->i)
    {
        case 0: current->y += step; break;
        case 1: current->x += step; break;
        case 2: current->y -= step; break;
        case 3: current->x -= step; break;
    }
    uint32_t values[2];
    values[0] = current->x;
    values[1] = current->y;
    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_X
                                   | XCB_CONFIG_WINDOW_Y, values);
    /* if it was centered it no longer is */
    if (current->is_centered) current->is_centered = false;
    if (current->is_maximized_vert) current->is_maximized_vert = false;
    xcb_flush(conn);
}


void
toggle_maximize(const Arg *arg) {
    if (current == NULL) return;
    (void) arg;

    uint32_t val[5];
    val[4] = XCB_STACK_MODE_ABOVE;
    uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                  | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
                  | XCB_CONFIG_WINDOW_STACK_MODE;

    if (!current->is_maximized)
    {
        /* remove border */
        change_border_width(current->win, 0);
        /* save our dimensions */
        current->oldx = current->x;
        current->oldy = current->y;
        current->oldw = current->width;
        current->oldh = current->height;
        /* go fullscreen */
        val[0] = 0; val[1] = 0;
        val[2] = screen->width_in_pixels; val[3] = screen->height_in_pixels;

        xcb_configure_window(conn, current->win, mask, val);
        /* update our current dimensions */
        current->x = 0;
        current->y = 0;
        current->width = screen->width_in_pixels;
        current->height = screen->height_in_pixels;
        current->is_maximized = true;
    }
    else
    {
        /* switch back to the old dimensions */
        val[0] = current->oldx;
        val[1] = current->oldy;
        val[2] = current->oldw;
        val[3] = current->oldh;
        change_border_width(current->win, BORDER_WIDTH);
        xcb_configure_window(conn, current->win, mask, val);
        /* save our dimensions for later */
        current->x = current->oldx;
        current->y = current->oldy;
        current->width = current->oldw;
        current->height = current->oldh;
        current->is_maximized = false;
        focus_client(current);
    }

    long data[] = { current->is_maximized ? wmatoms[NET_FULLSCREEN]
                                          : XCB_ICCCM_WM_STATE_NORMAL };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, current->win,
                        wmatoms[NET_WM_STATE], XCB_ATOM_ATOM, 32, 1, data);
    xcb_flush(conn);
}

void
vert_max_toggle(const Arg *arg) {
    if (!current) return;
    if (current->is_maximized) return;
    (void) arg;
    uint32_t val[3];
    uint32_t mask;
    if (!current->is_maximized_vert) {
        val[0] = 0;
        val[1] = screen->height_in_pixels - 2 * BORDER_WIDTH;
        val[2] = XCB_STACK_MODE_ABOVE;
        current->oldx = current->x;
        current->oldy = current->y;
        current->oldh = current->height;
        current->oldw = current->width;
        current->y = 0;
        current->height = val[1];
        mask = XCB_CONFIG_WINDOW_Y
            | XCB_CONFIG_WINDOW_HEIGHT
            | XCB_CONFIG_WINDOW_STACK_MODE;
        current->is_maximized_vert = true;
    } else {
        current->x = current->oldx;
        current->y = current->oldy;
        current->height = current->oldh;
        current->width = current->oldw;
        val[0] = current->y;
        val[1] = current->height;
        val[2] = XCB_STACK_MODE_ABOVE;
        mask = XCB_CONFIG_WINDOW_Y
            | XCB_CONFIG_WINDOW_HEIGHT
            | XCB_CONFIG_WINDOW_STACK_MODE;
        current->is_maximized_vert = false;
    }

    xcb_configure_window(conn, current->win, mask, val);
    xcb_flush(conn);
}

void
toggle_centered(const Arg *arg) {
    if (!current) return;
    (void) arg;
    current->is_centered = current->is_centered ? false : true;
    center_win(current);
}

/* move the Client's window to the middle of the screen */
void
center_win(Client *c) {
    if (!c) return;
    c->x = screen->width_in_pixels - (c->width + BORDER_WIDTH*2);
    c->x /= 2;
    c->y = screen->height_in_pixels - (c->height + BORDER_WIDTH*2);
    c->y /= 2;
    uint32_t values[4] = { c->x, c->y };
    xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_X
            | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(conn);
}

void
change_to_previous_workspace(const Arg *arg) {
    const Arg a = { .i = previous_workspace };
    change_workspace(&a);
}

void
change_workspace(const Arg *arg) {
    if (arg->i == current_workspace || arg->i >= NUM_WORKSPACES)
        return;

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        wmatoms[NET_CURRENT_DESKTOP], XCB_ATOM_CARDINAL, 32, 1,
                        &arg->i);
    xcb_ewmh_set_current_desktop(ewmh, scrno, arg->i);

    int previous = current_workspace;
    save_workspace(current_workspace);
    /* map new windows before unmapping the current ones to avoid flickering */
    select_workspace(arg->i);
    Client *c;
    for (c=head; c != NULL; c=c->next)
        xcb_map_window(conn, c->win);

    /* unmap current workspace clients */
    select_workspace(previous);
    for (c=head; c != NULL; c=c->next)
        xcb_unmap_window(conn, c->win);
    select_workspace(arg->i);
    focus_client(current);
}

void
save_workspace(const int i) {
    workspaces[i].head = head;
    workspaces[i].current = current;
}

void
select_workspace(const int i) {
    head = workspaces[i].head;
    current = workspaces[i].current;
    previous_workspace = current_workspace;
    current_workspace = i;
}

/* move a Client to another workspace */
/* TODO: save fullscreen and centered properties */
void
client_to_workspace(const Arg *arg) {
    if (arg->i == current_workspace || current == NULL || arg->i >= NUM_WORKSPACES)
        return;

    Client *tmp = current;
    const int tmp2 = current_workspace;

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, current->win,
                    wmatoms[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, &arg->i);

    /* add Client to new workspace */
    select_workspace(arg->i);
    add_window_to_list(tmp->win);
    save_workspace(arg->i);

    /* remove Client from current workspace */
    select_workspace(tmp2);
    xcb_unmap_window(conn,tmp->win);
    remove_window(tmp->win);
    save_workspace(tmp2);
    focus_client(current);
}

/* XCB_MAP_REQUEST handler
 * set up window's mask and border and add it to the list, and map+focus it */
void
setup_win(xcb_window_t w) {
    change_border_width(w, BORDER_WIDTH);

    if (CENTER_BY_DEFAULT == 0)
        xcb_map_window(conn, w);

    add_window_to_list(w);

    /* set its workspace hint */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w, wmatoms[NET_WM_DESKTOP],
                        XCB_ATOM_CARDINAL, 32, 1,&current_workspace);
    /* set normal state */
    long data[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w, wmatoms[WM_STATE],
                        wmatoms[WM_STATE], 32, 2, data);
    focus_client(current);

    xcb_get_property_reply_t *prop_reply;
    prop_reply = xcb_get_property_reply(conn,
            xcb_get_property_unchecked(conn, 0, w, wmatoms[NET_WM_STATE], XCB_ATOM_ATOM, 0, 1), NULL);
    if (prop_reply) {
        if (prop_reply->format == 32) {
            xcb_atom_t *v = xcb_get_property_value(prop_reply);
            if (v[0] == wmatoms[NET_FULLSCREEN])
                toggle_maximize(NULL);
        }
        free(prop_reply);
    }

    xcb_flush(conn);
}

/* make the window into a Client and set it to head and current
 * get wm size hints */
void
add_window_to_list(xcb_window_t w) {
    Client *c, *t;
    if ((c = malloc(sizeof(Client))) == NULL)
        err(EXIT_FAILURE, "couldn't allocate memory");

    if (!w) return;
    PDEBUG("add window to list %d\n", w);
    c->id = w;
    c->x = c->y = c->width = c->height = c->oldx = c->oldy = c->oldw = c->oldh = 0;
    c->min_width = c->min_height = c->base_width = c->base_height = 0;
    c->is_maximized = c->is_maximized_vert = c->is_centered = c->dontcenter = false;

    if (head == NULL)
    {
        c->next = NULL;
        c->prev = NULL;
        c->win = w;
        c->is_maximized = false;
        get_geom(c->id, &c->x, &c->y, &c->width, &c->height);
        head = c;
    }
    else
    {
        for (t=head; t->prev != NULL; t=t->prev)
            ;
        c->prev = NULL;
        c->next = t;
        c->win = w;
        c->is_maximized = false;
        t->prev = c;
        get_geom(c->id, &c->x, &c->y, &c->width, &c->height);
        head = c;
    }
    if (current) change_border_color(current->win, UNFOCUS);
    current = c;

    /* size hints */
    xcb_size_hints_t h;
    xcb_icccm_get_wm_normal_hints_reply(conn,
            xcb_icccm_get_wm_normal_hints_unchecked(conn, w), &h, NULL);

    if (h.flags &XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)
    {
        if (h.min_width > 0 && h.min_width < screen->width_in_pixels) {
            c->min_width = h.min_width;
        }

        if (h.min_height > 0 && h.min_height < screen->height_in_pixels) {
            c->min_height = h.min_height;
        }
    }

    if (h.flags &XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
        c->base_width  = h.base_width;
        c->base_height = h.base_height;
    }

    if (c->width < MAX(c->min_width, c->base_width) ||
        c->height < MAX(c->min_height, c->base_height))
    {
        c->width = c->width > c->min_width ? c->width : c->min_width;
        c->height = c->height > c->min_height ? c->height : c->min_height;

        uint32_t values[2] = { c->width, c->height };
        xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_WIDTH
                            | XCB_CONFIG_WINDOW_HEIGHT, values);
    }
    /* wm state */
    long data[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, current->win,
                        wmatoms[WM_STATE], wmatoms[WM_STATE], 32, 2, data);

    save_workspace(current_workspace);
}

/* find the window in the Client list and remove the Client */
void
remove_window(xcb_window_t w) {
    Client *c;
    if ((c = window_to_client(w)) == NULL) return;

    PDEBUG("remove_window %d\n", c->win);
    if (c->prev == NULL && c->next == NULL)
    { // one Client in list
        PDEBUG("[removing head]\n");
        free(head);
        head = NULL;
        current = NULL;
        save_workspace(current_workspace);
        return;
    }
    if (c->prev == NULL)
    {               // head
        head = c->next;
        c->next->prev = NULL;
        current = c->next;
    }
    else if (c->next == NULL)
    {          // tail
        c->prev->next = NULL;
        current = c->prev;
    }
    else
    {
        c->prev->next = c->next;
        c->next->prev = c->prev;
        current = c->prev;
    }
    free(c);
    c = NULL;
    save_workspace(current_workspace);
    return;
}

void
kill_current() {
    if (NULL == current)
        return;

    /* try sending WM_DELETE_WINDOW message */
    xcb_get_property_cookie_t c;
    xcb_icccm_get_wm_protocols_reply_t reply;
    bool use_delete= false;

    c = xcb_icccm_get_wm_protocols(conn, current->win, wmatoms[WM_PROTOCOLS]);

    if (xcb_icccm_get_wm_protocols_reply(conn, c, &reply, NULL) == 1)
    {
        for (unsigned int i=0; i<reply.atoms_len; i++)
        {
            if (reply.atoms[i] == wmatoms[WM_DELETE_WINDOW])
            {
                use_delete = true;
                xcb_icccm_get_wm_protocols_reply_wipe(&reply);
                break;
            }
        }
    }

    if (use_delete)
    {
        xcb_client_message_event_t ev = {
            .response_type = XCB_CLIENT_MESSAGE,
            .format = 32,
            .sequence = 0,
            .window = current->win,
            .type = wmatoms[WM_PROTOCOLS],
            .data.data32 = { wmatoms[WM_DELETE_WINDOW], XCB_CURRENT_TIME }
        };
        xcb_send_event(conn, true, current->win,
                      XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
    }
    else
    {
        xcb_kill_client(conn, current->win);
    }

    xcb_flush(conn);
}

/* focus the prev/next window in the list */
void
cycle_win(const Arg *arg) {
    if (current == NULL || head == NULL)
        return;

    Client *c;

    if (arg->i == 1) // focus prev
    {
        if (current->prev == NULL) // at head.. prev win is tail
            for(c=head; c->next != NULL; c=c->next);
        else
            c = current->prev;
    }
    else // focus next
    {
        if (current->next == NULL) // at tail.. next win is head
        {
            if (current == head)
                return;  // this is the only Client
            c = head;
        }
        else
            c = current->next;
    }

    change_border_color(current->win, UNFOCUS);
    current = c;

    if (arg->i == 2) // focus next window without changing stacking order
    {
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                            c->win, XCB_CURRENT_TIME);
        change_border_color(c->win, FOCUS);
        xcb_flush(conn);
    }
    else
    {
        focus_client(current);
        uint32_t values[1];
        values[0] = XCB_STACK_MODE_ABOVE;
        xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_STACK_MODE,
                            values);
    }

    xcb_ewmh_set_active_window(ewmh, scrno, current->win);
}

/* focus input on window and change window's border color */
void
focus_client(struct Client *c) {
    if (NULL == current || NULL == c)
    {
        xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                            XCB_CURRENT_TIME);
        xcb_delete_property(conn, screen->root, wmatoms[NET_ACTIVE]);
        xcb_flush(conn);
        return;
    }

    uint32_t values[1];
    if (BORDER_WIDTH > 0)
    {
        values[0] = FOCUS;
        xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, values);
    }

    long data[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, c->win,
                        wmatoms[WM_STATE], wmatoms[WM_STATE], 32, 2, data);

    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win,
                        XCB_CURRENT_TIME);

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        wmatoms[NET_ACTIVE] , XCB_ATOM_WINDOW, 32, 1,&c->win);
    xcb_flush(conn);
}

void
unmapnotify(xcb_generic_event_t *ev) {
    xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
    Client *c = window_to_client(e->window);
    PDEBUG("event: Unmap Notify %d\n", e->window);
    if (e->window == screen->root) return;
    if (c)
    {
        remove_window(c->win);
        if (current)
        {
            focus_client(current);
            uint32_t values[1] = {XCB_STACK_MODE_ABOVE};
            xcb_configure_window(conn, current->win,
                    XCB_CONFIG_WINDOW_STACK_MODE, values);
        }
    }
}

void
clientmessage(xcb_generic_event_t *ev) {
    xcb_client_message_event_t *e = (xcb_client_message_event_t*)ev;
    Client *c = window_to_client(e->window);
    if (!c) return;

    if (e->type == wmatoms[NET_WM_STATE]
            && (unsigned)e->data.data32[1] == wmatoms[NET_FULLSCREEN])
    {
        /* data32[0]=1 -> maximize; data32[0]=0 -> minimize */
        if ((e->data.data32[0] == 0 && c->is_maximized) ||
                (e->data.data32[0] == 1 && !c->is_maximized))
        {
            toggle_maximize(NULL);
        }
    }
    else if (e->type == wmatoms[NET_ACTIVE])
        focus_client(c);
}

void
configurerequest(xcb_generic_event_t *ev) {
    xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
    PDEBUG("event: Confgirue Request, mask: %d\n", e->value_mask);
    Client *c = window_to_client(e->window);
    if (!c) return;
    if (c->is_maximized) return;

    unsigned int v[7];
    int i = 0;

    if (e->value_mask & XCB_CONFIG_WINDOW_X) {
        v[i++] = c->x = e->x;
        c->dontcenter = true;
    }
    if (e->value_mask & XCB_CONFIG_WINDOW_Y) {
        v[i++] = c->y = e->y;
        c->dontcenter = true;
    }
    if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) v[i++] = c->width = e->width;
    if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) v[i++] = c->height = e->height;
    /* if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) v[i++] = e->border_width; */
    /* if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) v[i++] = e->sibling; */
    /* if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) v[i++] = e->stack_mode; */
    if (c->is_centered && !c->dontcenter) center_win(c);

    xcb_configure_window(conn, e->window, e->value_mask, v);
    xcb_flush(conn);
}

void
maprequest(xcb_generic_event_t *ev) {
    PDEBUG("maprequest\n");
    xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
    setup_win(e->window);
    if (CENTER_BY_DEFAULT > 0)
    {
        toggle_centered(NULL);
        xcb_map_window(conn, e->window);
    }
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, e->window,
            XCB_CURRENT_TIME);
}

void
destroynotify(xcb_generic_event_t *ev) {
    xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;

    Client *c;
    if ((c = window_to_client(e->window)) != NULL)
    {
        remove_window(c->win);
        focus_client(current);
    }
}

void
keypress(xcb_generic_event_t *ev) {
    PDEBUG("keypress\n");
    xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
    xcb_keysym_t keysym = xcb_get_keysym(e->detail);
    for (unsigned int i=0; i<LENGTH(keys); i++)
    {
        if (keysym == keys[i].keysym &&
                CLEANMASK(keys[i].mod) == CLEANMASK(e->state) &&
                keys[i].function)
        {
            keys[i].function(&keys[i].arg);
            break;
        }
    }
}

void
run(void) {
    PDEBUG("welcome to tfwm!\n");
    xcb_generic_event_t *ev;
    while (running) {
        xcb_flush(conn);
        ev = xcb_wait_for_event(conn);
        if (handler[CLEANMASK(ev->response_type)])
            handler[CLEANMASK(ev->response_type)](ev);
        free(ev);
        if (xcb_connection_has_error(conn))
            running = false;
    }
}

void
grab_keys(void) {
    xcb_keycode_t *keycode;
    for (int i=0; i<LENGTH(keys); i++) {
        keycode = xcb_get_keycodes(keys[i].keysym);
        for (int k=0; keycode[k] != XCB_NO_SYMBOL; k++)
            xcb_grab_key(conn, 1, screen->root, keys[i].mod, keycode[k],
                         XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
}

xcb_keysym_t
xcb_get_keysym(xcb_keycode_t keycode) {
    xcb_key_symbols_t *keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        err(EXIT_FAILURE, "couldn't get keysym.");
    xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
    xcb_key_symbols_free(keysyms);
    return keysym;
}

xcb_keycode_t *
xcb_get_keycodes(xcb_keysym_t keysym) {
    xcb_key_symbols_t *keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        err(EXIT_FAILURE, "couldn't get keycode.");
    xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
    xcb_key_symbols_free(keysyms);
    return keycode;
}

void
change_border_width(xcb_window_t win, int width) {
    xcb_icccm_get_wm_class_reply_t ch;
    if (xcb_icccm_get_wm_class_reply(conn, xcb_icccm_get_wm_class(conn, win), &ch, NULL)) {
        for (int i = 0; i < LENGTH(border_blacklist); i++) {
            if (strcmp(ch.class_name, border_blacklist[i]) == 0)
                return;
        }
    }
    uint32_t value[1] = { width };
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, value);
}

void
change_border_color(xcb_window_t win, long color) {
    if (!win || BORDER_WIDTH == 0) return;
    uint32_t value[1] = { color };
    xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, value);
}

xcb_atom_t
getatom(const char *atom_name) {
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *rep;
    atom_cookie = xcb_intern_atom(conn, 0, strlen(atom_name), atom_name);
    rep = xcb_intern_atom_reply(conn, atom_cookie, NULL);
    if (NULL == rep) return 0;
    xcb_atom_t atom = rep->atom;
    free(rep);
    return atom;
}

void
setup() {
    /* clean up any zombies */
    sigchld();
    /* init screen */
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    if (!screen)
        err(EXIT_FAILURE, "couldn't find screen.");
    /* init ewmh */
    ewmh = malloc(sizeof(xcb_ewmh_connection_t));
    if (xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh), NULL) == 0)
            err(EXIT_FAILURE, "couldn't initialize ewmh.");
    /* subscribe to handler */
    unsigned int value[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY|XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
    xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(conn, screen->root, XCB_CW_EVENT_MASK, value);
    xcb_generic_error_t *error = xcb_request_check(conn, cookie);
    if (error != NULL)
        err(EXIT_FAILURE, "another window manager is running.");
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[] = { XCB_EVENT_MASK_POINTER_MOTION };
    xcb_window_t recorder = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, recorder, screen->root, 0, 0,
            screen->width_in_pixels, screen->height_in_pixels, 0,
            XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, mask, values);
    xcb_atom_t net_atoms[] = {ewmh->_NET_SUPPORTED,
                              ewmh->_NET_NUMBER_OF_DESKTOPS,
                              ewmh->_NET_CURRENT_DESKTOP,
                              ewmh->_NET_ACTIVE_WINDOW,
                              ewmh->_NET_CLOSE_WINDOW,
                              ewmh->_NET_WM_DESKTOP,
                              ewmh->_NET_WM_WINDOW_TYPE,
                              ewmh->_NET_WM_STATE,
                              ewmh->_NET_WM_STATE_FULLSCREEN};
    xcb_ewmh_set_supported(ewmh, scrno, LENGTH(net_atoms), net_atoms);
    xcb_ewmh_set_supporting_wm_check(ewmh, screen->root, recorder);
    xcb_ewmh_set_supporting_wm_check(ewmh, recorder, recorder);
    for (unsigned int i=0; i<ATOM_COUNT; i++)
        wmatoms[i] = getatom(WM_ATOM_NAMES[i]);
    static const int _WORKSPACES = NUM_WORKSPACES;
    current_workspace = previous_workspace = 0;
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        wmatoms[NET_CURRENT_DESKTOP], XCB_ATOM_CARDINAL,
                        32, 1,&current_workspace);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        wmatoms[NET_NUMBER_OF_DESKTOPS], XCB_ATOM_CARDINAL,
                        32, 1,&_WORKSPACES);
    xcb_ewmh_set_number_of_desktops(ewmh, scrno, _WORKSPACES);
    xcb_ewmh_set_current_desktop(ewmh, scrno, 0);
    focus_client(NULL);
    /* init keys */
    grab_keys();
    xcb_flush(conn);
    /* set handler */
    for (int i=0; i<XCB_NO_OPERATION; i++)
        handler[i] = NULL;
    handler[XCB_CONFIGURE_REQUEST] = configurerequest;
    handler[XCB_DESTROY_NOTIFY] = destroynotify;
    handler[XCB_UNMAP_NOTIFY] = unmapnotify;
    handler[XCB_MAP_REQUEST] = maprequest;
    handler[XCB_CLIENT_MESSAGE] = clientmessage;
    handler[XCB_KEY_PRESS] = keypress;
}

void
quit(const Arg *arg) {
    running = false;
}

void
cleanup() {
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
    xcb_ewmh_connection_wipe(ewmh);
    if (ewmh)
        free(ewmh);
    xcb_query_tree_reply_t  *query;
    xcb_window_t *c;
    if ((query = xcb_query_tree_reply(conn,xcb_query_tree(conn,screen->root),0))) {
        c = xcb_query_tree_children(query);
        for (int i = 0; i != query->children_len; ++i)
            free(window_to_client(c[i]));
        free(query);
    }
    xcb_flush(conn);
    xcb_disconnect(conn);
}

void
sigchld() {
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        err(EXIT_FAILURE, "can't install SIGCHLD handler");
    while (0 < waitpid(-1, NULL, WNOHANG));
}

int
main() {
    conn = xcb_connect(NULL, &scrno);
    if (xcb_connection_has_error(conn))
        err(EXIT_FAILURE, "xcb_connect error");
    setup();
    run();
    cleanup();
    return EXIT_SUCCESS;
}
