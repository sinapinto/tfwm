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

#if 1
#   define DEBUG(...) \
		do { fprintf(stderr, "tfwm: ");fprintf(stderr, __VA_ARGS__); } while(0)
#else
#   define DEBUG(...)
#endif

#define Mod1Mask                XCB_MOD_MASK_1
#define Mod4Mask                XCB_MOD_MASK_4
#define ShiftMask               XCB_MOD_MASK_SHIFT
#define ControlMask             XCB_MOD_MASK_CONTROL
#define CLEANMASK(mask)         ((mask & ~0x80))
#define LENGTH(X)               (sizeof(X)/sizeof(*X))
#define MAX(X, Y)               ((X) > (Y) ? (X) : (Y))

typedef union {
	const char **com;
	const int i;
} Arg;

typedef struct Key {
	unsigned int mod;
	xcb_keysym_t keysym;
	void (*function)(const Arg *);
	const Arg arg;
} Key;

typedef struct Client Client;
struct Client{
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, minw, minh;
	bool isfullscreen;
	Client *next;
	Client *snext;
	xcb_window_t win;
	unsigned int workspace;
};

/* function declarations */
static void attach(Client *c);
static void attachstack(Client *c);
static void change_border_color(xcb_window_t win, long color);
static void change_border_width(xcb_window_t win, int width);
static void cleanup();
static void clientmessage(xcb_generic_event_t *ev);
static void configurerequest(xcb_generic_event_t *ev);
static void destroynotify(xcb_generic_event_t *ev);
static void detach(Client *c);
static void detachstack(Client *c);
static void focus(struct Client *c);
static void getatoms(xcb_atom_t *atoms, char **names, int count);
static xcb_keycode_t* getkeycodes(xcb_keysym_t keysym);
static xcb_keysym_t getkeysym(xcb_keycode_t keycode);
static bool isvisible(Client *c);
static void keypress(xcb_generic_event_t *ev);
static void killclient(const Arg *arg);
static void manage(xcb_window_t w);
static void maprequest(xcb_generic_event_t *ev);
static void move(const Arg *arg);
static void quit(const Arg *arg);
static void resize(const Arg *arg);
static void run(void);
static bool sendevent(Client *c, xcb_atom_t proto);
static void setcursor(int cursorid);
static void setup();
static void sigchld();
static void testcookie(xcb_void_cookie_t cookie, char *errormsg);
static void unfocus(struct Client *c);
static void unmanage(Client *c);
static void unmapnotify(xcb_generic_event_t *ev);
static Client *wintoclient(xcb_window_t w);

/* enums */
enum { WMProtocols, WMDeleteWindow, WMState, WMLast }; /* default atoms */
enum { NetSupported, NetWMFullscreen, NetWMState, NetActiveWindow,
	NetLast }; /* EWMH atoms */

/* variables */
static xcb_connection_t *conn;
static xcb_screen_t *screen;
static int sw, sh;
static Client *clients;
static Client *sel;
static Client *stack;
static bool running = true;
static void (*handler[XCB_NO_OPERATION]) (xcb_generic_event_t *ev);
static xcb_atom_t wmatom[WMLast];
static xcb_atom_t netatom[NetLast];
static char *wmatomnames[] = { "WM_PROTOCOLS", "WM_DELETE_WINDOW", "WM_STATE" };
static char *netatomnames[] = { "_NET_SUPPORTED", "_NET_WM_STATE_FULLSCREEN",
	"_NET_WM_STATE", "_NET_ACTIVE_WINDOW" };

#include "config.h"

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
change_border_width(xcb_window_t win, int width) {
	uint32_t value[1] = { width };
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_BORDER_WIDTH, value);
}

void
change_border_color(xcb_window_t win, long color) {
	if (!win || BORDER_WIDTH == 0) return;
	uint32_t value[1] = { color };
	xcb_change_window_attributes(conn, win, XCB_CW_BORDER_PIXEL, value);
}

void
cleanup() {
	while (stack)
		unmanage(stack);

	xcb_set_input_focus(conn, XCB_NONE,
			XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
	xcb_flush(conn);
	xcb_disconnect(conn);
}

void
clientmessage(xcb_generic_event_t *ev) {
	DEBUG("clientmessage\n");
	/* xcb_client_message_event_t *e = (xcb_client_message_event_t*)ev; */
	/* Client *c; */
	/* if (c = wintoclient(e->window)) { */
	/* } */
}

void
configurerequest(xcb_generic_event_t *ev) {
	xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
	DEBUG("confgiruerequest\n");
	Client *c;
	if (!(c = wintoclient(e->window)))
		return;
	unsigned int v[4];
	int i = 0;
	if (e->value_mask & XCB_CONFIG_WINDOW_X) v[i++] = c->x = e->x;
	if (e->value_mask & XCB_CONFIG_WINDOW_Y) v[i++] = c->y = e->y;
	if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) v[i++] = c->w = e->width;
	if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) v[i++] = c->h = e->height;
	xcb_configure_window(conn, e->window, e->value_mask, v);
}

void
destroynotify(xcb_generic_event_t *ev) {
	DEBUG("destroynotify\n");
	xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
	Client *c;

	if ((c = wintoclient(e->window)))
		unmanage(c);
}

void
detach(Client *c) {
	Client **tc;

	for(tc = &clients; *tc && *tc != c; tc = &(*tc)->next);
	*tc = c->next;
}

void
detachstack(Client *c) {
	Client **tc, *t;

	for(tc = &stack; *tc && *tc != c; tc = &(*tc)->snext);
	*tc = c->snext;

	if(c == sel) {
		for(t = stack; t && !isvisible(t); t = t->snext);
		sel = t;
	}
}

void
focus(struct Client *c) {
	DEBUG("focus\n");
	if(!c || !isvisible(c))
		for(c = stack; c && !isvisible(c); c = c->snext)
			if(sel && sel != c)
				unfocus(sel);
	if(c) {
		detachstack(c);
		attachstack(c);
		change_border_width(c->win, BORDER_WIDTH);
		change_border_color(c->win, FOCUS);
		xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
				c->win, XCB_CURRENT_TIME);
	}
	else {
		xcb_set_input_focus(conn, XCB_NONE,
				XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
	}
	sel = c;
}

void
getatoms(xcb_atom_t *atoms, char **names, int count) {
	xcb_intern_atom_cookie_t cookies[count];
	for (unsigned int i = 0; i < count; ++i)
		cookies[i] = xcb_intern_atom(conn, 0, strlen(names[i]), names[i]);
	for (unsigned int i = 0; i < count; ++i) {
		xcb_intern_atom_reply_t *reply =
			xcb_intern_atom_reply(conn, cookies[i], NULL);
		if (reply) {
			atoms[i] = reply->atom;
			free(reply);
		} else
			DEBUG("failed to register %s atom.\n", wmatomnames[i]);
	}
}

xcb_keycode_t *
getkeycodes(xcb_keysym_t keysym) {
	xcb_key_symbols_t *keysyms;
	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err(EXIT_FAILURE, "ERROR: couldn't get keycode.");
	xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
	xcb_key_symbols_free(keysyms);
	return keycode;
}

xcb_keysym_t
getkeysym(xcb_keycode_t keycode) {
	xcb_key_symbols_t *keysyms;
	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err(EXIT_FAILURE, "ERROR: couldn't get keysym.");
	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
	xcb_key_symbols_free(keysyms);
	return keysym;
}

bool
isvisible(Client *c) {
	if (!sel || !c) return false;
	return c->workspace == sel->workspace;
}

void
keypress(xcb_generic_event_t *ev) {
	xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
	xcb_keysym_t keysym = getkeysym(e->detail);
	for (int i=0; i<LENGTH(keys); i++) {
		if (keysym == keys[i].keysym &&
				CLEANMASK(keys[i].mod) == CLEANMASK(e->state) &&
				keys[i].function)
		{
			keys[i].function(&keys[i].arg);
			break;
		}
	}
}

void
killclient(const Arg *arg) {
	if(!sel)
		return;
	if(!sendevent(sel, wmatom[WMDeleteWindow])) {
		xcb_kill_client(conn, sel->win);
	}
}

void
manage(xcb_window_t w) {
	DEBUG("manage\n");
	Client *c = NULL;
	if (!(c = malloc(sizeof(Client))))
		err(EXIT_FAILURE, "ERROR: couldn't allocate memory");
	c->win = w;

	/* geometry */
	xcb_get_geometry_reply_t *geom;
	geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, w), NULL);
	if (!geom)
		err(EXIT_FAILURE, "ERROR: geometry reply failed");
	c->x = c->oldx = geom->x;
	c->y = c->oldy = geom->y;
	c->w = c->oldw = geom->width;
	c->h = c->oldh = geom->height;
	free(geom);
	c->workspace = 0;
	c->isfullscreen = false;
	attach(c);
	attachstack(c);
	sel = c;
	change_border_width(w, BORDER_WIDTH);
	xcb_map_window(conn, w);
	focus(NULL);
}

void
maprequest(xcb_generic_event_t *ev) {
	DEBUG("maprequest\n");
	xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
	if (!wintoclient(e->window))
		manage(e->window);
	focus(NULL);
}

void
move(const Arg *arg) {
	if (!sel)
		return;
	if (sel->win == screen->root)
		return;
	DEBUG("move\n");
	int step = steps[0];
	switch (arg->i) {
		case 0: sel->y += step; break;
		case 1: sel->x += step; break;
		case 2: sel->y -= step; break;
		case 3: sel->x -= step; break;
	}
	uint32_t values[2];
	values[0] = sel->x;
	values[1] = sel->y;
	xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_X|
			XCB_CONFIG_WINDOW_Y, values);
}

void
quit(const Arg *arg) {
	DEBUG("bye\n");
	running = false;
}

void
resize(const Arg *arg) {
	if (!sel)
		return;
	if (sel->win == screen->root)
		return;
	DEBUG("resize\n");
	int step = steps[1];
	switch (arg->i) {
		case 0: sel->h = sel->h + step; break;
		case 1: sel->w = sel->w + step; break;
		case 2: sel->h = sel->h - step > 0 ? sel->h - step : sel->h; break;
		case 3: sel->w = sel->w - step > 0 ? sel->w - step : sel->w; break;
	}
	uint32_t values[2];
	values[0] = sel->w;
	values[1] = sel->h;
	xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_WIDTH
			|XCB_CONFIG_WINDOW_HEIGHT, values);
}

void
run(void) {
	xcb_generic_event_t *ev;
	while (running) {
		xcb_flush(conn);
		ev = xcb_wait_for_event(conn);
		if (handler[CLEANMASK(ev->response_type)])
			handler[CLEANMASK(ev->response_type)](ev);
		free(ev);
		if (xcb_connection_has_error(conn))
			running = false;
	}
}

bool
sendevent(Client *c, xcb_atom_t proto) {
	xcb_client_message_event_t ev;
	xcb_get_property_cookie_t cookie;
	xcb_icccm_get_wm_protocols_reply_t reply;
	bool exists = false;
	cookie = xcb_icccm_get_wm_protocols(conn, c->win, wmatom[WMProtocols]);
	if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &reply, NULL) == 1) {
		for (int i = 0; i < reply.atoms_len && !exists; i++) {
			if (reply.atoms[i] == proto) {
				exists = true;
				xcb_icccm_get_wm_protocols_reply_wipe(&reply);
			}
		}
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
setcursor(int cursorid) {
	xcb_font_t font = xcb_generate_id(conn);
	xcb_void_cookie_t fontcookie =
		xcb_open_font_checked(conn, font, strlen ("cursor"), "cursor");
	testcookie(fontcookie, "can't open font");
	xcb_cursor_t cursor = xcb_generate_id(conn);
	xcb_create_glyph_cursor(conn, cursor, font, font,
			cursorid, cursorid + 1, 0, 0, 0, 65535, 65535, 65535);
	uint32_t mask = XCB_CW_CURSOR;
	uint32_t value_list = cursor;
	xcb_change_window_attributes(conn, screen->root, mask, &value_list);
	xcb_free_cursor(conn, cursor);
	fontcookie = xcb_close_font_checked(conn, font);
	testcookie(fontcookie, "can't close font");
}

void
setup() {
	/* clean up any zombies */
	sigchld();
	/* init screen */
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	if (!screen)
		err(EXIT_FAILURE, "ERROR: couldn't find screen.");
	sw = screen->width_in_pixels;
	sh = screen->height_in_pixels;
	/* subscribe to handler */
	unsigned int value[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
		XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT};
	xcb_void_cookie_t cookie;
	cookie = xcb_change_window_attributes_checked(conn, screen->root,
			XCB_CW_EVENT_MASK, value);
	testcookie(cookie, "another window manager is running.");
	/* init atoms */
	getatoms(wmatom, wmatomnames, WMLast);
	getatoms(netatom, netatomnames, NetLast);
	/* init keys */
	xcb_keycode_t *keycode;
	for (unsigned int i = 0; i < LENGTH(keys); ++i) {
		keycode = getkeycodes(keys[i].keysym);
		for (unsigned int k = 0; keycode[k] != XCB_NO_SYMBOL; k++)
			xcb_grab_key(conn, 1, screen->root, keys[i].mod, keycode[k],
				XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	}
	/* set handlers */
	for (unsigned int i = 0; i < XCB_NO_OPERATION; ++i)
		handler[i] = NULL;
	handler[XCB_CONFIGURE_REQUEST] = configurerequest;
	handler[XCB_DESTROY_NOTIFY] = destroynotify;
	handler[XCB_UNMAP_NOTIFY] = unmapnotify;
	handler[XCB_MAP_REQUEST] = maprequest;
	handler[XCB_CLIENT_MESSAGE] = clientmessage;
	handler[XCB_KEY_PRESS] = keypress;
	setcursor(68); /* left_ptr */
	focus(NULL);
}

void
sigchld() {
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		err(EXIT_FAILURE, "ERROR: can't install SIGCHLD handler");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
testcookie(xcb_void_cookie_t cookie, char *errormsg) {
	xcb_generic_error_t *error = xcb_request_check(conn, cookie);
	if (error) {
		fprintf(stderr, "ERROR: %s : %d\n", errormsg, error->error_code);
		xcb_disconnect(conn);
		exit(EXIT_FAILURE);
	}
}

void
unfocus(struct Client *c) {
	DEBUG("unfocus\n");
	if(!c)
		return;
	change_border_color(c->win, UNFOCUS);
	xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
			c->win, XCB_CURRENT_TIME);
}

void
unmanage(Client *c) {
	DEBUG("unmanage\n");
	xcb_kill_client(conn, c->win);
	detach(c);
	detachstack(c);
	free(c);
	focus(NULL);
}

void
unmapnotify(xcb_generic_event_t *ev) {
	xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
	DEBUG("unmapnotify\n");
	Client *c;
	if ((c = wintoclient(e->window))) {
		unmanage(c);
	}
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
	conn = xcb_connect(NULL, NULL);
	if (xcb_connection_has_error(conn))
		err(EXIT_FAILURE, "ERROR: xcb_connect error");
	setup();
	run();
	cleanup();
	return EXIT_SUCCESS;
}

/* vi: set noexpandtab noautoindent : */
