#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <xcb/xcb.h>

/*#define DEBUG*/
/*#define MOUSE*/

#ifdef DEBUG
#include "events.h"
#endif

#define LENGTH(X)    (sizeof(X)/sizeof(*X))

#ifdef DEBUG
#define PDEBUG(...) \
    fprintf(stderr, "asdf: "); fprintf(stderr, __VA_ARGS__);
#else
#define PDEBUG(...)
#endif

/* keybinds set in config.h */
typedef struct Key {
    unsigned int mod;
    unsigned int keysym;
    void (*function)(int, int);
    int x, y;
} Key;

xcb_connection_t *conn;   /* connection to X server */
xcb_screen_t *screen;     /* current screen */
xcb_drawable_t focuswin;  /* currently focused window */

static void cleanup();
static void killwin();
static void nextwin();
static void resize(int x, int y);
static void move(int x, int y);
static void setfocus(xcb_window_t w);
static void handle_events(void);
static void grab_keys_and_buttons(void);

#include "config.h"

/*
 *Set keyboard focus to follow mouse pointer, then disconnect
 */
static void cleanup()
{
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
    xcb_flush(conn);

    PDEBUG("cleanup()\n");
    xcb_disconnect(conn);

    exit(0);
}

/*
 *Kill focuswin (used for keybind)
 */
static void killwin()
{
    /* don't kill root */
    if(!focuswin || focuswin == screen->root)
        return;

    xcb_kill_client(conn, focuswin);
}

/*
 *Change focus to the next window
 *alt + tab doesn't loop
 */
static void nextwin()
{
    xcb_query_tree_reply_t *r;
    xcb_window_t *c,t = 0;
    xcb_get_window_attributes_cookie_t ac;
    xcb_get_window_attributes_reply_t *ar;
    int i;

    r = xcb_query_tree_reply(conn, xcb_query_tree(conn, screen->root), 0);
    if (!r || !r->children_len)
        return;
    c = xcb_query_tree_children(r);

    for (i=0; i < r->children_len; i++)
    {
        ac = xcb_get_window_attributes(conn, c[i]);
        ar = xcb_get_window_attributes_reply(conn, ac, NULL);

        if (ar)
        {
            if (ar->map_state == XCB_MAP_STATE_VIEWABLE) {
                if (ar->override_redirect) break;
                if (r->children_len == 1) {
                    t = c[i];
                    break;
                }
                if (c[i] == focuswin) break;
                t = c[i];
            }
        }
    }

    if (t)
    {
        setfocus(t);
    }
    free(r);
}

/*
 *Resize the focused window by x and y
 */
static void resize(int x, int y)
{
    uint32_t values[2];
    xcb_get_geometry_reply_t *geom;
    xcb_window_t win = focuswin;

    /* don't resize root */
    if (!win || win == screen->root)
        return;

    geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), NULL);
    if (!geom)
        return;

    /*don't resize to smaller than 10px*/
    values[0] = geom->width+x>10  ? geom->width + x  : geom->width;
    values[1] = geom->height+y>10 ? geom->height + y : geom->height;

    PDEBUG("X: %d -> %d, Y:%d -> %d\n", geom->width, geom->width + x,
                                        geom->height, geom->height + y);

    xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_WIDTH
                                   | XCB_CONFIG_WINDOW_HEIGHT, values);

    free(geom);
}

/*
 *Move the focused window by x or y
 */
static void move(int x, int y)
{
    uint32_t values[2];
    xcb_get_geometry_reply_t *geom;

    /* don't move root */
    if (!focuswin || focuswin == screen->root)
        return;

    geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, focuswin), NULL);
    if (!geom)
        return;

    values[0] = geom->x + x;
    values[1] = geom->y + y;

    PDEBUG("X: %d -> %d, Y:%d -> %d\n", geom->x, geom->x + x,
                                        geom->y, geom->y + y);

    xcb_configure_window(conn, focuswin, XCB_CONFIG_WINDOW_X
                                   | XCB_CONFIG_WINDOW_Y, values);

    free(geom);
}

/*
 *unfocus focuswin and set focus to w
 */
static void setfocus(xcb_window_t w)
{
    uint32_t values[1];

    /* don't focus root */
    if (w == screen->root || !w)
    {
        return;
    }

    values[0] = NORMCOLOR;
    xcb_change_window_attributes(conn, w, XCB_CW_BORDER_PIXEL, values);

    xcb_flush(conn);

    /*unfocus the last window*/
    values[0] = SELCOLOR;
    xcb_change_window_attributes(conn, focuswin, XCB_CW_BORDER_PIXEL, values);

    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, w, XCB_CURRENT_TIME);

    focuswin = w;
}

static void handle_events(void)
{
    xcb_generic_event_t *ev;
    uint32_t values[3];

    /* register for events */
    uint32_t mask = XCB_CW_EVENT_MASK;
    values[0] =  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;

    xcb_change_window_attributes_checked(conn, screen->root, mask, values);
    xcb_flush(conn);

    /* block until we receive an event */
    while ( (ev = xcb_wait_for_event(conn)) )
    {
        PDEBUG("Event: %s\n", evnames[ev->response_type]);

        switch ( ev->response_type & ~0x80 ) 
        {
        case XCB_MAP_REQUEST:
        {
            xcb_map_request_event_t *e;
            /*PDEBUG("event: map requeset\n");*/

            e = (xcb_map_request_event_t *)ev;
            xcb_map_window(conn, e->window);
        }
        break;

        case XCB_DESTROY_NOTIFY:
        {
            xcb_destroy_notify_event_t *e;
            /*PDEBUG("event: destroy window\n");*/

            e = (xcb_destroy_notify_event_t *)ev;
            xcb_kill_client(conn, e->window);
        }
        break;

        case XCB_CREATE_NOTIFY:
        {
            xcb_create_notify_event_t *e;
            /*PDEBUG("event: create window\n");*/

            e = (xcb_create_notify_event_t *)ev;

            values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
            xcb_change_window_attributes_checked(conn, e->window, 
                                                XCB_CW_EVENT_MASK, values);

            values[0] = 4;
            xcb_configure_window(conn, e->window, 
                                XCB_CONFIG_WINDOW_BORDER_WIDTH, values);

            setfocus(e->window);
        }
        break;

        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e;
            e = (xcb_key_press_event_t *)ev;
            /*PDEBUG("Key %d pressed \n", e->detail);*/

            int i;
            for (i=0; i< LENGTH(keys); i++)
            {
                PDEBUG("config key = %d , key pressed = %d\n", 
                        keys[i].keysym, e->detail);

                if (keys[i].keysym == e->detail
                        && (keys[i].mod & ~0x80) == (e->state & ~0x80)
                        && keys[i].function)
                {
                    keys[i].function(keys[i].x, keys[i].y);
                    break;
                }
            }
        }
        break;

#ifdef MOUSE
        case XCB_ENTER_NOTIFY:
        {
            xcb_enter_notify_event_t *e;
            e = (xcb_enter_notify_event_t *)ev;
        
            setfocus(e->event);
        }
        break;
#endif

        } /* switch */

        xcb_flush(conn); 
        free(ev);
    } /* while */
}

/*
 *Set up keys
 */
static void grab_keys_and_buttons(void)
{
    int i;

    for (i=0; i<LENGTH(keys); ++i)
    {
        xcb_grab_key(conn, 1, screen->root, keys[i].mod, keys[i].keysym, 
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }

#ifdef MOUSE
    xcb_grab_button(conn, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS | 
                XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
                XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 1, XCB_MOD_MASK_1);

    xcb_grab_button(conn, 0, screen->root, XCB_EVENT_MASK_BUTTON_PRESS | 
                XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
                XCB_GRAB_MODE_ASYNC, screen->root, XCB_NONE, 3, XCB_MOD_MASK_1);
#endif

    xcb_flush(conn);
}

int main ()
{
    atexit(cleanup);

    /* open the connection */
    conn = xcb_connect(NULL, NULL);

    if (xcb_connection_has_error(conn))
    {
        fprintf(stderr, "xcb_connect error");
        exit(1);
    }

    /* get the first screen */
    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    focuswin = screen->root;

    grab_keys_and_buttons();

    handle_events();

    exit(1);
}
