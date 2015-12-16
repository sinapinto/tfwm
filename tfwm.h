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

#define ROOT_EVENT_MASK    (XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY      \
                            | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT  \
                            | XCB_EVENT_MASK_BUTTON_PRESS)

#define CLIENT_EVENT_MASK  (XCB_EVENT_MASK_ENTER_WINDOW             \
                            | XCB_EVENT_MASK_PROPERTY_CHANGE)

#define POINTER_EVENT_MASK (XCB_EVENT_MASK_BUTTON_PRESS             \
                            | XCB_EVENT_MASK_BUTTON_RELEASE         \
                            | XCB_EVENT_MASK_BUTTON_MOTION          \
                            | XCB_EVENT_MASK_POINTER_MOTION)

#define FRAME_EVENT_MASK   (POINTER_EVENT_MASK                      \
                            | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY    \
                            | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT)

#define CLEANMASK(mask) (mask & ~(numlockmask|XCB_MOD_MASK_LOCK))
#define LENGTH(X)       (sizeof(X)/sizeof(*X))
#define MAX(X, Y)       ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y)       ((X) < (Y) ? (X) : (Y))
#define WIDTH(C)        ((C)->geom.width + 2 * border_width)
#define ISVISIBLE(C)    ((C)->ws == selws)

/* types */
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
enum { XC_LEFT_PTR, XC_FLEUR, XC_BOTTOM_RIGHT_CORNER, XC_MAX };

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
#ifdef SHAPE
void roundcorners(Client *c);
#endif
void savegeometry(Client *c);
void send_client_message(Client *c, xcb_atom_t proto);
void sendtows(const Arg *arg);
void setborder(Client *c, bool focus);
void setborderwidth(xcb_window_t win, uint16_t bw);
void showhide(Client *c);
void spawn(const Arg *arg);
void teleport(const Arg *arg);
void unmanage(Client *c);
void updatenumlockmask(void);
void warn(const char *fmt, ...);
void warp_pointer(Client *c);
Client *wintoclient(xcb_window_t w);

/* globals */
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
extern bool shape_ext;
extern xcb_atom_t WM_DELETE_WINDOW;
extern xcb_atom_t WM_TAKE_FOCUS;
extern xcb_atom_t WM_PROTOCOLS;
extern Display *display;
extern cursor_t cursors[XC_MAX];
extern unsigned int selws;
extern unsigned int prevws;
extern Client *clients;
extern Client *sel;
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
extern int cursor_position;

#endif /* TFWM_H */
