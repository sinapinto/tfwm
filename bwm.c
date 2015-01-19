/* TODO: fix focus*/
/* TODO: multiple desktops? */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <signal.h>
#include <xcb/xcb.h>

/*#define DEBUG*/

/*#ifdef DEBUG
#include "events.h"
#endif*/

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
static client *focuswin;
static int screen_height;
static int screen_width;

static void cleanup();
static void sighandle(int signal);
static void killwin();
static void nextwin();
static void resize(int x, int y);
static void move(int x, int y);
static void maximize();
static void add_window(xcb_window_t w);
static void remove_window(xcb_window_t w);
static void warp_pointer(struct client *c);
static void focus_client(struct client *c);
static void event_loop(void);

#include "config.h"

static void cleanup()
{
    xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
                        XCB_CURRENT_TIME);
    xcb_flush(conn);

    fprintf(stdout, "\033[032mCleaning up and exiting... Bye!\033[0m\n");
    xcb_disconnect(conn);

    exit(0);
}

static void sighandle(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        fprintf(stdout, "\033[032mReceived a terminating signal.\033[0m\n");
        cleanup();
    }
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

            PDEBUG("\033[031mkillwin\033[0m\n");
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
        if(current->next == NULL) // we at tail.. go back to head
        {
            if (current == head) // this is the only client, don't do anything
                return;
            PDEBUG("\033[031mnextwin [going to head]\033[0m\n");
            c = head;
        }
        else
        {
            PDEBUG("\033[031mnextwin\033[0m\n");
            c = current->next;
        }

        current = c;
        focus_client(c);
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

    /*PDEBUG("X: %d -> %d, Y:%d -> %d\n", r->width, r->width + x,
                                        r->height, r->height + y);*/

    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_WIDTH
                                   | XCB_CONFIG_WINDOW_HEIGHT, values);
    xcb_flush(conn); 
    warp_pointer(current);

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

    /*PDEBUG("X: %d -> %d, Y:%d -> %d\n", r->x, r->x + x,
                                        r->y, r->y + y);*/

    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_X
                                   | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(conn); 
    warp_pointer(current);

    free(r);
}

static void maximize()
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
    warp_pointer(current);
}

static void add_window(xcb_window_t w)
{
    client *c,*t;

    if ((c = malloc(sizeof(client))) == NULL)
    {
        fprintf(stderr, "Error calloc!");
        exit(1);
    }

    if (head == NULL)
    {
        PDEBUG("add_window [new head]\n");
        c->next = NULL;
        c->prev = NULL;
        c->win = w;
        head = c;
    }
    else
    {
        PDEBUG("add_window\n");
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

static void remove_window(xcb_window_t w)
{
    client *c;

    for(c=head; c != NULL; c=c->next) {
        if(c->win == w) {
            PDEBUG("remove_window\n");
            if(c->prev == NULL && c->next == NULL) { // one client in list
                PDEBUG("[removing head]\n");
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

/* warp pointer to lower right */
static void warp_pointer(struct client *c)
{
    xcb_get_geometry_reply_t *geom;
    geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, c->win), NULL);
    if (NULL == geom) return;
    xcb_warp_pointer(conn, XCB_NONE, c->win, 0, 0, 0, 0, 
                    geom->width, geom->height);
    free(geom);
}

static void focus_client(struct client *c)
{
    uint32_t values[2];
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

    /* uncofus the previously focused window */
    if (NULL != focuswin)
    {
        values[0] = NORMCOLOR;
        xcb_change_window_attributes(conn, focuswin->win, XCB_CW_BORDER_PIXEL,
                                    values);
        xcb_flush(conn);
    }

    PDEBUG("setting input focus\n");
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win,
                        XCB_CURRENT_TIME);

    xcb_flush(conn);

    focuswin = c;
}

static void event_loop(void)
{
    xcb_generic_event_t *ev;

    /* block until we receive an event */
    while ( (ev = xcb_wait_for_event(conn)) )
    {
        /* CREATE_NOTIFY -> CONFIGURE_NOTIFY -> MAP_NOTIFY */ 
        /*PDEBUG("Event: %s\n", evnames[ev->response_type]);*/

        switch ( ev->response_type & ~0x80 ) 
        {
        case XCB_CREATE_NOTIFY:
        {
            xcb_create_notify_event_t *e;
            e = (xcb_create_notify_event_t *)ev;

            uint32_t values[2];
            values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
            values[1] = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
            xcb_change_window_attributes(conn, e->window, XCB_CW_EVENT_MASK, values);

            values[0] = BORDER_WIDTH;
            xcb_configure_window(conn, e->window, XCB_CONFIG_WINDOW_BORDER_WIDTH, values);

            add_window(e->window);
            focus_client(current);
            warp_pointer(current);
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

            int i;
            for (i=0; i<LENGTH(keys); i++)
            {
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

        case XCB_ENTER_NOTIFY:
        {
            xcb_enter_notify_event_t *e;
            e = (xcb_enter_notify_event_t *)ev;
            client *c;
            
            if (e->event != focuswin->win)
            {
                for (c=head; c != NULL; c=c->next)
                {
                    if(e->event == c->win)
                    {
                        focus_client(c);
                        break;
                    }
                }
            }
        }
        break;

        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t *e;
            e = (xcb_configure_notify_event_t *)ev;
            client *c;
            for (c=head; c != NULL; c=c->next)
            {
                if(e->window == c->win)
                {
                    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
                                        c->win, XCB_CURRENT_TIME);
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

    /* set up globals */
    screen_width = screen->width_in_pixels-(2*BORDER_WIDTH);
    screen_height = screen->height_in_pixels-(2*BORDER_WIDTH);
    head = NULL;
    current = NULL;

    /* register for events */
    uint32_t values[1];
    uint32_t mask = XCB_CW_EVENT_MASK;
    values[0] =  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_change_window_attributes_checked(conn, root, mask, values);

    /* grab keys */
    for (int i=0; i<LENGTH(keys); i++)
    {
        xcb_grab_key(conn, 1, screen->root, keys[i].mod, keys[i].keysym, 
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
    xcb_flush(conn);

    fprintf(stdout, "\033[033mWelcome to BWM!\033[0m\n");

    event_loop();

    exit(0);
}
