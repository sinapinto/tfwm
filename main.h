/* See LICENSE file for copyright and license details. */
#ifndef MAIN_H
#define MAIN_H
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <X11/Xlib-xcb.h>
#include <stdbool.h>
#include "keys.h"

/* types */
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
	char        *com;
	enum action  i;
} Arg;

typedef struct {
	const char   *class;
	unsigned int  workspace;
	bool         *fullscreen;
	bool          border;
} Rule;

typedef struct Key {
	unsigned int mod;
	xcb_keysym_t keysym;
	void (*func)(const Arg *);
	Arg    arg;
} Key;

typedef struct {
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg    arg;
} Button;

#define MAX_STATE    6
#define REMOVE_STATE XCB_EWMH_WM_STATE_REMOVE   /* 0 */
#define ADD_STATE    XCB_EWMH_WM_STATE_ADD      /* 1 */
#define TOGGLE_STATE XCB_EWMH_WM_STATE_TOGGLE   /* 2 */
enum { 
	EWMH_MAXIMIZED_VERT    = (1 << 0),
	EWMH_MAXIMIZED_HORZ    = (1 << 1),
	EWMH_STICKY            = (1 << 2),
	EWMH_FULLSCREEN        = (1 << 3),
	EWMH_DEMANDS_ATTENTION = (1 << 4),
	EWMH_ABOVE             = (1 << 5)
};

typedef struct Client Client;
struct Client {
	xcb_rectangle_t   geom;
	xcb_rectangle_t   old_geom;
	xcb_size_hints_t  size_hints;
	int32_t           wm_hints;
	uint32_t          ewmh_flags;
	bool              noborder;
	bool              can_focus;
	bool              can_delete;
	xcb_window_t      frame;
	Client           *next;
	Client           *snext;
	xcb_window_t      win;
	unsigned int      ws;
};

enum {
	XC_LEFT_PTR,
	XC_FLEUR,
	XC_BOTTOM_RIGHT_CORNER,
	XC_WATCH,
	XC_MAX
};

/* functions */
void applyrules(Client *c);
void attach(Client *c);
void attachstack(Client *c);
void detach(Client *c);
void detachstack(Client *c);
void err(const char *fmt, ...);
void fitclient(Client *c);
void focus(Client *c);
void focusstack(bool next);
void free_cursors(void);
#ifdef DEBUG
char *get_atom_name(xcb_atom_t atom);
#endif
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
void grabkeys(void);
void handleevent(xcb_generic_event_t *ev);
void killselected(const Arg *arg);
void load_cursors(void);
void manage(xcb_window_t w);
void maprequest(xcb_generic_event_t *ev);
void maximizeaxis(const Arg *arg);
void maximizeaxis_client(Client *c, uint16_t direction);
void maximizeclient(Client *c, bool doit);
void maximize(const Arg *arg);
void mousemotion(const Arg *arg);
void move(const Arg *arg);
void moveresize_win(xcb_window_t win, int16_t x, int16_t y, uint16_t w, uint16_t h);
void movewin(xcb_window_t win, int16_t x, int16_t y);
void quit(const Arg *arg);
void raisewindow(xcb_drawable_t win);
void resize(const Arg *arg);
void resizewin(xcb_window_t win, uint16_t w, uint16_t h);
void restart(const Arg *arg);
void savegeometry(Client *c);
void send_client_message(Client *c, xcb_atom_t proto);
void sendtows(const Arg *arg);
void setborder(Client *c, bool focus);
void setborderwidth(xcb_window_t win, uint16_t bw);
void showhide(Client *c);
void spawn(const Arg *arg);
void teleport(const Arg *arg);
void teleport_client(Client *c, uint16_t location);
void maximize_half(const Arg *arg);
void maximize_half_client(Client *c, uint16_t location);
void unmanage(Client *c);
void updatenumlockmask(void);
void warn(const char *fmt, ...);
void warp_pointer(Client *c);
Client *wintoclient(xcb_window_t w);

/* globals */
extern const Rule rules[RULE_MAX];
extern Key keys[KEY_MAX];
extern Button buttons[BUTTON_MAX];
extern xcb_connection_t *conn;
extern xcb_screen_t *screen;
extern unsigned int numlockmask;
extern int scrno;
extern Client *stack;
extern xcb_ewmh_connection_t *ewmh;
extern xcb_atom_t WM_DELETE_WINDOW;
extern xcb_atom_t WM_TAKE_FOCUS;
extern xcb_atom_t WM_PROTOCOLS;
extern Display *display;
extern unsigned int selws;
extern unsigned int prevws;
extern Client *clients;
extern Client *sel;

#endif /* MAIN_H */
