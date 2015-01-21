/* TODO: fix focus*/
/* TODO: maximize toggle */
/* TODO: multiple desktops? */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <signal.h>
#include <xcb/xcb.h>

#define DEBUG

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

typedef union {
    const char ** com;
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
    xcb_drawable_t id;
    int16_t x, y;
    uint16_t width, height;
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
static void resize(const Arg *arg);
static void move(const Arg *arg);
static void maximize();
static void add_window_to_list(xcb_window_t w);
static void remove_window_from_list(xcb_window_t w);
static int get_geom(xcb_drawable_t win, int16_t *x, int16_t *y,
                    uint16_t *width, uint16_t *height);
static void warp_pointer(struct client *c);
static void focus_client(struct client *c);
static void setup_win(xcb_window_t w);
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
                PDEBUG("\033[031mnextwin\033[0m\n");
                c = current->next;
            }
            current = c;
            focus_client(c);
        }
        else // focus previous
        {
            if (current == head) // this is the only client, don't do anything
                return;
            if (current->prev == NULL) // at head.. prev win is tail
            {
                for(c=head; c->next != NULL; c=c->next);
            }
            else
            {
                c = current->prev;
            }
            current = c;
            focus_client(c);
        }
    }
}

static void resize(const Arg *arg)
{
    /* don't resize root */
    if (!current->win || current->win == screen->root)
        return;
    uint8_t step = steps[1];

    switch (arg->i)
    {
        case 0: {
            current->height = current->height+step > 5 ? current->height + step 
                                                       : current->height;
        }
        break;
        case 1: {
            current->height = current->height-step > 5 ? current->height - step 
                                                       : current->height;
        }
        break;
        case 2: {
            current->width = current->width-step > 5 ? current->width - step 
                                                     : current->width;
        }
        break;
        case 3: {
            current->width = current->width+step > 5 ? current->width + step 
                                                     : current->width;
        }
        break;
        default:
            return;
    }

    uint32_t values[2];
    values[0] = current->width;
    values[1] = current->height;

    /*PDEBUG("X: %d -> %d, Y:%d -> %d\n", r->width, r->width + x,
                                        r->height, r->height + y);*/

    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_WIDTH
                                   | XCB_CONFIG_WINDOW_HEIGHT, values);
    xcb_flush(conn); 
    warp_pointer(current);
}

static void move(const Arg *arg)
{
    /* don't move root */
    if (!current->win || current->win == screen->root)
        return;
    uint8_t step = steps[0];

    switch (arg->i)
    {
        case 0: {
            current->y += step;
        }
        break;
        case 1: {
            current->y -= step;
        }
        break;
        case 2: {
            current->x -= step;
        }
        break;
        case 3: {
            current->x += step;
        }
        break;
        default:
            return;
    }
    uint32_t values[2];
    values[0] = current->x; 
    values[1] = current->y;

    /*PDEBUG("X: %d -> %d, Y:%d -> %d\n", r->x, r->x + x,
                                        r->y, r->y + y);*/

    xcb_configure_window(conn, current->win, XCB_CONFIG_WINDOW_X
                                   | XCB_CONFIG_WINDOW_Y, values);
    xcb_flush(conn); 
    warp_pointer(current);
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
            current->x = 0;
            current->y = 0;
            current->width = screen_width;
            current->height = screen_height;
            break;
        }
    }
    xcb_flush(conn);
    warp_pointer(current);
}

static void add_window_to_list(xcb_window_t w)
{
    client *c,*t;
    if ((c = malloc(sizeof(client))) == NULL)
    {
        PDEBUG("add_window_to_list: Malloc error\n");
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
        t->prev = c;
        get_geom(c->id, &c->x, &c->y, &c->width, &c->height);
        head = c;
    }
    current = c;
    focus_client(c);
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


static void remove_window_from_list(xcb_window_t w)
{
    client *c;

    for(c=head; c != NULL; c=c->next) {
        if(c->win == w) {
            PDEBUG("remove_window_from_list %d\n", c->win);
            if(c->prev == NULL && c->next == NULL) { // one client in list
                PDEBUG("[removing head]\n");
                free(head);
                head = NULL;
                current = NULL;
                focuswin = NULL;
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

    PDEBUG("setting input focus: %d\n", c->win);
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win,
                        XCB_CURRENT_TIME);

    xcb_flush(conn);

    focuswin = c;
}

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
    warp_pointer(current);
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
            setup_win(e->window);
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
