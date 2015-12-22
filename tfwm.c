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
#include "xcb.h"
#ifdef SHAPE
#include "shape.h"
#endif

static void cleanup(void);
static void remanage_windows(void);
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
bool pixmap_border      = false;
int border_width       = 4;
int move_step          = 30;
int resize_step        = 30;
bool sloppy_focus      = false;
bool java_workaround   = true;
int cursor_position    = 0;
char *focus_color;
char *unfocus_color;

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
cleanup(void) {
	PRINTF("========= shutting down ==========\n");

	xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT,
			    XCB_CURRENT_TIME);
	while (stack) {
		/* xcb_reparent_window(conn, stack->win, screen->root, */
		/* 		    stack->geom.x, stack->geom.y); */
		/* xcb_destroy_window(conn, stack->frame); */
		PRINTF("unmap and free win %#x\n", stack->win);
		xcb_unmap_window(conn, stack->win);

		detach(stack);
		detachstack(stack);
		free(stack);
	}

	ewmh_teardown();
	free_cursors();
	free(focus_color);
	free(unfocus_color);
	xcb_flush(conn);
	xcb_disconnect(conn);
	PRINTF("bye\n\n");
}

void
remanage_windows(void) {
	xcb_query_tree_cookie_t             qtc;
	xcb_query_tree_reply_t             *qtr;
	xcb_get_window_attributes_cookie_t  gac;
	xcb_get_window_attributes_reply_t  *gar;

	qtc = xcb_query_tree(conn, screen->root);
	if ((qtr = xcb_query_tree_reply(conn, qtc, NULL))) {
		PRINTF("parent = 0x%08x\n", qtr->parent);
		PRINTF("root   = 0x%08x\n", qtr->root);
		xcb_window_t *children = xcb_query_tree_children(qtr);

		for (int i = 0; i < xcb_query_tree_children_length(qtr); i++) {
			warn("restore window %#x\n", children[i]);
			manage(children[i]);

			gac = xcb_get_window_attributes(conn, children[i]);
			gar = xcb_get_window_attributes_reply(conn, gac, NULL);
			if (!gar)
				continue;
			if (gar->override_redirect) {
				PRINTF("remanage_windows: skip %#x: "
				       "override_redirect set\n", children[i]);
				free(gar);
				continue;
			}
		}
		free(qtr);
	}
}

void
setup(void) {
	xcb_generic_error_t *e;
	xcb_void_cookie_t    wac;
	uint32_t             vals[1];

	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	if (!screen)
		err("can't find screen.");

	/* subscribe to handler */
	vals[0] = ROOT_EVENT_MASK;
	wac = xcb_change_window_attributes_checked(conn, screen->root,
						   XCB_CW_EVENT_MASK, vals);
	e = xcb_request_check(conn, wac);
	if (e) {
		xcb_disconnect(conn);
		err("another window manager is running.");
	}

	/* restore windows from prior session
	 * must come before ewmh_setup to avoid managing the wm_check window
	 */
	remanage_windows();

	getatom(&WM_DELETE_WINDOW, "WM_DELETE_WINDOW");
	getatom(&WM_TAKE_FOCUS, "WM_TAKE_FOCUS");
	getatom(&WM_PROTOCOLS, "WM_PROTOCOLS");

	ewmh_setup();

	updatenumlockmask();
	grabkeys();

	focuscol = getcolor(focus_color);
	unfocuscol = getcolor(unfocus_color);

	load_cursors();
	vals[0] = cursors[XC_LEFT_PTR].cid;
	xcb_change_window_attributes_checked(conn, screen->root,
					     XCB_CW_CURSOR, vals);
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

	setup();
	run();
	cleanup();

	if (dorestart)
		execvp(argv[0], argv);

	return EXIT_SUCCESS;
}

