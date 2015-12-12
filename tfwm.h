/* See LICENSE file for copyright and license details. */
#ifndef TFWM_H
#define TFWM_H
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <X11/Xlib-xcb.h>
#include <stdbool.h>

#ifdef DEBUG
# include <stdio.h>
# define PRINTF(...)  do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
# define PRINTF(...)
#endif

#define ROOT_EVENT_MASK    (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY   |\
                            XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |\
                            XCB_EVENT_MASK_BUTTON_PRESS)
#define CLIENT_EVENT_MASK  (XCB_EVENT_MASK_ENTER_WINDOW          |\
                            XCB_EVENT_MASK_PROPERTY_CHANGE)
#define POINTER_EVENT_MASK (XCB_EVENT_MASK_BUTTON_PRESS          |\
                            XCB_EVENT_MASK_BUTTON_RELEASE        |\
                            XCB_EVENT_MASK_BUTTON_MOTION         |\
                            XCB_EVENT_MASK_POINTER_MOTION)

#define CLEANMASK(mask) (mask & ~(numlockmask|XCB_MOD_MASK_LOCK))
#define LENGTH(X)       (sizeof(X)/sizeof(*X))
#define MAX(X, Y)       ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y)       ((X) < (Y) ? (X) : (Y))
#define WIDTH(C)        ((C)->geom.width + 2 * border_width)
#define ISVISIBLE(C)    ((C)->ws == selws)
#define MAX_ATOMS       4

typedef union {
	const char **com;
	const unsigned int i;
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
	const Arg arg;
} Key;

typedef struct {
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;

 /* enum { */ 
	/* EWMH_MAXIMIZED_VERT = (1 << 0), */
	/* EWMH_MAXIMIZED_HORZ = (1 << 1), */
	/* EWMH_FULLSCREEN     = (1 << 2), */
	/* EWMH_BELOW          = (1 << 3), */
	/* EWMH_ABOVE          = (1 << 4) */
 /* }; */

typedef struct Client Client;
struct Client {
	xcb_rectangle_t geom;
	xcb_rectangle_t old_geom;
	xcb_size_hints_t size_hints;
	xcb_icccm_wm_hints_t wm_hints;
	/* uint32_t ewmh_flags; */

	// TODO: remove these in favor of ewmh_flags
	bool ismax, isvertmax, ishormax;
	bool noborder;

	bool can_focus;
	bool can_delete;
	xcb_window_t frame;
	Client *next;
	Client *snext;
	xcb_window_t win;
	unsigned int ws;
};

typedef struct cursor_t {
	char *name;
	uint8_t cf_glyph;
	xcb_cursor_t cid;
} cursor_t;


enum { MoveDown, MoveRight, MoveUp, MoveLeft };
enum { GrowHeight, GrowWidth, ShrinkHeight, ShrinkWidth, GrowBoth, ShrinkBoth };
enum { Center, TopLeft, TopRight, BottomLeft, BottomRight };
enum { MouseMove, MouseResize };
enum { MaxVertical, MaxHorizontal };
enum { LastWorkspace, PrevWorkspace, NextWorkspace };
enum { PrevWindow, NextWindow };
enum {
	XC_LEFT_PTR,
	XC_FLEUR,
	XC_BOTTOM_RIGHT_CORNER,
	XC_MAX
};


/* events.c */
void handleevent(xcb_generic_event_t *ev);
void mousemotion(const Arg *arg);
void maprequest(xcb_generic_event_t *ev);
#ifdef DEBUG
char * get_atom_name(xcb_atom_t atom);
#endif

/* pointer.c */
void load_cursors(void);
void free_cursors(void);

/* keys.c */
void updatenumlockmask(void);

/* util.c */
void warn(const char *fmt, ...);
void err(const char *fmt, ...);
void spawn(const Arg *arg);

/* list.c */
void attach(Client *c);
void attachstack(Client *c);
void detach(Client *c);
void detachstack(Client *c);
void focus(Client *c);
void focusstack(bool next);

/* tfwm.c */
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
void grabkeys(void);
void quit(const Arg *arg);
void restart(const Arg *arg);

extern const Rule rules[2];
extern Key keys[61];
extern Button buttons[2];
extern xcb_connection_t *conn;
extern xcb_screen_t *screen;
extern unsigned int numlockmask;
extern int scrno;
extern Client *stack;
extern xcb_ewmh_connection_t *ewmh;
extern uint32_t focuscol, unfocuscol, outercol;
extern bool dorestart;
extern xcb_atom_t WM_DELETE_WINDOW;
extern xcb_atom_t WM_TAKE_FOCUS;
extern xcb_atom_t WM_PROTOCOLS;
extern Display *display;
extern cursor_t cursors[XC_MAX];
extern bool double_border;
extern int border_width;
extern int outer_border_width;
extern char *focus_color;
extern char *outer_color;
extern char *unfocus_color;
extern int move_step;
extern int resize_step;
extern bool sloppy_focus;
extern bool java_workaround;

/* client.c */
void applyrules(Client *c);
void fitclient(Client *c);
void killselected(const Arg *arg);
void manage(xcb_window_t w);
void maximize(const Arg *arg);
void maximizeaxis(const Arg *arg);
void maximizeclient(Client *c, bool doit);
void move(const Arg *arg);
void movewin(xcb_window_t win, int x, int y);
void moveresize(Client *c, int w, int h, int x, int y);
void raisewindow(xcb_drawable_t win);
void resize(const Arg *arg);
void resizewin(xcb_window_t win, int w, int h);
void savegeometry(Client *c);
void send_client_message(Client *c, xcb_atom_t proto);
void sendtows(const Arg *arg);
void setborder(Client *c, bool focus);
void setborderwidth(Client *c, int width);
void showhide(Client *c);
void teleport(const Arg *arg);
void unmanage(Client *c);
Client *wintoclient(xcb_window_t w);

extern unsigned int selws;
extern unsigned int prevws;
extern Client *clients;
extern Client *sel;

#ifdef SHAPE
void roundcorners(Client *c);
#endif

#endif /* TFWM_H */
