/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <X11/Xlib-xcb.h>
#include <X11/cursorfont.h>
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "workspace.h"
#include "events.h"
#include "keys.h"
#include "pointer.h"
#include "ewmh.h"
#include "config.h"
#ifdef SHAPE
#include "shape.h"
#endif

static void cleanup(void);
static bool connection_has_error(void);
static void getatom(xcb_atom_t *atom, char *name);
static void run(void);
static void setup(void);
static void sigchld();
static void sigcatch(int sig);

xcb_connection_t *conn;
xcb_screen_t *screen;
unsigned int numlockmask;
int scrno;
Client *stack;
int sigcode;
xcb_ewmh_connection_t *ewmh;
uint32_t focuscol, unfocuscol, outercol;
bool dorestart;
bool shape_ext;
xcb_atom_t WM_DELETE_WINDOW;
xcb_atom_t WM_TAKE_FOCUS;
xcb_atom_t WM_PROTOCOLS;
Display *display;
cursor_t cursors[XC_MAX] = {
	{"left_ptr",            XC_left_ptr,            XCB_CURSOR_NONE},
	{"fleur",               XC_fleur,               XCB_CURSOR_NONE},
	{"bottom_right_corner", XC_bottom_right_corner, XCB_CURSOR_NONE}
};
unsigned int selws = 0;
unsigned int prevws = 0;
Client *sel;
Client *clients;

/* config defaults */
bool double_border     = false;
int border_width       = 4;
int outer_border_width = 4;
int move_step          = 30;
int resize_step        = 30;
bool sloppy_focus      = false;
bool java_workaround   = true;
int cursor_position    = 0;
char *focus_color;
char *outer_color;
char *unfocus_color;

#ifdef DEBUG
char *
get_atom_name(xcb_atom_t atom) {
	char *name = NULL;
	xcb_get_atom_name_reply_t *reply;
	int len;

	reply = xcb_get_atom_name_reply(conn, xcb_get_atom_name(conn, atom), NULL);
	if (!reply)
		return NULL;

	len = xcb_get_atom_name_name_length(reply);
	if (len) {
		if (!(name = malloc(len + 1)))
			err("can't allocate memory.");
		memcpy(name, xcb_get_atom_name_name(reply), len);
		name[len] = '\0';
	}

	free(reply);

	return name;
}
#endif

void
cleanup(void) {
	PRINTF("shutting down..\n");

	while (stack) {
		PRINTF("freeing win %#x...\n", stack->win);
		if (stack->can_delete)
			send_client_message(stack, WM_DELETE_WINDOW);
		else
			xcb_kill_client(conn, stack->win);
		unmanage(stack);
	}

	ewmh_teardown();

	free_cursors();

	free(focus_color);
	free(outer_color);
	free(unfocus_color);

	xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
	xcb_flush(conn);
	xcb_disconnect(conn);
	warn("bye\n");
}

bool
connection_has_error(void) {
	int err = 0;

	if ((err = xcb_connection_has_error(conn)) > 0) {
		warn("X connection error: ");
		switch (err) {
			case XCB_CONN_ERROR:
				warn("socket, pipe, or other stream error.\n");
				break;
			case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
				warn("extension not supported.\n");
				break;
			case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
				warn("memory not available.\n");
				break;
			case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
				warn("exceeding request length.\n");
				break;
			case XCB_CONN_CLOSED_PARSE_ERR:
				warn("error parsing display string.\n");
				break;
			case XCB_CONN_CLOSED_INVALID_SCREEN:
				warn("no screen matching the display.\n");
				break;
			default:
				warn("unknown error.\n");
				break;
		}
		return true;
	}

	return false;
}

void
getatom(xcb_atom_t *atom, char *name) {
	xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0, strlen(name), name), NULL);

	if (reply) {
		*atom = reply->atom;
		free(reply);
	} else
		*atom = XCB_NONE;
}

uint32_t
getcolor(char *color) {
	uint32_t pixel;
	xcb_colormap_t map = screen->default_colormap;

	if (color[0] == '#') {
		unsigned int r, g, b;
		if (sscanf(color + 1, "%02x%02x%02x", &r, &g, &b) != 3)
			err("bad color: %s.", color);
		/* convert from 8-bit to 16-bit */
		r = r << 8 | r;
		g = g << 8 | g;
		b = b << 8 | b;
		xcb_alloc_color_cookie_t cookie = xcb_alloc_color(conn, map, r, g, b);
		xcb_alloc_color_reply_t *reply = xcb_alloc_color_reply(conn, cookie, NULL);
		if (!reply)
			err("can't alloc color.");
		pixel = reply->pixel;
		free(reply);
		return pixel;
	}
	xcb_alloc_named_color_cookie_t cookie = xcb_alloc_named_color(conn, map, strlen(color), color);
	xcb_alloc_named_color_reply_t *reply = xcb_alloc_named_color_reply(conn, cookie, NULL);
	if (!reply)
		err("can't alloc named color.");
	pixel = reply->pixel;
	free(reply);
	return pixel;
}

void
quit(const Arg *arg) {
	(void)arg;
	cleanup();
	exit(sigcode);
}

void
restart(const Arg *arg) {
	(void) arg;
	dorestart = true;
	sigcode = 1;
}

void
run(void) {
	xcb_generic_event_t *ev;
	while (sigcode == 0) {
		xcb_flush(conn);
		if ((ev = xcb_wait_for_event(conn)) != NULL) {
			handleevent(ev);
			free(ev);
		}
		if (connection_has_error()) {
			cleanup();
			exit(1);
		}
	}
}


void
sigcatch(int sig) {
	sigcode = sig;
}

void
sigchld() {
	if (signal(SIGCHLD, sigchld) == SIG_ERR)
		err("can't install SIGCHLD handler.");
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
setup(void) {
	/* init screen */
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	if (!screen)
		err("can't find screen.");

	/* subscribe to handler */
	uint32_t values[] = { ROOT_EVENT_MASK };
	xcb_generic_error_t *e = xcb_request_check(conn, xcb_change_window_attributes_checked(conn, screen->root, XCB_CW_EVENT_MASK, values));
	if (e) {
		xcb_disconnect(conn);
		err("another window manager is running.");
	}

	/* init atoms */
	getatom(&WM_DELETE_WINDOW, "WM_DELETE_WINDOW");
	getatom(&WM_TAKE_FOCUS, "WM_TAKE_FOCUS");
	getatom(&WM_PROTOCOLS, "WM_PROTOCOLS");

	/* init ewmh */
	ewmh_setup();

	/* init keys */
	updatenumlockmask();
	grabkeys();

	/* init colors */
	focuscol = getcolor(focus_color);
	unfocuscol = getcolor(unfocus_color);
	outercol = getcolor(outer_color);

	/* init cursors */
	load_cursors();
	xcb_change_window_attributes_checked(conn, screen->root, XCB_CW_CURSOR, (uint32_t[]){cursors[XC_LEFT_PTR].cid});

#ifdef SHAPE
	shape_ext = check_shape_extension();
#endif
	focus(NULL);
}

int
main(int argc, char **argv) {
	(void)argc;
	char *rc_path = NULL;

	warn("welcome to tfwm %s\n", VERSION);

	/* set up shared XLib/XCB connection */
	if ((display = XOpenDisplay(NULL)) == NULL)
		err("can't open display.");

	conn = XGetXCBConnection(display);
	if (connection_has_error())
		return EXIT_FAILURE;

	XSetEventQueueOwner(display, XCBOwnsEventQueue);

	/* reap children */
	sigchld();

	/* set up signal handlers */
	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);

	scrno = 0;

	/* load config */
	if ((rc_path = find_config("tfwmrc"))) {
		parse_config(rc_path);
		free(rc_path);
	} else {
		warn("no config file found. using default settings\n");
	}

	/* set up and run */
	setup();
	run();
	cleanup();

	if (dorestart)
		execvp(argv[0], argv);

	return EXIT_SUCCESS;
}

