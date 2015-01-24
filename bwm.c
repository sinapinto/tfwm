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
#include <unistd.h>
#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <xcb/xcb.h>

#define DEBUG
#ifdef DEBUG
#include "events.h"
#endif
#ifdef DEBUG
#define PDEBUG(...) \
    fprintf(stderr, "asdf: "); fprintf(stderr, __VA_ARGS__);
#else
#define PDEBUG(...)
#endif

#define LENGTH(X)    (sizeof(X)/sizeof(*X))

typedef union {
    const char **com;
    const int8_t i;
} Arg;

/* keybinds set in config.h */
typedef struct key {
    unsigned int mod;
    unsigned int keysym;
    void (*function)(const Arg *);
    const Arg arg;
} key;

typedef struct client client;
struct client{
    xcb_drawable_t id;  /* get_geom */
    int16_t x, y; uint16_t width, height;
    int16_t oldx, oldy; uint16_t oldw, oldh; /* toggle_maximize */
    bool is_maximized;
    client *next;
    client *prev;
    xcb_window_t win;
};

typedef struct workspace {
    client *head;
    client *current;
} workspace;

xcb_connection_t *conn;         /* connection to X server */
xcb_screen_t *screen;           /* current screen */
static client *head;            /* head of window list */
static client *current;         /* current window in list */
static int current_workspace;
static workspace workspaces[6]; /* number of workspaces */

static void cleanup();
static void sighandle(int signal);
static int get_geom(xcb_drawable_t win, int16_t *x, int16_t *y,
                    uint16_t *width, uint16_t *height);
static void spawn(const Arg *arg);
static void resize(const Arg *arg);
static void move(const Arg *arg);
static void toggle_maximize(const Arg *arg);
static void setup_win(xcb_window_t w);
static void change_workspace(const Arg *arg);
static void save_workspace(int i);
static void select_workspace(int i);
static void client_to_workspace(const Arg *arg);
static void add_window_to_list(xcb_window_t w);
static void remove_window_from_list(xcb_window_t w);
static void killwin();
static void nextwin(const Arg *arg);
static void unfocus_client(struct client *c);
static void focus_client(struct client *c);
static void event_loop(void);

#include "config.h"

static void cleanup()
{
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
    xcb_flush(conn);

    xcb_disconnect(conn);
    exit(0);
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
        fprintf(stderr, "bwm: execvp %s\n", ((char **)arg->com)[0]);
        exit(0);
    }
}

static void resize(const Arg *arg)
{
    if (!current->win || current->win == screen->root)
        return;
    uint8_t step = steps[1];

    switch (arg->i)
    {
        case 0:
            current->height = current->height+step > 0 ? current->height + step 
                                                       : current->height;
            break;
        case 1:
            current->height = current->height-step > 0 ? current->height - step 
                                                       : current->height;
            break;
        case 2:
            current->width = current->width-step > 0 ? current->width - step 
                                                     : current->width;
            break;
        case 3:
            current->width = current->width+step > 0 ? current->width + step 
                                                     : current->width;
            break;
    }

    uint32_t values[2];
    values[0] = current->width;
    values[1] = current->height;
    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_WIDTH
                                   | XCB_CONFIG_WINDOW_HEIGHT, values);
    xcb_flush(conn); 
}

static void move(const Arg *arg)
{
    if (!current->win || current->win == screen->root)
        return;
    uint8_t step = steps[0];

    switch (arg->i)
    {
        case 0:
            current->y += step; break;
        case 1:
            current->y -= step; break;
        case 2:
            current->x -= step; break;
        case 3:
            current->x += step; break;
    }
    uint32_t values[2];
    values[0] = current->x; 
    values[1] = current->y;
    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_X
                                   | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(conn); 
}

static void toggle_maximize(const Arg *arg)
{
    client *c; 
    if (current == NULL) return;
    for (c=head; c != NULL; c=c->next)
        if (c == current ) break;
    if (c != current) return;

    uint32_t val[5];
    val[4] = XCB_STACK_MODE_ABOVE;
    uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                  | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
                  | XCB_CONFIG_WINDOW_STACK_MODE;
    if (!current->is_maximized)
    {
        uint16_t scrwidth = screen->width_in_pixels-(2*BORDER_WIDTH);
        uint16_t scrheight = screen->height_in_pixels-(2*BORDER_WIDTH);
        current->oldx = current->x;
        current->oldy = current->y;
        current->oldw = current->width;
        current->oldh = current->height;
        val[0] = 0; val[1] = 0;
        val[2] = scrwidth; val[3] = scrheight;
        PDEBUG("MAX: x: %d y: %d width: %d height: %d\n",
                current->x, current->y, current->width, current->height);
        xcb_configure_window(conn, c->win, mask, val);
        current->x = 0;
        current->y = 0;
        current->width = scrwidth;
        current->height = scrheight;
        current->is_maximized = true;
    }
    else
    {
        val[0] = current->oldx;
        val[1] = current->oldy;
        val[2] = current->oldw;
        val[3] = current->oldh;
        PDEBUG("MIN: x: %d y: %d width: %d height: %d\n",
                current->oldx, current->oldy, current->oldw, current->oldh);
        xcb_configure_window(conn, c->win, mask, val);
        current->x = current->oldx;
        current->y = current->oldy;
        current->width = current->oldw;
        current->height = current->oldh;
        current->is_maximized = false;
        focus_client(current);
    }
    xcb_flush(conn);
}

/* TODO: fix reordering */ 
static void change_workspace(const Arg *arg)
{
    client *c;
    if(arg->i == current_workspace)
        return;

    /* Save current "properties" */
    save_workspace(current_workspace);

    /* Unmap all window */
    if(head != NULL)
        for(c=head; c != NULL; c=c->next)
            xcb_unmap_window(conn, c->win);

    /* Take "properties" from the new workspace */
    select_workspace(arg->i);

    /* Map all windows */
    if(head != NULL)
    {
        for(c=head; c != NULL; c=c->next)
            xcb_map_window(conn, c->win);
    }

    focus_client(current);
}

static void save_workspace(int i)
{
    workspaces[i].head = head;
    workspaces[i].current = current;
}

static void select_workspace(int i)
{
    head = workspaces[i].head;
    current = workspaces[i].current;
    current_workspace = i;
}

static void client_to_workspace(const Arg *arg)
{
    client *tmp = current;
    int tmp2 = current_workspace;

    if(arg->i == current_workspace || current == NULL)
        return;

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

////////////////////////// WINDOW FOCUS /////////////////////////////
static void setup_win(xcb_window_t w)
{
    uint32_t values[2];
    values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
    values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_change_window_attributes(conn, w, XCB_CW_EVENT_MASK, values);
    values[0] = BORDER_WIDTH;
    xcb_configure_window(conn, w, XCB_CONFIG_WINDOW_BORDER_WIDTH, values);
    xcb_flush(conn); 
    add_window_to_list(w);
}

static void add_window_to_list(xcb_window_t w)
{
    client *c,*t;
    if ((c = malloc(sizeof(client))) == NULL)
    {
        fprintf(stderr, "bwm: out of memory!\n");
        exit(1);
    }
    c->id = w;
    c->x = 0;
    c->y = 0;
    c->width = 0;
    c->height = 0;

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
}

static void remove_window_from_list(xcb_window_t w)
{
    client *c;

    for(c=head; c != NULL; c=c->next)
    {
        if(c->win == w)
        {
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
            return;
        }
    }
    xcb_flush(conn); 
}

static void killwin()
{
    client *c;
    for (c=head; c != NULL; c=c->next)
    {
        if (c == current)
        {
            if (!c->win || c->win == screen->root)
                return;
            xcb_kill_client(conn, c->win);
            xcb_flush(conn); 
            return;
        }
    }
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
                PDEBUG("\033[031mnextwin [going to head]\033[0m\n");
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
            {
                for(c=head; c->next != NULL; c=c->next);
            }
            else
                c = current->prev;
            PDEBUG("\033[031mnextwin %d\033[0m\n", c->win);
        }
        unfocus_client(current);
        current = c;
        focus_client(current);
    }
}

static void unfocus_client(struct client *c)
{
    uint32_t values[1];
    if (c == NULL || current == NULL) return;
    values[0] = NORMCOLOR;
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
    values[0] = SELCOLOR;
    xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, values);
    values[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_STACK_MODE, values);
    xcb_flush(conn);

    PDEBUG("setting input focus: %d\n", c->win);
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win,
                        XCB_CURRENT_TIME);
    xcb_flush(conn);
}

static void event_loop(void)
{
    xcb_generic_event_t *ev;

    /* block until we receive an event */
    while ( (ev = xcb_wait_for_event(conn)) )
    {
        /* CREATE_NOTIFY -> CONFIGURE_NOTIFY -> MAP_NOTIFY */ 
        PDEBUG("Event: %s\n", evnames[ev->response_type]);
        switch ( ev->response_type & ~0x80 ) 
        {
        case XCB_CREATE_NOTIFY:
        {
            /*xcb_create_notify_event_t *e;*/
            /*e = (xcb_create_notify_event_t *)ev;*/
            /*setup_win(e->window);*/
        }
        break;

        case XCB_MAP_NOTIFY:
        {
            xcb_map_notify_event_t *e;
            e = (xcb_map_notify_event_t *)ev;
            setup_win(e->window);
            focus_client(current);
        }
        break;

        case XCB_DESTROY_NOTIFY:
        {
            xcb_destroy_notify_event_t *e;
            e = (xcb_destroy_notify_event_t *)ev;

            client *c;
            for (c=head; c != NULL; c=c->next)
            {
                if(e->window == c->win)
                {
                    remove_window_from_list(e->window);
                    focus_client(current);
                    break;
                }
            }
        }
        break;

        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e;
            e = (xcb_key_press_event_t *)ev;
            /*PDEBUG("Key %d pressed \n", e->detail);*/

            for (uint8_t i=0; i<LENGTH(keys); i++)
            {
                if (keys[i].keysym == e->detail
                    && (keys[i].mod & ~0x80) == (e->state & ~0x80)
                    && keys[i].function)
                {
                    keys[i].function(&keys[i].arg);
                    break;
                }
            }
        }
        break;
        } /* switch */
        free(ev);
    }
}

int main ()
{
    atexit(cleanup);
    signal(SIGINT, sighandle);
    signal(SIGTERM, sighandle);

    /* open the connection */
    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn))
    {
        fprintf(stderr, "xcb_connect error");
        exit(1);
    }

    /* get the first screen */
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    xcb_drawable_t root = screen->root;

    head = NULL;
    current = NULL;

    /* register for events */
    uint32_t values[1];
    uint32_t mask = XCB_CW_EVENT_MASK;
    values[0] =  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_change_window_attributes_checked(conn, root, mask, values);

    /* initialize workspaces */
    for (int i=0; i<LENGTH(workspaces); i++)
    {
        workspaces[i].head = head;
        workspaces[i].current = current;
    }
    const Arg arg = {.i = 0};
    current_workspace = 0;
    change_workspace(&arg);

    /* grab keys */
    for (int i=0; i<LENGTH(keys); i++)
    {
        xcb_grab_key(conn, 1, screen->root, keys[i].mod, keys[i].keysym, 
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
    xcb_flush(conn);

    event_loop();

    exit(0);
}
