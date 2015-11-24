/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_ewmh.h>
#include "types.h"
#include "list.h"
#include "events.h"
#include "window.h"
#include "shape.h"
#include "tfwm.h"
#include "config.h"

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

xcb_keycode_t *
getkeycodes(xcb_keysym_t keysym) {
	xcb_key_symbols_t *keysyms;
	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err("can't get keycode.");
	xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
	xcb_key_symbols_free(keysyms);
	return keycode;
}

xcb_keysym_t
getkeysym(xcb_keycode_t keycode) {
	xcb_key_symbols_t *keysyms;
	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err("can't get keysym.");
	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
	xcb_key_symbols_free(keysyms);
	return keysym;
}

void
grabbuttons(Client *c) {
	unsigned int i, j;
	unsigned int modifiers[] = { 0, XCB_MOD_MASK_LOCK, numlockmask,
		numlockmask | XCB_MOD_MASK_LOCK };

	for (i = 0; i < LENGTH(buttons); i++)
		for (j = 0; j < LENGTH(modifiers); j++)
			xcb_grab_button(conn, 1, c->win, XCB_EVENT_MASK_BUTTON_PRESS,
					XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, screen->root,
					XCB_NONE, buttons[i].button, buttons[i].mask|modifiers[j]);
}

void
grabkeys() {
	unsigned int i, j, k;
	unsigned int modifiers[] = { 0, XCB_MOD_MASK_LOCK, numlockmask,
		numlockmask | XCB_MOD_MASK_LOCK };
	xcb_keycode_t *keycode;

	xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
	for (i = 0; i < LENGTH(keys); ++i) {
		keycode = getkeycodes(keys[i].keysym);
		for (j = 0; keycode[j] != XCB_NO_SYMBOL; j++)
			for (k = 0; k < LENGTH(modifiers); k++)
				xcb_grab_key(conn, 1, screen->root, keys[i].mod | modifiers[k],
						keycode[j], XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
		free(keycode);
	}
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
selectrws(const Arg* arg) {
	int i;
	if (arg->i == LastWorkspace)
		i = prevws;
	else if (arg->i == PrevWorkspace)
		i = selws == 0 ? 9 : selws - 1;
	else if (arg->i == NextWorkspace)
		i = selws == 9 ? 0 : selws + 1;
	else
		return;
	const Arg a = { .i = i };
	selectws(&a);
}

void
selectws(const Arg* arg) {
	if (selws == arg->i)
		return;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
			ewmh->_NET_CURRENT_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &arg->i);
	xcb_ewmh_set_current_desktop(ewmh, scrno, arg->i);
	prevws = selws;
	selws = arg->i;
	focus(NULL);
	showhide(stack);
}

void
setup() {
	sigchld();
	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);
	/* init screen */
	screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	if (!screen)
		err("can't find screen.");
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
	getatom(&WM_DELETE_WINDOW, "WM_DELETE_WINDOW");
	/* init ewmh */
	ewmh = malloc(sizeof(xcb_ewmh_connection_t));
	if (xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh), NULL) == 0)
		err("can't initialize ewmh.");
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
	/* set handlers */
	handler[0]                     = requesterror;
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

void
spawn(const Arg *arg) {
	(void)arg;
	if (fork() == 0) {
		if (conn)
			close(screen->root);
		setsid();
		execvp((char*)arg->com[0], (char**)arg->com);
		err("execvp %s", ((char **)arg->com)[0]);
	}
}

void
testcookie(xcb_void_cookie_t cookie, char *errormsg) {
	xcb_generic_error_t *error = xcb_request_check(conn, cookie);
	if (error) {
		warn("%s : %d\n", errormsg, error->error_code);
		free(error);
		xcb_disconnect(conn);
		exit(EXIT_FAILURE);
	}
}

void
updatenumlockmask() {
	unsigned int i, j, n;
	numlockmask = 0;
	xcb_keycode_t *modmap ;
	xcb_get_modifier_mapping_reply_t *reply;

	reply = xcb_get_modifier_mapping_reply(conn, xcb_get_modifier_mapping_unchecked(conn), NULL);
	if (!reply)
		err("mod map reply");
	modmap = xcb_get_modifier_mapping_keycodes(reply);
	if (!modmap)
		err("mod map keycodes");

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


int
main(int argc, char **argv) {
	if ((argc == 2) && !strcmp("-v", argv[1]))
		err("tfwm-%s\n", VERSION);
	else if (argc != 1)
		err("usage: tfwm [-v]\n");

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

