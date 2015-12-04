/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "workspace.h"
#include "events.h"
#include "keys.h"

static void cleanup(void);
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
xcb_atom_t WM_DELETE_WINDOW;

void
cleanup(void) {
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
		ev = xcb_wait_for_event(conn);
		handleevent(ev);
		free(ev);
		if (xcb_connection_has_error(conn)) {
			cleanup();
			exit(1);
		}
	}
}

void
setup(void) {
	/* init screen */
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	if (!screen)
		err("can't find screen.");
	/* subscribe to handler */
	unsigned int values[] = {
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
			XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
			XCB_EVENT_MASK_BUTTON_PRESS
	};
	xcb_void_cookie_t cookie;
	cookie = xcb_change_window_attributes_checked(conn, screen->root,
			XCB_CW_EVENT_MASK, values);
	testcookie(cookie, "another window manager is running.");
	xcb_flush(conn);
	/* init atoms */
	getatom(&WM_DELETE_WINDOW, "WM_DELETE_WINDOW");
	/* init ewmh */
	ewmh = malloc(sizeof(xcb_ewmh_connection_t));
	if (xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh), NULL) == 0)
		err("can't initialize ewmh.");
	xcb_drawable_t root = screen->root;
	xcb_window_t recorder = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, recorder, root, 0, 0,
			screen->width_in_pixels, screen->height_in_pixels, 0,
			XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_NONE, NULL);
	xcb_ewmh_set_wm_pid(ewmh, recorder, getpid());
	xcb_ewmh_set_wm_name(ewmh, screen->root, 4, "tfwm");
	xcb_ewmh_set_supporting_wm_check(ewmh, recorder, recorder);
	xcb_ewmh_set_supporting_wm_check(ewmh, screen->root, recorder);
	xcb_atom_t net_atoms[] = {
		ewmh->_NET_WM_NAME,
		ewmh->_NET_WM_PID,
		ewmh->_NET_SUPPORTED,
		ewmh->_NET_NUMBER_OF_DESKTOPS,
		ewmh->_NET_CURRENT_DESKTOP,
		ewmh->_NET_WM_DESKTOP,
		ewmh->_NET_ACTIVE_WINDOW,
		ewmh->_NET_CLOSE_WINDOW,
		ewmh->_NET_WM_STATE,
		ewmh->_NET_WM_STATE_FULLSCREEN,
		/* ewmh->_NET_CLIENT_LIST, */
		/* ewmh->_NET_CLIENT_LIST_STACKING, */
		/* ewmh->_NET_WM_STATE_MAXIMIZED_VERT, */
		/* ewmh->_NET_WM_STATE_MAXIMIZED_HORZ, */
		/* ewmh->_NET_WM_STATE_BELOW, */
		/* ewmh->_NET_WM_STATE_ABOVE, */
		/* ewmh->_NET_WM_STATE_STICKY, */
		/* ewmh->_NET_WM_STATE_DEMANDS_ATTENTION, */
		/* ewmh->_NET_WM_WINDOW_TYPE, */
		/* ewmh->_NET_WM_WINDOW_TYPE_DOCK, */
		/* ewmh->_NET_WM_WINDOW_TYPE_DESKTOP, */
		/* ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION, */
		/* ewmh->_NET_WM_WINDOW_TYPE_DIALOG, */
		/* ewmh->_NET_WM_WINDOW_TYPE_UTILITY, */
		/* ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR, */
		/* ewmh->_NET_SUPPORTING_WM_CHECK, */
	};
	xcb_ewmh_set_supported(ewmh, scrno, LENGTH(net_atoms), net_atoms);
	static const uint8_t numdesktops = 10;
	xcb_ewmh_set_number_of_desktops(ewmh, scrno, numdesktops);
	xcb_ewmh_set_current_desktop(ewmh, scrno, 0);
	/* init keys */
	updatenumlockmask();
	grabkeys();
	/* init colors */
	focuscol = getcolor(FOCUS_COLOR);
	unfocuscol = getcolor(UNFOCUS_COLOR);
	outercol = getcolor(OUTER_COLOR);
	focus(NULL);
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

int
main(int argc, char **argv) {
	if ((argc == 2) && !strcmp("-v", argv[1]))
		err("tfwm-%s\n", VERSION);
	else if (argc != 1)
		err("usage: tfwm [-v]\n");

	sigchld();
	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);

	conn = xcb_connect(NULL, &scrno);
	if (xcb_connection_has_error(conn))
		err("xcb_connect error");

	setup();
	run();
	cleanup();

	if (dorestart)
		execvp(argv[0], argv);

	return EXIT_SUCCESS;
}

