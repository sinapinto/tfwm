/* See LICENSE file for copyright and license details. */
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "workspace.h"

static void gotows(unsigned int i);

void
selectrws(const Arg* arg) {
	unsigned int i;
	if (arg->i == LastWorkspace)
		i = prevws;
	else if (arg->i == PrevWorkspace)
		i = selws == 0 ? 9 : selws - 1;
	else if (arg->i == NextWorkspace)
		i = selws == 9 ? 0 : selws + 1;
	else
		return;
	gotows(i);
}

void
selectws(const Arg* arg) {
	gotows(arg->i);
}

void
gotows(unsigned int i) {
	if (selws == i)
		return;
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
			ewmh->_NET_CURRENT_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &i);
	xcb_ewmh_set_current_desktop(ewmh, scrno, i);
	prevws = selws;
	selws = i;
	focus(NULL);
	showhide(stack);
}

