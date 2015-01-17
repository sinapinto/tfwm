#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <xcb/xcb.h>

/*#define DEBUG*/

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

typedef struct client client;
struct client{
    client *next;
    client *prev;
    xcb_window_t win;
};

xcb_connection_t *conn;   /* connection to X server */
xcb_screen_t *screen;     /* current screen */
static client *head;
static client *current;
static int screen_height;
static int screen_width;

static void cleanup();
static void killwin();
static void nextwin();
static void resize(int x, int y);
static void move(int x, int y);
static void fullscreen();
static void add_window(xcb_window_t w);
static void remove_window(xcb_window_t w);
static void focus_current();
static void handle_events(void);
static void grab_keys(void);

#include "config.h"

static void cleanup()
{
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
    xcb_flush(conn);

    PDEBUG("cleanup()\n");
    xcb_disconnect(conn);

    exit(0);
}

static void killwin()
{
    client *c;
    for (c=head; c != NULL; c=c->next)
    {
        if (c == current)
        {
            /* don't kill root */
            if (!c->win || c->win == screen->root)
                return;

            xcb_kill_client(conn, c->win);
            xcb_flush(conn); 
            return;
        }
    }
}

static void nextwin()
{
  client *c;
   if(current != NULL && head != NULL)
   {
        if(current->next == NULL)
            c = head;
        else
            c = current->next;

        current = c;
        focus_current();
   }
}

static void resize(int x, int y)
{
    /* don't resize root */
    if (!current->win || current->win == screen->root)
        return;

    xcb_get_geometry_cookie_t c = xcb_get_geometry(conn, current->win);
    xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(conn, c, NULL);

    uint32_t values[2];
    /*don't resize to less than 5 pixels */
    values[0] = r->width+x>5  ? r->width + x  : r->width;
    values[1] = r->height+y>5 ? r->height + y : r->height;

    PDEBUG("X: %d -> %d, Y:%d -> %d\n", r->width, r->width + x,
                                        r->height, r->height + y);

    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_WIDTH
                                   | XCB_CONFIG_WINDOW_HEIGHT, values);
    xcb_flush(conn); 

    free(r);
}

static void move(int x, int y)

{
    /* don't move root */
    if (!current->win || current->win == screen->root)
        return;

    xcb_get_geometry_cookie_t c = xcb_get_geometry(conn, current->win);
    xcb_get_geometry_reply_t *r = xcb_get_geometry_reply(conn, c, NULL);

    uint32_t values[2];
    values[0] = r->x + x;
    values[1] = r->y + y;

    PDEBUG("X: %d -> %d, Y:%d -> %d\n", r->x, r->x + x,
                                        r->y, r->y + y);

    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_X
                                   | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(conn); 

    free(r);
}

static void fullscreen()
{
    client *c;
    for (c=head; c != NULL; c=c->next)
    {
        if (c == current)
        {
            uint32_t val[5];
            uint32_t mask = XCB_CONFIG_WINDOW_X
                          | XCB_CONFIG_WINDOW_Y
                          | XCB_CONFIG_WINDOW_WIDTH
                          | XCB_CONFIG_WINDOW_HEIGHT
                          | XCB_CONFIG_WINDOW_STACK_MODE;
            val[0] = 0;
            val[1] = 0;
            val[2] = screen_width;
            val[3] = screen_height;
            val[4] = XCB_STACK_MODE_ABOVE;

            xcb_configure_window(conn, c->win, mask, val);
        }
    }
    xcb_flush(conn);
}

static void add_window(xcb_window_t w) {
    client *c,*t;

    if ((c = malloc(sizeof(c))) == NULL)
    {
        fprintf(stderr, "Error calloc!");
        exit(1);
    }

    if (head == NULL)
    {
        c->next = NULL;
        c->prev = NULL;
        c->win = w;
        head = c;
    }
    else
    {
        for (t=head; t->prev != NULL; t=t->prev)
            ;
        c->prev = NULL;
        c->next = t;
        c->win = w;
        t->prev = c;
        head = c;
    }

    current = c;
}

static void remove_window(xcb_window_t w) {
    client *c;

    for(c=head; c != NULL; c=c->next) {
        if(c->win == w) {
            if(c->prev == NULL && c->next == NULL) { // one client in list
                free(head);
                head = NULL;
                current = NULL;
                return;
            }
            if(c->prev == NULL) {               // head
                head = c->next;
                c->next->prev = NULL;
                current = c->next;
            }
            else if(c->next == NULL) {          // tail
                c->prev->next = NULL;
                current = c->prev;
            }
            else {
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

static void focus_current() {
    client *c;
    uint32_t values[1];
    for (c=head; c != NULL; c=c->next)
    {
        if(current == c)
        {
            values[0] = SELCOLOR;
            xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, values);
            xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win, XCB_CURRENT_TIME);
            xcb_flush(conn);
        }
        else
        {
            values[0] = NORMCOLOR;
            xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, values);
            xcb_flush(conn);
        }
    }
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
        /* CREATE_NOTIFY -> CONFIGURE_NOTIFY -> MAP_NOTIFY */ 
        PDEBUG("Event: %s\n", evnames[ev->response_type]);

        switch ( ev->response_type & ~0x80 ) 
        {
        case XCB_CREATE_NOTIFY:
        {
            xcb_create_notify_event_t *e;
            e = (xcb_create_notify_event_t *)ev;

            values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
            xcb_change_window_attributes_checked(conn, e->window, 
                                                XCB_CW_EVENT_MASK, values);

            values[0] = BORDER_WIDTH;
            xcb_configure_window(conn, e->window, 
                                XCB_CONFIG_WINDOW_BORDER_WIDTH, values);

            add_window(e->window);
            focus_current();
            xcb_flush(conn); 
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
                    remove_window(e->window);
                    focus_current();
                    break;
                }
            }
        }
        break;

        case XCB_KEY_PRESS:
        {
            xcb_key_press_event_t *e;
            e = (xcb_key_press_event_t *)ev;
            PDEBUG("Key %d pressed \n", e->detail);

            int i;
            for (i=0; i<LENGTH(keys); i++)
            {
                /*PDEBUG("config key = %d , key pressed = %d\n", */
                        /*keys[i].keysym, e->detail);*/

                if (keys[i].keysym == e->detail
                    && (keys[i].mod & ~0x80) == (e->state & ~0x80)
                    && keys[i].function)
                {
                    keys[i].function(keys[i].x, keys[i].y);
                    xcb_flush(conn); 
                    break;
                }
            }
        }
        break;

        } /* switch */
        free(ev);
    }
}

static void grab_keys(void)
{
    int i;

    for (i=0; i<LENGTH(keys); i++)
    {
        xcb_grab_key(conn, 1, screen->root, keys[i].mod, keys[i].keysym, 
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
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
    xcb_drawable_t root = screen->root;

    screen_width = screen->width_in_pixels-(2*BORDER_WIDTH);
    screen_height = screen->height_in_pixels-(2*BORDER_WIDTH);
    head = NULL;
    current = NULL;

    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
                       | XCB_EVENT_MASK_STRUCTURE_NOTIFY
                       | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY};
    xcb_change_window_attributes_checked(conn, root, mask, values);
    xcb_flush(conn);

    grab_keys();

    handle_events();

    exit(0);
}
