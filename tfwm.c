/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <err.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <X11/keysym.h>

#define CLEANMASK(mask)         (mask & ~(numlockmask|XCB_MOD_MASK_LOCK))
#define LENGTH(X)               (sizeof(X)/sizeof(*X))
#define MAX(X, Y)               ((X) > (Y) ? (X) : (Y))
#define SUBTRACTLIM(X, Y)       ((X - Y > 0) ? (X - Y) : (X))
#define WIDTH(C)                ((C)->w + 2 * BORDER_WIDTH)
#define ISVISIBLE(C)            ((C)->ws == selws || (C)->isfixed)

typedef union {
	const char **com;
	const int i;
} Arg;

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
	int16_t x, y;
	uint16_t w, h;
	int oldx, oldy, oldw, oldh;
	uint16_t basew, baseh, minw, minh;
	bool ismax, isvertmax, ishormax;
	bool isfixed, noborder;
	Client *next;
	Client *snext;
	xcb_window_t win;
	unsigned int ws;
};

/* function declarations */
static void applyrules(Client *c);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(xcb_generic_event_t *ev);
static void centerwin(const Arg *arg);
static void circulaterequest(xcb_generic_event_t *ev);
static void cleanup();
static void clientmessage(xcb_generic_event_t *ev);
static void configurerequest(xcb_generic_event_t *ev);
static void destroynotify(xcb_generic_event_t *ev);
static void detach(Client *c);
static void detachstack(Client *c);
static void enternotify(xcb_generic_event_t *ev);
static void fix(const Arg *arg);
static void focus(struct Client *c);
static void focusstack(const Arg *arg);
static void getatoms(xcb_atom_t *atoms, char **names, int count);
static void gethints(Client *c);
static xcb_keycode_t* getkeycodes(xcb_keysym_t keysym);
static xcb_keysym_t getkeysym(xcb_keycode_t keycode);
static void grabbuttons(Client *c);
static void grabkeys();
static void keypress(xcb_generic_event_t *ev);
static void killclient(const Arg *arg);
static void manage(xcb_window_t w);
static void mappingnotify(xcb_generic_event_t *ev);
static void maprequest(xcb_generic_event_t *ev);
static void maximize(const Arg *arg);
static void maximizeaxis(const Arg *arg);
static void mousemotion(const Arg *arg);
static void move(const Arg *arg);
static void moveresize(Client *c, int w, int h, int x, int y);
static void movewin(xcb_window_t win, int x, int y);
static void quit(const Arg *arg);
static void raisewindow(xcb_drawable_t win);
static void resize(const Arg *arg);
static void resizewin(xcb_window_t win, int w, int h);
static void run(void);
static void savegeometry(Client *c);
static void selectprevws(const Arg* arg);
static void selectws(const Arg* arg);
static bool sendevent(Client *c, xcb_atom_t proto);
static void sendtows(const Arg *arg);
static void setbordercolor(Client *c, long color);
static void setborderwidth(Client *c, int width);
static void setup();
static void showhide(Client *c);
static void sigcatch(int sig);
static void sigchld();
static void spawn(const Arg *arg);
static void testcookie(xcb_void_cookie_t cookie, char *errormsg);
static void unfocus(Client *c);
static void unmanage(Client *c);
static void unmapnotify(xcb_generic_event_t *ev);
static void unmaximize(Client *c);
static void updatenumlockmask();
static Client* wintoclient(xcb_window_t w);

/* enums */
enum { MoveDown, MoveRight, MoveUp, MoveLeft };
enum { GrowHeight, GrowWidth, ShrinkHeight, ShrinkWidth, GrowBoth, ShrinkBoth };
enum { MouseMove, MouseResize };
enum { MaxVertical, MaxHorizontal };
enum { WMProtocols, WMDeleteWindow, WMState, WMLast }; /* default atoms */
enum { NetSupported, NetWMFullscreen, NetWMState, NetActiveWindow,
	NetWMDesktop, NetCurrentDesktop, NetNumberOfDesktops, NetLast }; /* EWMH atoms */

/* variables */
static xcb_connection_t *conn;
static xcb_screen_t *screen;
static unsigned int sw, sh;
static unsigned int selws = 0;
static unsigned int prevws = 0;
static unsigned int numlockmask = 0;
static int scrno;
static Client *clients;
static Client *sel;
static Client *stack;
static int sigcode;
static void (*handler[XCB_NO_OPERATION])(xcb_generic_event_t *ev);
xcb_ewmh_connection_t *ewmh;
static xcb_atom_t wmatom[WMLast];
static xcb_atom_t netatom[NetLast];
static char *wmatomnames[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW", "WM_STATE" };
static char *netatomnames[] = { "_NET_SUPPORTED", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE",
	"_NET_ACTIVE_WINDOW", "_NET_WM_DESKTOP", "_NET_CURRENT_DESKTOP", "_NET_NUMBER_OF_DESKTOPS" };

#include "config.h"

void
applyrules(Client *c) {
	unsigned int i;

	if (!c)
		return;
	xcb_icccm_get_wm_class_reply_t ch;
	if (xcb_icccm_get_wm_class_reply(conn, xcb_icccm_get_wm_class(conn, c->win), &ch, NULL)) {
		for (i = 0; i < LENGTH(noborder); i++) {
			if (strstr(ch.class_name, noborder[i])) {
				setborderwidth(c, 0);
				c->noborder = true;
				return;
			}
		}
	}
}

void
attach(Client *c) {
	c->next = clients;
	clients = c;
}

void
attachstack(Client *c) {
	c->snext = stack;
	stack = c;
}

void
buttonpress(xcb_generic_event_t *ev) {
	xcb_button_press_event_t *e = (xcb_button_press_event_t*)ev;
	Client *c;
	if ((c = wintoclient(e->event)))
		focus(c);
	for (int i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].button == e->detail &&
				CLEANMASK(buttons[i].mask) == CLEANMASK(e->state) &&
				buttons[i].func)
			if (sel != NULL && e->event != screen->root)
				buttons[i].func(&buttons[i].arg);
}

void
centerwin(const Arg *arg) {
	if (!sel)
		return;
	sel->x = sw - (sel->w + BORDER_WIDTH*2);
	sel->x /= 2;
	sel->y = sh - (sel->h + BORDER_WIDTH*2);
	sel->y /= 2;
	movewin(sel->win, sel->x, sel->y);
}

void
circulaterequest(xcb_generic_event_t *ev) {
	xcb_circulate_request_event_t *e = (xcb_circulate_request_event_t *)ev;
	xcb_circulate_window(conn, e->window, e->place);
}

void
cleanup() {
	while (stack)
		unmanage(stack);

	xcb_ewmh_connection_wipe(ewmh);
	if (ewmh)
		free(ewmh);

	xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
	xcb_flush(conn);
	xcb_disconnect(conn);
}

void
clientmessage(xcb_generic_event_t *ev) {
	xcb_client_message_event_t *e = (xcb_client_message_event_t*)ev;
	Client *c;
	if ((c = wintoclient(e->window))) {
		if (e->type == netatom[NetWMState] &&
				(unsigned)e->data.data32[1] == netatom[NetWMFullscreen]) {
			if (e->data.data32[0] == 0 || e->data.data32[0] == 1)
				maximize(NULL);
		}
		else if (e->type == netatom[NetActiveWindow])
			focus(c);
	}
}

void
configurerequest(xcb_generic_event_t *ev) {
	xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
	Client *c;
	unsigned int v[7];
	int i = 0;

	if ((c = wintoclient(e->window))) {
		if (e->value_mask & XCB_CONFIG_WINDOW_X)
			v[i++] = c->x = e->x;
		if (e->value_mask & XCB_CONFIG_WINDOW_Y)
			v[i++] = c->y = e->y;
		if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
			v[i++] = c->w = e->width;
		if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
			v[i++] = c->h = e->height;
		if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
			v[i++] = e->sibling;
		if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
			v[i++] = e->stack_mode;
		if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
			v[i++] = e->border_width;
		xcb_configure_window(conn, e->window, e->value_mask, v);
	}
	else {
		if (e->value_mask & XCB_CONFIG_WINDOW_X)
			v[i++] = e->x;
		if (e->value_mask & XCB_CONFIG_WINDOW_Y)
			v[i++] = e->y;
		if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
			v[i++] = e->width;
		if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
			v[i++] = e->height;
		if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
			v[i++] = e->sibling;
		if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
			v[i++] = e->stack_mode;
		xcb_configure_window(conn, e->window, e->value_mask, v);
	}
}

void
destroynotify(xcb_generic_event_t *ev) {
	xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
	Client *c;

	if ((c = wintoclient(e->window)))
		unmanage(c);
}

void
detach(Client *c) {
	Client **tc;

	for (tc = &clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c) {
	Client **tc, *t;

	for (tc = &stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if (c == sel) {
		for (t = stack; t && !ISVISIBLE(t); t = t->snext);
		sel = t;
	}
}

void
enternotify(xcb_generic_event_t *ev) {
	xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
	Client *c;
	if (e->mode == XCB_NOTIFY_MODE_NORMAL || e->mode == XCB_NOTIFY_MODE_UNGRAB) {
		if (sel != NULL && e->event == sel->win)
			return;
		if ((c = wintoclient(e->event))) {
			focus(c);
		}
	}
}

void
fix(const Arg *arg) {
	if (!sel)
		return;
	if (sel->isfixed) {
		sel->isfixed = false;
		sel->ws = selws;
		xcb_change_property(conn, XCB_PROP_MODE_REPLACE, sel->win,
				netatom[NetWMDesktop], XCB_ATOM_CARDINAL, 32, 1, &selws);
	}
	else {
		sel->isfixed = true;
		raisewindow(sel->win);
	}
}

void
focus(Client *c) {
	if (!c || !ISVISIBLE(c))
		for (c = stack; c && !ISVISIBLE(c); c = c->snext)
			if (sel && sel != c)
				unfocus(sel);
	if (c) {
		detachstack(c);
		attachstack(c);
		grabbuttons(c);
		setbordercolor(c, FOCUS);
		long data[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
		xcb_change_property(conn, XCB_PROP_MODE_REPLACE, c->win,
				wmatom[WMState], wmatom[WMState], 32, 2, data);
		xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
				c->win, XCB_CURRENT_TIME);
		xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
				netatom[NetActiveWindow] , XCB_ATOM_WINDOW, 32, 1,&c->win);
	}
	else {
		xcb_delete_property(conn, screen->root, netatom[NetActiveWindow]);
		xcb_set_input_focus(conn, XCB_NONE,
				XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
	}
	sel = c;
}

void
focusstack(const Arg *arg) {
	Client *c = NULL, *i;

	if (!sel)
		return;
	setbordercolor(sel, UNFOCUS);
	if (arg->i > 0) {
		for (c = sel->next; c && !ISVISIBLE(c); c = c->next);
		if (!c)
			for (c = clients; c && !ISVISIBLE(c); c = c->next);
	}
	else {
		for (i = clients; i != sel; i = i->next)
			if (ISVISIBLE(i))
				c = i;
		if (!c)
			for (; i; i = i->next)
				if (ISVISIBLE(i))
					c = i;
	}
	if (c) {
		focus(c);
		raisewindow(sel->win);
	}
}

void
getatoms(xcb_atom_t *atoms, char **names, int count) {
	xcb_intern_atom_cookie_t cookies[count];
	unsigned int i;

	for (i = 0; i < count; ++i)
		cookies[i] = xcb_intern_atom(conn, 0, strlen(names[i]), names[i]);
	for (i = 0; i < count; ++i) {
		xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, cookies[i], NULL);
		if (reply) {
			atoms[i] = reply->atom;
			free(reply);
		}
	}
}

void
gethints(Client *c) {
	xcb_size_hints_t h;
	xcb_get_property_cookie_t cookie = xcb_icccm_get_wm_normal_hints_unchecked(conn, c->win);
	xcb_generic_error_t *e;
	xcb_icccm_get_wm_normal_hints_reply(conn, cookie, &h, &e);
	if (e) {
		free(e);
		return;
	}
	free(e);

	if (h.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
		c->minw = h.min_width;
		c->minh = h.min_height;
	}
	if (h.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
		c->basew = h.base_width;
		c->baseh = h.base_height;
	}
	/* configure win */
	if (c->basew < sw)
		c->w = MAX(c->w, c->basew);
	if (c->baseh < sh)
		c->h = MAX(c->h, c->baseh);
	if (c->minw < sw)
		c->w = MAX(c->w, c->minw);
	if (c->minh < sh)
		c->h = MAX(c->h, c->minh);

	resizewin(c->win, c->w, c->h);
}

xcb_keycode_t *
getkeycodes(xcb_keysym_t keysym) {
	xcb_key_symbols_t *keysyms;
	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err(EXIT_FAILURE, "can't get keycode.");
	xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
	xcb_key_symbols_free(keysyms);
	return keycode;
}

xcb_keysym_t
getkeysym(xcb_keycode_t keycode) {
	xcb_key_symbols_t *keysyms;
	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err(EXIT_FAILURE, "can't get keysym.");
	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
	xcb_key_symbols_free(keysyms);
	return keysym;
}

void
grabbuttons(Client *c) {
	unsigned int i, j;
	unsigned int modifiers[] = { 0, XCB_MOD_MASK_LOCK, numlockmask, numlockmask|XCB_MOD_MASK_LOCK };

	for (i = 0; i < LENGTH(buttons); i++)
		for (j = 0; j < LENGTH(modifiers); j++)
			xcb_grab_button(conn, 1, c->win, XCB_EVENT_MASK_BUTTON_PRESS,
					XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root,
					XCB_NONE, buttons[i].button, buttons[i].mask|modifiers[j]);
}

void
grabkeys() {
	unsigned int i, j, k;
	unsigned int modifiers[] = { 0, XCB_MOD_MASK_LOCK, numlockmask, numlockmask|XCB_MOD_MASK_LOCK };
	xcb_keycode_t *keycode;

	xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
	for (i = 0; i < LENGTH(keys); ++i) {
		keycode = getkeycodes(keys[i].keysym);
		for (j = 0; keycode[j] != XCB_NO_SYMBOL; j++)
			for (k = 0; k < LENGTH(modifiers); k++)
				xcb_grab_key(conn, 1, screen->root, keys[i].mod | modifiers[k],
						keycode[j], XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	}
}

void
keypress(xcb_generic_event_t *ev) {
	xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
	xcb_keysym_t keysym = getkeysym(e->detail);
	for (int i=0; i < LENGTH(keys); i++) {
		if (keysym == keys[i].keysym &&
				CLEANMASK(keys[i].mod) == CLEANMASK(e->state) &&
				keys[i].func) {
			keys[i].func(&keys[i].arg);
			break;
		}
	}
}

void
killclient(const Arg *arg) {
	if (!sel)
		return;
	if (!sendevent(sel, wmatom[WMDeleteWindow])) {
		xcb_kill_client(conn, sel->win);
	}
}

void
manage(xcb_window_t w) {
	Client *c = NULL;
	if (!(c = malloc(sizeof(Client))))
		err(EXIT_FAILURE, "can't allocate memory");
	c->win = w;
	/* geometry */
	xcb_get_geometry_reply_t *geom;
	geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, w), NULL);
	if (!geom)
		err(EXIT_FAILURE, "geometry reply failed");
	c->x = c->oldx = geom->x;
	c->y = c->oldy = geom->y;
	c->w = c->oldw = geom->width;
	c->h = c->oldh = geom->height;
	free(geom);
	gethints(c);
	c->ws = selws;
	c->ismax = c->isvertmax = c->ishormax = c->isfixed = c->noborder = false;
	attach(c);
	attachstack(c);
	sel = c;
	setbordercolor(c, FOCUS);
	setborderwidth(c, BORDER_WIDTH);
	applyrules(c);
	xcb_map_window(conn, w);
	/* set its workspace hint */
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, c->win, netatom[NetWMDesktop],
			XCB_ATOM_CARDINAL, 32, 1, &selws);
	/* set normal state */
	long data[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w, netatom[NetWMState],
			netatom[NetWMState], 32, 2, data);

	xcb_get_property_reply_t *reply;
	xcb_get_property_cookie_t cookie;
	cookie = xcb_get_property(conn, 0, w, netatom[NetWMState], XCB_ATOM_ATOM, 0, 1);

	if ((reply = xcb_get_property_reply(conn, cookie, NULL))) {
		if (xcb_get_property_value_length(reply) != 0) {
			if (reply->format == 32) {
				xcb_atom_t *v = xcb_get_property_value(reply);
				if (v[0] == netatom[NetWMFullscreen])
					maximize(NULL);
			}
		}
		free(reply);
	}
	focus(NULL);
}

void
mappingnotify(xcb_generic_event_t *ev) {
	xcb_mapping_notify_event_t *e = (xcb_mapping_notify_event_t *)ev;
	if (e->request != XCB_MAPPING_MODIFIER && e->request != XCB_MAPPING_KEYBOARD)
		return;
	xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
	grabkeys();
}

void
maprequest(xcb_generic_event_t *ev) {
	xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
	if (sel && sel->win != e->window)
		setbordercolor(sel, UNFOCUS);
	if (!wintoclient(e->window))
		manage(e->window);
}

void
maximize(const Arg *arg) {
	if (!sel)
		return;
	if (sel->ismax) {
		unmaximize(sel);
		return;
	}
	savegeometry(sel);
	sel->ismax = true;
	sel->isvertmax = sel->ishormax = false;
	sel->x = 0;
	sel->y = 0;
	sel->w = sw;
	sel->h = sh;
	moveresize(sel, sel->x, sel->y, sel->w, sel->h);
	focus(NULL);
	setborderwidth(sel, 0);

	long data[] = { sel->ismax ? netatom[NetWMFullscreen] : XCB_ICCCM_WM_STATE_NORMAL };
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, sel->win,
			netatom[NetWMState], XCB_ATOM_ATOM, 32, 1, data);
}

void
maximizeaxis(const Arg *arg) {
	if (!sel || sel->ismax)
		return;
	if (sel->isvertmax || sel->ishormax) {
		unmaximize(sel);
		return;
	}
	savegeometry(sel);
	uint32_t values[3];
	if (arg->i == MaxVertical) {
		sel->y = 0;
		sel->h = sh - (BORDER_WIDTH * 2);
		values[0] = sel->y;
		values[1] = sel->h;

		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_Y
				| XCB_CONFIG_WINDOW_HEIGHT, values);
		sel->isvertmax = true;
	}
	else if (arg->i == MaxHorizontal) {
		sel->x = 0;
		sel->w = sw - (BORDER_WIDTH * 2);
		values[0] = sel->x;
		values[1] = sel->w;
		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_X
				| XCB_CONFIG_WINDOW_WIDTH, values);
		sel->ishormax = true;
	}
}

void
mousemotion(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	raisewindow(sel->win);
	xcb_query_pointer_reply_t *pointer;
	pointer = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, screen->root), 0);
	/* create cursor */
	xcb_font_t font = xcb_generate_id(conn);
	xcb_void_cookie_t fontcookie =
		xcb_open_font_checked(conn, font, strlen("cursor"), "cursor");
	testcookie(fontcookie, "can't open font");
	xcb_cursor_t cursor = xcb_generate_id(conn);
	int cursorid;
	if (arg->i == MouseMove)
		cursorid = 52;
	else
		cursorid = 14;
	xcb_create_glyph_cursor(conn, cursor, font, font,
			cursorid, cursorid + 1, 0, 0, 0, 65535, 65535, 65535);
	/* grab pointer */
	xcb_grab_pointer_reply_t *grab_reply = xcb_grab_pointer_reply(conn,
			xcb_grab_pointer(conn, 0, screen->root,
				XCB_EVENT_MASK_BUTTON_PRESS |
				XCB_EVENT_MASK_BUTTON_RELEASE |
				XCB_EVENT_MASK_BUTTON_MOTION |
				XCB_EVENT_MASK_POINTER_MOTION, XCB_GRAB_MODE_ASYNC,
				XCB_GRAB_MODE_ASYNC, XCB_NONE, cursor, XCB_CURRENT_TIME), NULL);
	if (grab_reply->status != XCB_GRAB_STATUS_SUCCESS) {
		free(grab_reply);
		return;
	}
	free(grab_reply);

	xcb_generic_event_t *ev;
	xcb_motion_notify_event_t *e;
	bool ungrab = false;
	int nx = sel->x;
	int ny = sel->y;
	int nw = sel->w;
	int nh = sel->h;
	while ((ev = xcb_wait_for_event(conn)) && !ungrab) {
		switch (ev->response_type & ~0x80) {
			case XCB_CONFIGURE_REQUEST:
			case XCB_MAP_REQUEST:
				handler[ev->response_type & ~0x80](ev);
				break;
			case XCB_MOTION_NOTIFY:
				e = (xcb_motion_notify_event_t*)ev;
				if (arg->i == MouseMove) {
					nx = sel->x + e->root_x - pointer->root_x;
					ny = sel->y + e->root_y - pointer->root_y;
					movewin(sel->win, nx, ny);
				}
				else {
					nw = MAX(sel->w + e->root_x - pointer->root_x, sel->minw + 40);
					nh = MAX(sel->h + e->root_y - pointer->root_y, sel->minh + 40);
					resizewin(sel->win, nw, nh);
				}
				break;
			case XCB_BUTTON_RELEASE:
				if (arg->i == MouseMove) {
					sel->x = nx;
					sel->y = ny;
				}
				else {
					sel->w = nw;
					sel->h = nh;
				}
				ungrab = true;
		}
		free(ev);
		xcb_flush(conn);
	}
	sel->ismax = false;
	free(ev);
	free(pointer);
	fontcookie = xcb_close_font_checked(conn, font);
	testcookie(fontcookie, "can't close font");
	xcb_free_cursor(conn, cursor);
	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
}

void
move(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	switch (arg->i) {
		case MoveDown:  sel->y += steps[0]; break;
		case MoveRight: sel->x += steps[0]; break;
		case MoveUp:    sel->y -= steps[0]; break;
		case MoveLeft:  sel->x -= steps[0]; break;
		default:        err(EXIT_FAILURE, "bad move argument");
	}
	movewin(sel->win, sel->x, sel->y);
}

void
moveresize(Client *c, int x, int y, int w, int h) {
	if (!c || !c->win)
		return;
	uint32_t val[5] = { x, y, w, h };
	uint32_t mask = XCB_CONFIG_WINDOW_X |
		XCB_CONFIG_WINDOW_Y |
		XCB_CONFIG_WINDOW_WIDTH |
		XCB_CONFIG_WINDOW_HEIGHT;
	xcb_configure_window(conn, c->win, mask, val);
}

void
movewin(xcb_window_t win, int x, int y) {
	unsigned int pos[2] = { x, y };
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y, pos);
}

void
quit(const Arg *arg) {
	cleanup();
	exit(sigcode);
}

void
raisewindow(xcb_drawable_t win) {
	uint32_t values[] = { XCB_STACK_MODE_ABOVE };
	if (screen->root == win || !win)
		return;
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

void
resize(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	int step = steps[1];
	if (arg->i == GrowHeight || arg->i == GrowBoth) {
		sel->h = sel->h + step;
	}
	if (arg->i == GrowWidth || arg->i == GrowBoth) {
		sel->w = sel->w + step;
	}
	if (arg->i == ShrinkHeight || arg->i == ShrinkBoth) {
		sel->h = SUBTRACTLIM(sel->h, step);
	}
	if (arg->i == ShrinkWidth || arg->i == ShrinkBoth) {
		sel->w = SUBTRACTLIM(sel->w, step);
	}
	uint32_t values[2];
	values[0] = sel->w;
	values[1] = sel->h;
	xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_WIDTH
			|XCB_CONFIG_WINDOW_HEIGHT, values);
	if (sel->ismax) {
		sel->ismax = false;
		setborderwidth(sel, BORDER_WIDTH);
	}
	sel->ishormax = false;
	sel->isvertmax = false;
}

void
resizewin(xcb_window_t win, int w, int h) {
	unsigned int values[2] = { w, h };
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_WIDTH |
			XCB_CONFIG_WINDOW_HEIGHT, values);
}

void
run(void) {
	xcb_generic_event_t *ev;
	sigcode = 0;
	while (sigcode == 0) {
		xcb_flush(conn);
		ev = xcb_wait_for_event(conn);
		if (handler[ev->response_type & ~0x80])
			handler[ev->response_type & ~0x80](ev);
		free(ev);
		if (xcb_connection_has_error(conn)) {
			cleanup();
			exit(1);
		}
	}
}

void
selectprevws(const Arg* arg) {
	const Arg a = { .i = prevws };
	selectws(&a);
}

void
selectws(const Arg* arg) {
	if (selws == arg->i)
		return;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
			netatom[NetCurrentDesktop], XCB_ATOM_CARDINAL, 32, 1, &arg->i);
	xcb_ewmh_set_current_desktop(ewmh, scrno, arg->i);
	prevws = selws;
	selws = arg->i;
	focus(NULL);
	showhide(stack);
}

bool
sendevent(Client *c, xcb_atom_t proto) {
	xcb_client_message_event_t ev;
	xcb_get_property_cookie_t cookie;
	xcb_icccm_get_wm_protocols_reply_t reply;
	bool exists = false;
	cookie = xcb_icccm_get_wm_protocols(conn, c->win, wmatom[WMProtocols]);
	if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &reply, NULL) == 1) {
		for (int i = 0; i < reply.atoms_len && !exists; i++)
			if (reply.atoms[i] == proto)
				exists = true;
		xcb_icccm_get_wm_protocols_reply_wipe(&reply);
	}
	if (exists) {
		ev.response_type = XCB_CLIENT_MESSAGE;
		ev.format = 32;
		ev.sequence = 0;
		ev.window = c->win;
		ev.type = wmatom[WMProtocols];
		ev.data.data32[0] = proto,
			ev.data.data32[1] = XCB_CURRENT_TIME;
		xcb_send_event(conn, true, c->win, XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
	}
	return exists;
}

void
sendtows(const Arg *arg) {
	if (arg->i == selws)
		return;
	sel->ws = arg->i;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, sel->win,
			netatom[NetWMDesktop], XCB_ATOM_CARDINAL, 32, 1, &arg->i);
	showhide(stack);
	focus(NULL);
}

void
setbordercolor(Client *c, long color) {
	if (c->noborder)
		return;
	uint32_t value[1] = { color };
	xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, value);
}

void
setborderwidth(Client *c, int width) {
	if (c->noborder)
		return;
	uint32_t value[1] = { width };
	xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, value);
}

void
setup() {
	sigchld();
	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);
	/* init screen */
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	if (!screen)
		err(EXIT_FAILURE, "can't find screen.");
	sw = screen->width_in_pixels;
	sh = screen->height_in_pixels;
	/* subscribe to handler */
	unsigned int value[1] = {
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
			XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
			XCB_EVENT_MASK_BUTTON_PRESS
	};
	xcb_void_cookie_t cookie;
	cookie = xcb_change_window_attributes_checked(conn, screen->root,
			XCB_CW_EVENT_MASK, value);
	testcookie(cookie, "another window manager is running.");
	xcb_flush(conn);
	/* init atoms */
	getatoms(wmatom, wmatomnames, WMLast);
	getatoms(netatom, netatomnames, NetLast);
	/* init ewmh */
	ewmh = malloc(sizeof(xcb_ewmh_connection_t));
	if (xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh), NULL) == 0)
		err(EXIT_FAILURE, "can't initialize ewmh.");
	xcb_drawable_t root = screen->root;
	uint32_t mask = XCB_CW_EVENT_MASK;
	uint32_t values[] = { XCB_EVENT_MASK_POINTER_MOTION };
	xcb_window_t recorder = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, recorder, root, 0, 0,
			screen->width_in_pixels, screen->height_in_pixels, 0,
			XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, mask, values);
	xcb_ewmh_set_wm_pid(ewmh, recorder, getpid());
	xcb_ewmh_set_wm_name(ewmh, screen->root, 4, "tfwm");
	xcb_ewmh_set_supporting_wm_check(ewmh, recorder, recorder);
	xcb_ewmh_set_supporting_wm_check(ewmh, screen->root, recorder);
	xcb_atom_t net_atoms[] = {
		ewmh->_NET_SUPPORTED,
		ewmh->_NET_NUMBER_OF_DESKTOPS,
		ewmh->_NET_CURRENT_DESKTOP,
		ewmh->_NET_ACTIVE_WINDOW,
		ewmh->_NET_CLOSE_WINDOW,
		ewmh->_NET_WM_NAME,
		ewmh->_NET_WM_DESKTOP,
		ewmh->_NET_WM_STATE,
		ewmh->_NET_WM_PID,
		ewmh->_NET_WM_STATE_FULLSCREEN
	};
	xcb_ewmh_set_supported(ewmh, scrno, LENGTH(net_atoms), net_atoms);
	static const uint8_t numworkspaces = 5;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
			netatom[NetCurrentDesktop], XCB_ATOM_CARDINAL,
			32, 1, &selws);
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
			netatom[NetNumberOfDesktops], XCB_ATOM_CARDINAL,
			32, 1, &numworkspaces);
	xcb_ewmh_set_number_of_desktops(ewmh, scrno, numworkspaces);
	xcb_ewmh_set_current_desktop(ewmh, scrno, 0);
	/* set handlers */
	handler[XCB_CONFIGURE_REQUEST] = configurerequest;
	handler[XCB_DESTROY_NOTIFY]    = destroynotify;
	handler[XCB_UNMAP_NOTIFY]      = unmapnotify;
	handler[XCB_MAP_REQUEST]       = maprequest;
	handler[XCB_MAPPING_NOTIFY]    = mappingnotify;
	handler[XCB_CLIENT_MESSAGE]    = clientmessage;
	handler[XCB_KEY_PRESS]         = keypress;
	handler[XCB_BUTTON_PRESS]      = buttonpress;
	handler[XCB_ENTER_NOTIFY]      = enternotify;
	handler[XCB_CIRCULATE_REQUEST] = circulaterequest;
	/* init keys */
	updatenumlockmask();
	grabkeys();
	focus(NULL);
}

void
showhide(Client *c) {
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		movewin(c->win, c->x, c->y);
		showhide(c->snext);
	}
	else {
		showhide(c->snext);
		movewin(c->win, WIDTH(c) * -2, c->y);
	}
}

void
sigcatch(int sig) {
	sigcode = sig;
}

void
sigchld() {
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		err(EXIT_FAILURE, "can't install SIGCHLD handler");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg) {
	if (fork() == 0) {
		if (conn)
			close(screen->root);
		setsid();
		execvp((char*)arg->com[0], (char**)arg->com);
		err(EXIT_FAILURE, "execvp %s", ((char **)arg->com)[0]);
	}
}

void
savegeometry(Client *c) {
	c->oldx = c->x;
	c->oldy = c->y;
	c->oldh = c->h;
	c->oldw = c->w;
}

void
testcookie(xcb_void_cookie_t cookie, char *errormsg) {
	xcb_generic_error_t *error = xcb_request_check(conn, cookie);
	if (error) {
		fprintf(stderr, "%s : %d\n", errormsg, error->error_code);
		free(error);
		xcb_disconnect(conn);
		exit(EXIT_FAILURE);
	}
}

void
unfocus(Client *c) {
	if (!c)
		return;
	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, c->win, XCB_CURRENT_TIME);
}

void
unmanage(Client *c) {
	if (!sendevent(c, wmatom[WMDeleteWindow]))
		xcb_kill_client(conn, c->win);
	detach(c);
	detachstack(c);
	free(c);
	focus(NULL);
}

void
unmapnotify(xcb_generic_event_t *ev) {
	xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
	Client *c;
	if (e->window == screen->root)
		return;
	if ((c = wintoclient(e->window)))
		unmanage(c);
}

void
unmaximize(Client *c) {
	if (!c)
		return;
	c->x = c->isvertmax ? c->x : c->oldx;
	c->y = c->ishormax ? c->y : c->oldy;
	c->w = c->oldw;
	c->h = c->oldh;
	c->ismax = c->ishormax = c->isvertmax = 0;
	moveresize(c, c->x, c->y, c->w, c->h);
	setborderwidth(sel, BORDER_WIDTH);
}

void
updatenumlockmask() {
	unsigned int i, j, n;
	numlockmask = 0;
	xcb_keycode_t *modmap ;
	xcb_get_modifier_mapping_reply_t *reply;

	reply = xcb_get_modifier_mapping_reply(conn, xcb_get_modifier_mapping_unchecked(conn), NULL);
	if (!reply)
		err(EXIT_FAILURE, "mod map reply");
	modmap = xcb_get_modifier_mapping_keycodes(reply);
	if (!modmap)
		err(EXIT_FAILURE, "mod map keycodes");

	xcb_keycode_t *numlock = getkeycodes(XK_Num_Lock);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < reply->keycodes_per_modifier; j++) {
			xcb_keycode_t keycode = modmap[i * reply->keycodes_per_modifier + j];
			if (keycode == XCB_NO_SYMBOL)
				continue;
			if (numlock)
				for (n = 0; numlock[n] != XCB_NO_SYMBOL; n++)
					if (numlock[n] == keycode) {
						numlockmask = 1 << i;
						break;
					}
		}
	}
	free(reply);
	free(numlock);
}

Client *
wintoclient(xcb_window_t w) {
	Client *c;
	for (c = clients; c; c = c->next)
		if (w == c->win)
			return c;
	return NULL;
}

int
main() {
	conn = xcb_connect(NULL, &scrno);
	if (xcb_connection_has_error(conn))
		err(EXIT_FAILURE, "xcb_connect error");
	setup();
	run();
	cleanup();
	return EXIT_SUCCESS;
}

/* vi: set noexpandtab noautoindent : */
