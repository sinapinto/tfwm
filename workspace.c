#include "tfwm.h"
#include "list.h"
#include "client.h"

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

