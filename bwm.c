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

/* uncomment to print debug statements */
/*#define DEBUG*/
#define MOUSE

#ifdef DEBUG
#   define PDEBUG(...) \
        do { fprintf(stderr, "bwm: ");fprintf(stderr, __VA_ARGS__); } while(0)
#   include "events.h"
#else
#   define PDEBUG(...)
#endif

#define Mod1Mask        XCB_MOD_MASK_1
#define Mod4Mask        XCB_MOD_MASK_4
#define ShiftMask       XCB_MOD_MASK_SHIFT
#define ControlMask     XCB_MOD_MASK_CONTROL

#define CLEANMASK(mask) ((mask & ~0x80))
#define LENGTH(X)       (sizeof(X)/sizeof(*X))
#define MAX(X, Y)       ((X) > (Y) ? (X) : (Y))

typedef union {
    const char **com;   /* command */
    const int8_t i;     /* integer */
} Arg;

typedef struct key {
    unsigned int mod;               /* modifier mask(s) */
    xcb_keysym_t keysym;            /* X11 keysym */
    void (*function)(const Arg *);  /* function call */
    const Arg arg;                  /* function argument */
} key;

typedef struct client client;
struct client{
    xcb_drawable_t id;                       /* for get_geom */
    int16_t x, y; uint16_t width, height;    /* current dimensions */
    int16_t oldx, oldy; uint16_t oldw, oldh; /* for toggle_maximize */
    uint16_t base_width, base_height, min_width, min_height; /* size hints */
    bool is_maximized;                       /* was fullscreened */
    bool is_centered;                        /* center mode is active */
    bool dontcenter;
    client *next;                            /* next client in list */
    client *prev;                            /* previous client */
    xcb_window_t win;                        /* the client's window */
};

typedef struct workspace {
    client *head;        /* head of the client list */ 
    client *current;     /* current client */
} workspace;

/* globals */
xcb_connection_t *conn;         /* connection to X server */
xcb_screen_t *screen;           /* first screen by default */
xcb_ewmh_connection_t *ewmh;    /* ewmh connection */
static client *head;            /* head of window list */
static client *current;         /* current window in list */
static uint8_t current_workspace;   /* active workspace number */
static uint8_t previous_workspace;
static int def_screen;
static bool running;

static char *WM_ATOM_NAMES[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW",
    "WM_STATE", "_NET_SUPPORTED", "_NET_CURRENT_DESKTOP",
    "_NET_NUMBER_OF_DESKTOPS", "_NET_WM_DESKTOP", "_NET_WM_STATE_FULLSCREEN",
    "_NET_WM_STATE", "_NET_ACTIVE_WINDOW" };

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_STATE, NET_SUPPORTED,
    NET_CURRENT_DESKTOP, NET_NUMBER_OF_DESKTOPS, NET_WM_DESKTOP, NET_FULLSCREEN,
    NET_WM_STATE, NET_ACTIVE, ATOM_COUNT };

static xcb_atom_t wmatoms[ATOM_COUNT];

/* prototypes */
static int get_geom(xcb_drawable_t win, int16_t *x, int16_t *y,
                    uint16_t *width, uint16_t *height);
static client *window_to_client(xcb_window_t w);
static void resize(const Arg *arg);
static void move(const Arg *arg);
static void toggle_maximize(const Arg *arg);
static void center_win(client *c);
static void toggle_centered(const Arg *arg);
static void setup_win(xcb_window_t w);
static void change_to_previous_workspace(const Arg *arg);
static void change_workspace(const Arg *arg);
static void save_workspace(const uint8_t i);
static void select_workspace(const uint8_t i);
static void client_to_workspace(const Arg *arg);
static void add_window_to_list(xcb_window_t w);
static void remove_window(xcb_window_t w);
static void kill_current();
static void cycle_win(const Arg *arg);
static void focus_client(struct client *c);
static void event_loop(void);
static bool grab_keys(void);
static xcb_keysym_t xcb_get_keysym(xcb_keycode_t keycode);
static xcb_keycode_t* xcb_get_keycodes(xcb_keysym_t keysym);
static void change_border_width(xcb_window_t win, uint8_t width);
static void change_border_color(xcb_window_t win, long color);
static xcb_atom_t getatom(const char *atom_name);
static void spawn(const Arg *arg);
static void bwm_setup();
static void bwm_exit();
static void bwm_restart();
static void cleanup();
static void sighandle(int signal);
static void sigchld();

#include "config.h"

static workspace workspaces[NUM_WORKSPACES];

/* store the window dimensions in x,y,width,height */
static int get_geom(xcb_drawable_t win, int16_t *x, int16_t *y,
                    uint16_t *width, uint16_t *height)
{
    xcb_get_geometry_reply_t *geom;
    geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), NULL);
    if (!geom) err(EXIT_FAILURE, "geometry reply failed");
    *x = geom->x;
    *y = geom->y;
    *width = geom->width;
    *height = geom->height;
    free(geom);
    return 0;
}

/* iterate througth the client list and find the client with window w */
static client *window_to_client(xcb_window_t w)
{
    if (!w) return NULL;

    client *c;
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
static void resize(const Arg *arg)
{
    if (current == NULL)
        return;
    if (!current->win || current->win == screen->root)
        return;

    uint8_t step = steps[1];
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
    change_border_width(current->win, BORDER_WIDTH);

    focus_client(current);

    xcb_flush(conn); 
}

/* move the window depending on arg->i
 * i=0 -> move down
 * i=1 -> move right
 * i=2 -> move up
 * i=3 -> move left
 * note: a maximized client that is moved will keep its maximized attribute 
 * so that it can still be unmaximized */
static void move(const Arg *arg)
{
    if (current == NULL)
        return;
    if (!current->win || current->win == screen->root)
        return;

    uint8_t step = steps[0];

    switch (arg->i)
    {
        case 0: current->y += step; break;
        case 1: current->x += step; break;
        case 2: current->y -= step; break;
        case 3: current->x -= step; break;
        default: PDEBUG("unknown argument for move\n"); break;
    }
    uint32_t values[2];
    values[0] = current->x; 
    values[1] = current->y;
    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_X
                                   | XCB_CONFIG_WINDOW_Y, values);
    /* if it was centered it no longer is */
    if (current->is_centered)
        current->is_centered = false;
    xcb_flush(conn); 
}


/* fullscreen the window or unfullscreen it
 * remove the border when fullscreen 
 * parameter is unused */
static void toggle_maximize(const Arg *arg)
{
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

        /* chromium hax */
        xcb_icccm_get_wm_class_reply_t ch;
        if (xcb_icccm_get_wm_class_reply(conn, xcb_icccm_get_wm_class(conn, current->win), &ch, NULL)) {
            if (strstr(ch.class_name, "Chromium")){
                val[2] = screen->width_in_pixels + 2;
            }
        }

        PDEBUG("maximize: x: %d y: %d width: %d height: %d\n",
                val[0], val[1], val[2], val[3]);
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
        /* readd the border */
        change_border_width(current->win, BORDER_WIDTH);
        /* switch back to the old dimensions */
        val[0] = current->oldx;
        val[1] = current->oldy;
        val[2] = current->oldw;
        val[3] = current->oldh;
        xcb_icccm_get_wm_class_reply_t ch;
        if (xcb_icccm_get_wm_class_reply(conn, xcb_icccm_get_wm_class(conn, current->win), &ch, NULL)) {
            if (strstr(ch.class_name, "Chromium")){
                val[2] = screen->width_in_pixels + 2;
            }
        }
        PDEBUG("minimize: x: %d y: %d width: %d height: %d\n",
                val[0], val[1], val[2], val[3]);
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

static void toggle_centered(const Arg *arg)
{
    if (!current) return;
    (void) arg;
    current->is_centered = current->is_centered ? false : true;
    center_win(current);
}

/* move the client's window to the middle of the screen */
static void center_win(client *c)
{
    if (!c) return;

    PDEBUG("centering window\n");

    c->x = screen->width_in_pixels - (c->width + BORDER_WIDTH*2);
    c->x /= 2;
    c->y = screen->height_in_pixels - (c->height + BORDER_WIDTH*2);
    c->y /= 2;

    uint32_t values[4] = { c->x, c->y };
    xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_X
            | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(conn); 
}

static void change_to_previous_workspace(const Arg *arg)
{
    const Arg a = { .i = previous_workspace };
    change_workspace(&a);
}

static void change_workspace(const Arg *arg)
{
    if (arg->i == current_workspace || arg->i >= NUM_WORKSPACES)
        return;

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        wmatoms[NET_CURRENT_DESKTOP], XCB_ATOM_CARDINAL, 32, 1,
                        &arg->i);
	xcb_ewmh_set_current_desktop(ewmh, def_screen, arg->i);

    uint8_t previous = current_workspace;
    save_workspace(current_workspace);
    /* map new windows before unmapping the current ones to avoid flickering */
    select_workspace(arg->i);
    client *c;
    for (c=head; c != NULL; c=c->next)
        xcb_map_window(conn, c->win);

    /* unmap current workspace clients */
    select_workspace(previous);
    for (c=head; c != NULL; c=c->next)
        xcb_unmap_window(conn, c->win);
    select_workspace(arg->i);
    focus_client(current);
}

static void save_workspace(const uint8_t i)
{
    workspaces[i].head = head;
    workspaces[i].current = current;
}

static void select_workspace(const uint8_t i)
{
    head = workspaces[i].head;
    current = workspaces[i].current;
    previous_workspace = current_workspace;
    current_workspace = i;
}

/* move a client to another workspace */
/* TODO: save fullscreen and centered properties */
static void client_to_workspace(const Arg *arg)
{
    if (arg->i == current_workspace || current == NULL || arg->i >= NUM_WORKSPACES)
        return;

    client *tmp = current;
    const uint8_t tmp2 = current_workspace;

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, current->win,
                    wmatoms[NET_WM_DESKTOP], XCB_ATOM_CARDINAL, 32, 1, &arg->i);

    /* add client to new workspace */
    select_workspace(arg->i);
    add_window_to_list(tmp->win);
    save_workspace(arg->i);

    /* remove client from current workspace */
    select_workspace(tmp2);
    xcb_unmap_window(conn,tmp->win);
    remove_window(tmp->win);
    save_workspace(tmp2);
    focus_client(current);
}

/* XCB_MAP_REQUEST handler
 * set up window's mask and border and add it to the list, and map+focus it */
static void setup_win(xcb_window_t w)
{
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

    xcb_icccm_get_wm_class_reply_t ch;
    if (xcb_icccm_get_wm_class_reply(conn, xcb_icccm_get_wm_class(conn, w), &ch, NULL)) {
        if (strstr(ch.class_name, "Chromium")){
            toggle_maximize(NULL);
        }
    }


    xcb_flush(conn); 
}

/* make the window into a client and set it to head and current
 * get wm size hints */
static void add_window_to_list(xcb_window_t w)
{
    client *c, *t;
    if ((c = malloc(sizeof(client))) == NULL)
        err(EXIT_FAILURE, "couldn't allocate memory");

    if (!w) return;
    PDEBUG("add window to list %d\n", w);
    c->id = w;
    c->x = c->y = c->width = c->height = c->oldx = c->oldy = c->oldw = c->oldh = 0;
    c->min_width = c->min_height = c->base_width = c->base_height = 0;
    c->is_maximized = c->is_centered = c->dontcenter = false;

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

    /* TODO: add support for US_SIZE and MAX_SIZE ? */
    if (h.flags &XCB_ICCCM_SIZE_HINT_US_POSITION)
        PDEBUG("HINTS: US POSITION: x: %d y: %d\n", h.x, h.y);
    if (h.flags &XCB_ICCCM_SIZE_HINT_US_SIZE)
        PDEBUG("HINTS: US SIZE: width: %d height: %d \n", h.width, h.height);
    if (h.flags &XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)
    {
        c->min_width = h.min_width;
        c->min_height = h.min_height;
        PDEBUG("HINTS: min_width %d min_height %d\n", c->min_width, c->min_height);
    }

    if (h.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)
        PDEBUG("HINTS: P MAX SIZE max_width: %d max_height: %d\n", h.max_width, h.max_height);

    if (h.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)
        PDEBUG("HINTS: P RESIZE INC width_inc: %d height_inc: %d\n", h.width_inc, h.height_inc);

    if (h.flags &XCB_ICCCM_SIZE_HINT_BASE_SIZE)
    {
        c->base_width  = h.base_width;
        c->base_height = h.base_height;
        PDEBUG("HINTS: base_width %d base_height %d\n", c->base_width, c->base_height);
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

/* find the window in the client list and remove the client */
static void remove_window(xcb_window_t w)
{
    client *c;
    if ((c = window_to_client(w)) == NULL) return;

    PDEBUG("remove_window %d\n", c->win);
    if (c->prev == NULL && c->next == NULL)
    { // one client in list
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

static void kill_current()
{
    if (NULL == current)
        /* TODO: query tree reply */
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
static void cycle_win(const Arg *arg)
{
    if (current == NULL || head == NULL)
        return;

    client *c;

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
                return;  // this is the only client
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

	xcb_ewmh_set_active_window(ewmh, def_screen, current->win);
}

/* focus input on window and change window's border color */
static void focus_client(struct client *c)
{
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

static void event_loop(void)
{
    xcb_generic_event_t *ev;
    running = true;
    while (running)
    {
        xcb_flush(conn);
        ev = xcb_wait_for_event(conn);

        switch ( CLEANMASK(ev->response_type) ) 
        {

        case XCB_UNMAP_NOTIFY:
        {
            xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
            client *c = window_to_client(e->window);
            PDEBUG("event: Unmap Notify %d\n", e->window);
            if (e->window == screen->root) break;
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
        } break;

        /* i.e. fullscreen */
        case XCB_CLIENT_MESSAGE:
        {
            PDEBUG("event: Client Message\n");
            xcb_client_message_event_t *e = (xcb_client_message_event_t*)ev;
            client *c = window_to_client(e->window);
            if (!c) break;

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

        } break;

        case XCB_CONFIGURE_REQUEST:
        {
            xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
            PDEBUG("event: Confgirue Request, mask: %d\n", e->value_mask);
            client *c = window_to_client(e->window);
            if (!c) break;
            if (c->is_maximized) break;

            unsigned int v[7];
            uint8_t i = 0;

            if (e->value_mask & XCB_CONFIG_WINDOW_X)
            {
                PDEBUG("CONFIG: x: %d\n", e->x);
                v[i++] = c->x = e->x;
                c->dontcenter = true;
            }
            if (e->value_mask & XCB_CONFIG_WINDOW_Y)
            {
                PDEBUG("CONFIG: y: %d\n", e->y);
                v[i++] = c->y = e->y;
                c->dontcenter = true;
            }
            if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
            {
                PDEBUG("CONFIG: width: %d\n", e->width);
                v[i++] = c->width = e->width;
            }
            if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
            {
                PDEBUG("CONFIG: height: %d\n", e->height);
                v[i++] = c->height = e->height;
            }
            if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
            {
                PDEBUG("CONFIG: border width: %d\n", e->border_width);
                v[i++] = e->border_width;
            }
            if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
            {
                PDEBUG("CONFIG: sibling\n");
                v[i++] = e->sibling;
            }
            if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
            {
                PDEBUG("CONFIG: stack mode\n");
                v[i++] = e->stack_mode;
            }
            /* re-center the win if it is in centered mode */
            if (c->is_centered && !c->dontcenter)
                center_win(c);

            xcb_configure_window(conn, e->window, e->value_mask, v);
            xcb_flush(conn);
        } break;

        case XCB_MAP_REQUEST:
        {
            PDEBUG("event: Map Request\n");
            xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
            setup_win(e->window);
            if (CENTER_BY_DEFAULT > 0)
            {
                toggle_centered(NULL);
                xcb_map_window(conn, e->window);
            }
            xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, e->window,
                        XCB_CURRENT_TIME);
        } break;

        case XCB_DESTROY_NOTIFY:
        {
            PDEBUG("event: \033[0;31mDestroy Notify\033[0m\n");
            xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;

            client *c;
            if ((c = window_to_client(e->window)) != NULL)
            {
                remove_window(c->win);
                focus_client(current);
            }
        } break;

        case XCB_KEY_PRESS:
        {
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
        } break;

#ifdef MOUSE
        case XCB_BUTTON_PRESS: {
            uint32_t values[3];
            xcb_window_t win = 0;

            xcb_button_press_event_t *e;
            e = ( xcb_button_press_event_t *)ev;
            win = e->child;

            if (!win || win == screen->root)
                break;

            client *c = window_to_client(win);
            if (c) current = c;

            values[0] = XCB_STACK_MODE_ABOVE;
            xcb_configure_window(conn, win,
                    XCB_CONFIG_WINDOW_STACK_MODE, values);

            xcb_grab_pointer(conn, 0, screen->root,
                XCB_EVENT_MASK_BUTTON_RELEASE
                | XCB_EVENT_MASK_BUTTON_MOTION
                | XCB_EVENT_MASK_POINTER_MOTION_HINT,
                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                screen->root, XCB_NONE, XCB_CURRENT_TIME);
            focus_client(current);
            xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
            xcb_flush(conn);
        } break;

        case XCB_BUTTON_RELEASE:
            break;
#endif
        
#ifdef DEBUG
        default:
            if (ev->response_type <= MAX_EVENTS)
                PDEBUG("\033[1;30mEvent: %s\033[0m\n", evnames[ev->response_type]);
            else
                PDEBUG("Event: #%d. Not known.\n", ev->response_type);
            break;
#endif

        } /* switch */
        free(ev);

        if (xcb_connection_has_error(conn) != 0)
            running = false;
    } /* while */
}

static bool grab_keys(void)
{
    xcb_keycode_t *keycode;
    for (unsigned int i=0; i<LENGTH(keys); i++)
    {
        keycode = xcb_get_keycodes(keys[i].keysym);
        for (unsigned int k=0; keycode[k] != XCB_NO_SYMBOL; k++)
            xcb_grab_key(conn, 1, screen->root, keys[i].mod, keycode[k],
                         XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

#ifdef MOUSE
    xcb_grab_button(conn, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 1, MOD);

    xcb_grab_button(conn, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC,
            XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 3, MOD);
#endif

    return true;
}

/* some small wrappers for xcb */
static xcb_keysym_t xcb_get_keysym(xcb_keycode_t keycode)
{
    xcb_key_symbols_t *keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        err(EXIT_FAILURE, "couldn't get keysym.");
    xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
    xcb_key_symbols_free(keysyms);
    return keysym;
}

static xcb_keycode_t* xcb_get_keycodes(xcb_keysym_t keysym)
{
    xcb_key_symbols_t *keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        err(EXIT_FAILURE, "couldn't get keycode.");
    xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
    xcb_key_symbols_free(keysyms);
    return keycode;
}

static void change_border_width(xcb_window_t win, uint8_t width)
{
    uint32_t value[1] = { width };
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, value);
}

static void change_border_color(xcb_window_t win, long color)
{
    if (!win || BORDER_WIDTH == 0) return;
    uint32_t value[1] = { color };
    xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, value);
}

static xcb_atom_t getatom(const char *atom_name)
{
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *rep;
    atom_cookie = xcb_intern_atom(conn, 0, strlen(atom_name), atom_name);
    rep = xcb_intern_atom_reply(conn, atom_cookie, NULL);
    if (NULL == rep) return 0;
    xcb_atom_t atom = rep->atom;
    free(rep);
    return atom;
}

static void ewmh_init(void)
{
    ewmh = malloc(sizeof(xcb_ewmh_connection_t));
    if (xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh), NULL) == 0)
            err(EXIT_FAILURE, "couldn't initialize ewmh.");
}

/* set up sinal handlers, display, mask, atoms, and keys */
static void bwm_setup()
{
    sigchld();
    signal(SIGINT, sighandle);
    signal(SIGTERM, sighandle);

    /* get the display  */
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    if (!screen)
        err(EXIT_FAILURE, "could not find screen.");
    xcb_drawable_t root = screen->root;

    ewmh_init();

    /* subscribe to events */
    unsigned int value[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
                          | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT };
    xcb_void_cookie_t cookie =
        xcb_change_window_attributes_checked(conn, root, XCB_CW_EVENT_MASK, value);
    xcb_generic_error_t *error = xcb_request_check(conn, cookie);
    if (error != NULL)
        err(EXIT_FAILURE, "another window manager is running.");

	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[] = { XCB_EVENT_MASK_POINTER_MOTION };
	xcb_window_t recorder = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, recorder, root, 0, 0,
            screen->width_in_pixels, screen->height_in_pixels, 0,
            XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, mask, values);

	xcb_atom_t net_atoms[] = {ewmh->_NET_SUPPORTED,
	                          ewmh->_NET_NUMBER_OF_DESKTOPS,
	                          ewmh->_NET_CURRENT_DESKTOP,
	                          ewmh->_NET_ACTIVE_WINDOW,
	                          ewmh->_NET_CLOSE_WINDOW,
                              ewmh->_NET_WM_NAME,
	                          ewmh->_NET_WM_DESKTOP,
	                          ewmh->_NET_WM_WINDOW_TYPE,
	                          ewmh->_NET_WM_STATE,
                              ewmh->_NET_WM_PID,
	                          ewmh->_NET_WM_STATE_FULLSCREEN};

	xcb_ewmh_set_supported(ewmh, def_screen, LENGTH(net_atoms), net_atoms);

	xcb_ewmh_set_wm_name(ewmh, recorder, 3, "bwm");
	xcb_ewmh_set_wm_pid(ewmh, recorder, getpid());
	xcb_ewmh_set_supporting_wm_check(ewmh, root, recorder);
	xcb_ewmh_set_supporting_wm_check(ewmh, recorder, recorder);

    for (unsigned int i=0; i<ATOM_COUNT; i++)
        wmatoms[i] = getatom(WM_ATOM_NAMES[i]);

    static const uint8_t _WORKSPACES = NUM_WORKSPACES;
    current_workspace = previous_workspace = 0;
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        wmatoms[NET_CURRENT_DESKTOP], XCB_ATOM_CARDINAL,
                        32, 1,&current_workspace);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        wmatoms[NET_NUMBER_OF_DESKTOPS], XCB_ATOM_CARDINAL,
                        32, 1,&_WORKSPACES);
    xcb_ewmh_set_number_of_desktops(ewmh, def_screen, _WORKSPACES);
	xcb_ewmh_set_current_desktop(ewmh, def_screen, 0);

    focus_client(NULL); 

    /* keys */
    if (!grab_keys())
        err(EXIT_FAILURE, "error setting up keycodes.");

    /* print the atom values for debugging */
    for (int i = 0; i<ATOM_COUNT; i++)
        PDEBUG("%s: %d\n", WM_ATOM_NAMES[i], wmatoms[i]);

    xcb_flush(conn);
}

/* fork the process and execute command */
static void spawn(const Arg *arg)
{
    if (fork() == 0)
    {
        if (conn)
            close(screen->root);
        setsid();
        execvp((char*)arg->com[0], (char**)arg->com);
        err(EXIT_FAILURE, "execvp %s", ((char **)arg->com)[0]);
    }
}

static void bwm_exit() {
    cleanup();
    exit(EXIT_SUCCESS);
}

static void bwm_restart() {
    xcb_set_input_focus(conn, XCB_NONE,XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
    xcb_ewmh_connection_wipe(ewmh);
    if (ewmh) free(ewmh);
    xcb_disconnect(conn);

    execvp(BWM_PATH, NULL);
}

static void cleanup()
{
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
	xcb_ewmh_connection_wipe(ewmh);
	if (ewmh) free(ewmh);

    xcb_query_tree_reply_t  *query;
    xcb_window_t *c;
    if ((query = xcb_query_tree_reply(conn,xcb_query_tree(conn,screen->root),0)))
    {
        c = xcb_query_tree_children(query);
        for (unsigned int i = 0; i != query->children_len; ++i)
            free(window_to_client(c[i]));
        free(query);
    }

    xcb_flush(conn);
    xcb_disconnect(conn);
}

static void sighandle(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
        running = false;
}

static void sigchld() {
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        err(EXIT_FAILURE, "cannot install SIGCHLD handler");

    while (0 < waitpid(-1, NULL, WNOHANG));
}

int main()
{
    if (xcb_connection_has_error(conn = xcb_connect(NULL, &def_screen)))
        err(EXIT_FAILURE, "xcb_connect error");

    bwm_setup();
    event_loop();
    cleanup();
    return EXIT_FAILURE;
}
