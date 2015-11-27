/* See LICENSE file for copyright and license details. */
#ifndef TFWM_H
#define TFWM_H
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>
#include <stdbool.h>

#ifdef DEBUG
# include <stdio.h>
# define PRINTF(...)     do { printf(__VA_ARGS__); } while(0)
#else
# define PRINTF(...)
#endif

#define CLEANMASK(mask)  (mask & ~(numlockmask|XCB_MOD_MASK_LOCK))
#define LENGTH(X)        (sizeof(X)/sizeof(*X))
#define MAX(X, Y)        ((X) > (Y) ? (X) : (Y))
#define MIN(X, Y)        ((X) < (Y) ? (X) : (Y))
#define WIDTH(C)         ((C)->geom.width + 2 * BORDER_WIDTH)
#define ISVISIBLE(C)     ((C)->ws == selws || (C)->isfixed)

#define MOD                  XCB_MOD_MASK_1
#define SHIFT                XCB_MOD_MASK_SHIFT
#define CTRL                 XCB_MOD_MASK_CONTROL
#define DOUBLE_BORDER        false
#define BORDER_WIDTH         3
#define OUTER_BORDER_WIDTH   4
#define FOCUS_COLOR          "tomato"
#define OUTER_COLOR          "black"
#define UNFOCUS_COLOR        "slate gray"
#define MOVE_STEP            30
#define RESIZE_STEP          30

typedef union {
	const char **com;
	const int i;
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

typedef struct Client Client;
struct Client{
	xcb_rectangle_t geom;
	xcb_rectangle_t oldgeom;
	int32_t basew, baseh, minw, minh, incw, inch;
	bool ismax, isvertmax, ishormax;
	bool isfixed, noborder;
	// TODO Client *parent;
	Client *next;
	Client *snext;
	xcb_window_t win;
	unsigned int ws;
};

enum { MoveDown, MoveRight, MoveUp, MoveLeft };
enum { GrowHeight, GrowWidth, ShrinkHeight, ShrinkWidth, GrowBoth, ShrinkBoth };
enum { ToCenter, ToTop, ToLeft, ToBottom, ToRight };
enum { MouseMove, MouseResize };
enum { MaxVertical, MaxHorizontal };
enum { LastWorkspace, PrevWorkspace, NextWorkspace };
enum { PrevWindow, NextWindow };

/* util.c */
void warn(const char *fmt, ...);
void err(const char *fmt, ...);

/* events.c */
void buttonpress(xcb_generic_event_t *ev);
void circulaterequest(xcb_generic_event_t *ev);
void clientmessage(xcb_generic_event_t *ev);
void configurerequest(xcb_generic_event_t *ev);
void destroynotify(xcb_generic_event_t *ev);
void enternotify(xcb_generic_event_t *ev);
void keypress(xcb_generic_event_t *ev);
void mappingnotify(xcb_generic_event_t *ev);
void maprequest(xcb_generic_event_t *ev);
void mousemotion(const Arg *arg);
void requesterror(xcb_generic_event_t *ev);
void unmapnotify(xcb_generic_event_t *ev);

/* list.c */
void attach(Client *c);
void attachstack(Client *c);
void detach(Client *c);
void detachstack(Client *c);
void focus(Client *c);
void focusstack(const Arg *arg);

/* workspace.c */
void selectrws(const Arg* arg);
void selectws(const Arg* arg);

/* tfwm.c */
xcb_keycode_t *getkeycodes(xcb_keysym_t keysym);
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
void grabbuttons(Client *c);
void grabkeys();
void quit(const Arg *arg);
void restart(const Arg *arg);
void sigcatch(int sig);
void sigchld();
void spawn(const Arg *arg);
void testcookie(xcb_void_cookie_t cookie, char *errormsg);

extern const Rule rules[2];
extern Key keys[100];
extern Button buttons[2];
extern xcb_connection_t *conn;
extern xcb_screen_t *screen;
extern unsigned int numlockmask;
extern int scrno;
extern Client *stack;
extern int sigcode;
extern void (*handler[XCB_NO_OPERATION])(xcb_generic_event_t *ev);
extern xcb_ewmh_connection_t *ewmh;
extern uint32_t focuscol, unfocuscol, outercol;
extern bool dorestart;
extern xcb_atom_t WM_DELETE_WINDOW;

/* client.c */
void applyrules(Client *c);
void fitclient(Client *c);
void gethints(Client *c);
void killselected(const Arg *arg);
void manage(xcb_window_t w);
void maximize(const Arg *arg);
void maximizeaxis(const Arg *arg);
void maximizeclient(Client *c, bool add);
void move(const Arg *arg);
void movewin(xcb_window_t win, int x, int y);
void moveresize(Client *c, int w, int h, int x, int y);
void raisewindow(xcb_drawable_t win);
void resize(const Arg *arg);
void resizewin(xcb_window_t win, int w, int h);
void savegeometry(Client *c);
bool sendevent(Client *c, xcb_atom_t proto);
void sendtows(const Arg *arg);
void setborder(Client *c, bool focus);
void setborderwidth(Client *c, int width);
void showhide(Client *c);
void sticky(const Arg *arg);
void teleport(const Arg *arg);
void unmanage(Client *c);
Client *wintoclient(xcb_window_t w);

extern unsigned int selws;
extern unsigned int prevws;
extern Client *clients;
extern Client *sel;

#endif
