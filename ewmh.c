/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <unistd.h>
#include <xcb/xcb_ewmh.h>
#include "tfwm.h"
#include "ewmh.h"

void
ewmh_setup() {
	ewmh = malloc(sizeof(xcb_ewmh_connection_t));
	if (xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh), NULL) == 0)
		err("can't initialize ewmh.");

	xcb_atom_t net_atoms[] = {
		ewmh->_NET_WM_NAME,
		ewmh->_NET_WM_PID,
		ewmh->_NET_SUPPORTED,
		ewmh->_NET_SUPPORTING_WM_CHECK,
		ewmh->_NET_NUMBER_OF_DESKTOPS,
		ewmh->_NET_CURRENT_DESKTOP,
		ewmh->_NET_WM_DESKTOP,
		ewmh->_NET_ACTIVE_WINDOW,
		ewmh->_NET_CLOSE_WINDOW,
		ewmh->_NET_CLIENT_LIST,
		ewmh->_NET_CLIENT_LIST_STACKING,
		ewmh->_NET_WM_STATE,
		ewmh->_NET_WM_STATE_FULLSCREEN,
		ewmh->_NET_WM_STATE_MAXIMIZED_VERT,
		ewmh->_NET_WM_STATE_MAXIMIZED_HORZ,
		ewmh->_NET_WM_STATE_BELOW,
		ewmh->_NET_WM_STATE_ABOVE,
		ewmh->_NET_WM_WINDOW_TYPE,
		ewmh->_NET_WM_WINDOW_TYPE_DOCK,
		ewmh->_NET_WM_WINDOW_TYPE_DESKTOP,
		ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION,
		ewmh->_NET_WM_WINDOW_TYPE_DIALOG,
		ewmh->_NET_WM_WINDOW_TYPE_UTILITY,
		ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR
	};
	xcb_ewmh_set_supported(ewmh, scrno, LENGTH(net_atoms), net_atoms);

	xcb_window_t recorder = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, recorder, screen->root, 0, 0,
			screen->width_in_pixels, screen->height_in_pixels, 0,
			XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_NONE, NULL);

	/* set up _NET_WM_PID */
	xcb_ewmh_set_wm_pid(ewmh, recorder, getpid());

	/* set up _NET_WM_NAME */
#if JAVA_WORKAROUND
	xcb_ewmh_set_wm_name(ewmh, screen->root, strlen("LG3D"), "LG3D");
#else
	xcb_ewmh_set_wm_name(ewmh, screen->root, strlen("tfwm"), "tfwm");
#endif

	/* set up _NET_SUPPORTING_WM_CHECK */
	xcb_ewmh_set_supporting_wm_check(ewmh, recorder, recorder);
	xcb_ewmh_set_supporting_wm_check(ewmh, screen->root, recorder);

	/* set up _NET_NUMBER_OF_DESKTOPS */
	static const uint8_t numdesktops = 10;
	xcb_ewmh_set_number_of_desktops(ewmh, scrno, numdesktops);
	xcb_ewmh_set_current_desktop(ewmh, scrno, 0);
}

void
ewmh_teardown() {
	xcb_window_t id;
	xcb_get_property_cookie_t pc;
	xcb_get_property_reply_t *pr;

	/* delete supporting wm check window */
	pc = xcb_get_property(conn, 0, screen->root, ewmh->_NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 0, 1);
	pr = xcb_get_property_reply(conn, pc, NULL);
	if (pr) {
		if (pr->format == ewmh->_NET_SUPPORTING_WM_CHECK) {
			id = *((xcb_window_t *)xcb_get_property_value(pr));

			xcb_destroy_window(conn, id);
			xcb_delete_property(conn, screen->root, ewmh->_NET_SUPPORTING_WM_CHECK);
			xcb_delete_property(conn, screen->root, ewmh->_NET_SUPPORTED);
		}
		free(pr);
	}

	xcb_ewmh_connection_wipe(ewmh);
	free(ewmh);
}

bool
ewmh_add_wm_state(Client *c, xcb_atom_t state) {
	(void)c;
	(void)state;
	return false;
}

bool
ewmh_remove_wm_state(Client *c, xcb_atom_t state) {
	(void)c;
	(void)state;
	return false;
}

void
ewmh_update_client_list(Client *list) {
	Client *t;
	int count = 0;

	for (t = list; t; t = t->next, count++);

	if (count == 0)
		return;

	PRINTF("ewmh_update_client_list: %d windows\n", count);
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root, ewmh->_NET_CLIENT_LIST, XCB_ATOM_WINDOW, 32, count, list);
}

