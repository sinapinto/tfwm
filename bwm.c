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
#include <unistd.h>
#include <err.h>
#include <signal.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

/* TODO xcb atoms, icccm, debug */

/* uncomment this to print debug statements */
/*#define DEBUG*/

#ifdef DEBUG
#include "events.h"
#endif
#ifdef DEBUG
#   define PDEBUG(...) fprintf(stderr, __VA_ARGS__);
#else
#   define PDEBUG(...)
#endif

#define Mod1Mask     XCB_MOD_MASK_1
#define Mod4Mask     XCB_MOD_MASK_4
#define ShiftMask    XCB_MOD_MASK_SHIFT
#define ControlMask  XCB_MOD_MASK_CONTROL

#define CLEANMASK(mask) ((mask & ~0x80))
#define LENGTH(X)       (sizeof(X)/sizeof(*X))

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
    bool is_maximized;                       /* was fullscreened */
    bool is_centered;                        /* center mode is active */
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
static client *head;            /* head of window list */
static client *current;         /* current window in list */
static uint8_t current_workspace;   /* active workspace number */

/* prototypes */
static void cleanup();
static void sighandle(int signal);
static int get_geom(xcb_drawable_t win, int16_t *x, int16_t *y,
                    uint16_t *width, uint16_t *height);
static void spawn(const Arg *arg);
static client *window_to_client(xcb_window_t w);
static void resize(const Arg *arg);
static void aspect_resize(const int8_t i);
static void move(const Arg *arg);
static void xcb_configure_border_width(xcb_window_t win, uint8_t width);
static void toggle_maximize(const Arg *arg);
static void center_win(client *c);
static void toggle_centered_mode(const Arg *arg);
static void setup_win(xcb_window_t w);
static void change_workspace(const Arg *arg);
static void save_workspace(const uint8_t i);
static void select_workspace(const uint8_t i);
static void client_to_workspace(const Arg *arg);
static void add_window_to_list(xcb_window_t w);
static void remove_window_from_list(xcb_window_t w);
static void killwin();
static void nextwin(const Arg *arg);
static void unfocus_client(struct client *c);
static void focus_client(struct client *c);
static void set_input_focus(xcb_window_t win);
static void event_loop(void);
static bool grab_keys(void);
static xcb_keysym_t xcb_get_keysym(xcb_keycode_t keycode);
static xcb_keycode_t* xcb_get_keycodes(xcb_keysym_t keysym);
static void bwm_setup(void);

#include "config.h"

static void cleanup()
{
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
    xcb_flush(conn);
    xcb_disconnect(conn);
    exit(EXIT_SUCCESS);
}

static void sighandle(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
        cleanup();
}

static int get_geom(xcb_drawable_t win, int16_t *x, int16_t *y,
                    uint16_t *width, uint16_t *height)
{
    xcb_get_geometry_reply_t *geom;
    geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), NULL);
    if (NULL == geom)
        return 1;

    *x = geom->x;
    *y = geom->y;
    *width = geom->width;
    *height = geom->height;
    free(geom);
    return 0;
}

static void spawn(const Arg *arg)
{
    if (fork() == 0)
    {
        if (conn)
            close(screen->root);
        setsid();
        execvp((char*)arg->com[0], (char**)arg->com);
        err(EXIT_FAILURE, "bwm: execvp %s", ((char **)arg->com)[0]);
    }
}

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
    if (!current->win || current->win == screen->root)
        return;
    if (arg->i > 3)
    {
        aspect_resize(arg->i);
        return;
    }

    uint8_t step = steps[1];
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

    uint32_t values[2] = { current->width, values[1] = current->height };
    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_WIDTH
                                   | XCB_CONFIG_WINDOW_HEIGHT, values);

    /* unmaximize (redraw border) */
    if (current->is_maximized) current->is_maximized = false;
    xcb_configure_border_width(current->win, BORDER_WIDTH);

    if (current->is_centered) center_win(current);
    xcb_flush(conn); 
}

/* i = 4: grow; i = 5: shrink */
static void aspect_resize(const int8_t i)
{
    if (!current || i<4 || i>5) return;
    uint8_t step = steps[1];
    if (i == 4) /* grow */
    {
        current->width += step;
        current->height += step;
    }
    else /* shrink */
    {
        current->height = current->height-step > 0
                        ? current->height - step
                        : current->height;
        current->width  = current->width-step > 0
                        ? current->width - step
                        : current->width;
    }
    uint32_t values[2] = { current->width, current->height };
    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_WIDTH
                                   | XCB_CONFIG_WINDOW_HEIGHT, values);

    /* unmaximize (redraw border) */
    if (current->is_maximized) current->is_maximized = false;
    xcb_configure_border_width(current->win, BORDER_WIDTH);

    if (current->is_centered) center_win(current);
    xcb_flush(conn); 
}

static void move(const Arg *arg)
{
    if (!current->win || current->win == screen->root)
        return;
    uint8_t step = steps[0];

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
    if (current->is_centered)
        current->is_centered = false;
    xcb_flush(conn); 
}

static void xcb_configure_border_width(xcb_window_t win, uint8_t width)
{
    uint32_t value[1] = { width };
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, value);
}

/* rmove the border on fullscreen */
static void toggle_maximize(const Arg *arg)
{
    if (current == NULL) return;

    uint32_t val[5];
    val[4] = XCB_STACK_MODE_ABOVE;
    uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                  | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
                  | XCB_CONFIG_WINDOW_STACK_MODE;
    if (!current->is_maximized)
    {
        /* remove border */
        xcb_configure_border_width(current->win, 0);
        /* save our dimensions */
        current->oldx = current->x;
        current->oldy = current->y;
        current->oldw = current->width;
        current->oldh = current->height;
        /* go fullscreen */
        val[0] = 0; val[1] = 0;
        val[2] = screen->width_in_pixels; val[3] = screen->height_in_pixels;
        PDEBUG("maximize: x: %d y: %d width: %d height: %d\n",
                current->x, current->y, current->width, current->height);
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
        xcb_configure_border_width(current->win, BORDER_WIDTH);
        /* switch back to the old dimensions */
        val[0] = current->oldx;
        val[1] = current->oldy;
        val[2] = current->oldw;
        val[3] = current->oldh;
        PDEBUG("minimize: x: %d y: %d width: %d height: %d\n",
                current->oldx, current->oldy, current->oldw, current->oldh);
        xcb_configure_window(conn, current->win, mask, val);
        /* save our dimensions for later */
        current->x = current->oldx;
        current->y = current->oldy;
        current->width = current->oldw;
        current->height = current->oldh;
        current->is_maximized = false;
        focus_client(current);
    }
    xcb_flush(conn);
}

static void toggle_centered_mode(const Arg *arg)
{
    if (current)
        current->is_centered = current->is_centered ? false : true;
    center_win(current);
}

/* center the window */
static void center_win(client *c)
{
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

static void change_workspace(const Arg *arg)
{
    if (arg->i == current_workspace)
        return;
    uint8_t previous = current_workspace;
    save_workspace(current_workspace);
    /* map the new windows before unmapping the current ones to avoid
     * screen flickering */
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
    current_workspace = i;
}

static void client_to_workspace(const Arg *arg)
{
    if (arg->i == current_workspace || current == NULL)
        return;

    client *tmp = current;
    const uint8_t tmp2 = current_workspace;

    // Add client to workspace
    select_workspace(arg->i);
    add_window_to_list(tmp->win);
    save_workspace(arg->i);

    // Remove client from current workspace
    select_workspace(tmp2);
    xcb_unmap_window(conn,tmp->win);
    remove_window_from_list(tmp->win);
    save_workspace(tmp2);
    focus_client(current);
}

static void setup_win(xcb_window_t w)
{
    uint32_t values[2];
    values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
    values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_change_window_attributes(conn, w, XCB_CW_EVENT_MASK, values);
    xcb_configure_border_width(w, BORDER_WIDTH);
    xcb_map_window(conn, w);
    add_window_to_list(w);
    focus_client(current);
    xcb_flush(conn); 
}

/* make the window into a client and set it to head and current */
static void add_window_to_list(xcb_window_t w)
{
    client *c, *t;
    if ((c = malloc(sizeof(client))) == NULL)
        err(EXIT_FAILURE, "bwm: out of memory");

    c->id = w;
    c->x = 0;
    c->y = 0;
    c->width = 0;
    c->height = 0;
    c->is_maximized = false;
    c->is_centered = false;

    if (head == NULL)
    {
        PDEBUG("add_window_to_list [new head] %d\n", w);
        c->next = NULL;
        c->prev = NULL;
        c->win = w;
        c->is_maximized = false;
        get_geom(c->id, &c->x, &c->y, &c->width, &c->height);
        head = c;
    }
    else
    {
        PDEBUG("add_window_to_list %d\n", w);
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
    if (current) unfocus_client(current);
    current = c;
    save_workspace(current_workspace);
}

static void remove_window_from_list(xcb_window_t w)
{
    client *c;
    if ((c = window_to_client(w)) == NULL) return;

    PDEBUG("remove_window_from_list %d\n", c->win);
    if (c->prev == NULL && c->next == NULL)
    { // one client in list
        PDEBUG("[removing head]\n");
        free(head);
        head = NULL;
        current = NULL;
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
    save_workspace(current_workspace);
    return;
    xcb_flush(conn); 
}

static void killwin()
{
    if (NULL == current) return;
    xcb_kill_client(conn, current->win);
    xcb_flush(conn); 
}

static void nextwin(const Arg *arg)
{
    client *c;
    if(current != NULL && head != NULL)
    {
        if (arg->i == 0) // focus next
        {
            if (current->next == NULL) // at tail.. next win is head
            {
                if (current == head) // only client, don't do anything
                    return;
                PDEBUG("nextwin: going to head\n");
                c = head;
            }
            else
            {
                c = current->next;
            }
        }
        else // focus previous
        {
            if (current->prev == NULL) // at head.. prev win is tail
                for(c=head; c->next != NULL; c=c->next);
            else
                c = current->prev;
            PDEBUG("nextwin: %d\n", c->win);
        }
        unfocus_client(current);
        current = c;
        focus_client(current);
    }
}

/* this just changes the border color */
static void unfocus_client(struct client *c)
{
    if (c == NULL || current == NULL || BORDER_WIDTH == 0) return;
    uint32_t values[1] = { UNFOCUS };
    xcb_change_window_attributes(conn, current->win,
                                XCB_CW_BORDER_PIXEL, values);
    xcb_flush(conn);
}

static void focus_client(struct client *c)
{
    uint32_t values[1];
    if (NULL == current) // we just removed the head, so nothing to focus
    {
        xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                            XCB_CURRENT_TIME);
        xcb_flush(conn);
        return;
    }
    if (BORDER_WIDTH > 0)
    {
        values[0] = FOCUS;
        xcb_change_window_attributes(conn, c->win,
                                    XCB_CW_BORDER_PIXEL, values);
    }
    set_input_focus(c->win);
}

static void set_input_focus(xcb_window_t win)
{
    uint32_t values[1];
    values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, 
                        values);
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, win,
                        XCB_CURRENT_TIME);
    xcb_flush(conn);
}


static void event_loop(void)
{
    xcb_generic_event_t *ev;
    /* block until we receive an event */
    while ( (ev = xcb_wait_for_event(conn)) )
    {
        PDEBUG("Event: %s\n", evnames[ev->response_type]);
        switch ( CLEANMASK(ev->response_type) ) 
        {

        case XCB_CONFIGURE_NOTIFY:
        {
            xcb_configure_notify_event_t *e =
                (xcb_configure_notify_event_t *)ev;
            set_input_focus(e->window);
        } break;

        case XCB_MAP_REQUEST:
        {
            xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
            setup_win(e->window);
        } break;

        case XCB_DESTROY_NOTIFY:
        {
            xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;

            client *c;
            if ((c = window_to_client(e->window)) != NULL)
            {
                remove_window_from_list(e->window);
                focus_client(current);
            }
        } break;

        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
            xcb_keysym_t keysym   = xcb_get_keysym(e->detail);
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
        } /* switch */
        free(ev);
    }
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
    return true;
}

static xcb_keysym_t xcb_get_keysym(xcb_keycode_t keycode) {
    xcb_key_symbols_t *keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        err(EXIT_FAILURE, "bwm: couldn't get keysym.");
    xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
    xcb_key_symbols_free(keysyms);
    return keysym;
}

static xcb_keycode_t* xcb_get_keycodes(xcb_keysym_t keysym) {
    xcb_key_symbols_t *keysyms;
    if (!(keysyms = xcb_key_symbols_alloc(conn)))
        err(EXIT_FAILURE, "bwm: couldn't get keycode.");
    xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
    xcb_key_symbols_free(keysyms);
    return keycode;
}

static void bwm_setup(void)
{
    /* install signal handlers */
    signal(SIGINT, sighandle);
    signal(SIGTERM, sighandle);

    /* get the first screen */
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    if (!screen)
        err(EXIT_FAILURE, "bwm: could not find screen.");
    xcb_drawable_t root = screen->root;

    /* subscribe to events */
    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
                       | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
    xcb_void_cookie_t cookie =
        xcb_change_window_attributes_checked(conn, root, mask, values);
    xcb_generic_error_t *error = xcb_request_check(conn, cookie);
    if (error != NULL)
        err(EXIT_FAILURE, "bwm: another window manager is running.");

    /* grab keys */ 
    if (!grab_keys())
        err(EXIT_FAILURE, "bwm: error etting up keycodes.");
    xcb_flush(conn);
}

int main()
{
    /* open the connection */
    if (xcb_connection_has_error(conn = xcb_connect(NULL, NULL)))
        err(EXIT_FAILURE, "xcb_connect error");

    bwm_setup();
    event_loop();

    cleanup();
    return EXIT_SUCCESS;
}
