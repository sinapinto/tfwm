/* See LICENSE file for copyright and license details. */
#include "tfwm.h"
#include "list.h"

Client *clients;
static void grabbuttons(Client *c);

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
focus(Client *c) {
	if (!c || !ISVISIBLE(c))
		for (c = stack; c && !ISVISIBLE(c); c = c->snext)
			if (sel && sel != c)
				setborder(sel, false);
	if (c) {
		detachstack(c);
		attachstack(c);
		grabbuttons(c);
		if (sel && sel != c)
			setborder(sel, false);
		setborder(c, true);
		PRINTF("focus win %#x\n", c->win);
		xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
				c->win, XCB_CURRENT_TIME);
		xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
				ewmh->_NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 32, 1,&c->win);
	}
	else {
		xcb_delete_property(conn, screen->root, ewmh->_NET_ACTIVE_WINDOW);
		xcb_set_input_focus(conn, XCB_NONE, XCB_INPUT_FOCUS_POINTER_ROOT, XCB_CURRENT_TIME);
	}
	sel = c;
}

void
focusstack(bool next) {
	Client *c = NULL, *i;

	if (!sel)
		return;
	if (next) {
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
		raisewindow(sel->frame);
		raisewindow(sel->win);
	}
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

