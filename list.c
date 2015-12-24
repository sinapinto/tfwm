/* See LICENSE file for copyright and license details. */
#include "tfwm.h"
#include "list.h"
#include "xcb.h"
#include "util.h"

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

	for (tc = &clients; *tc && *tc != c; tc = &(*tc)->next)
		continue;
	*tc = c->next;
}

void
detachstack(Client *c) {
	Client **tc, *t;

	for (tc = &stack; *tc && *tc != c; tc = &(*tc)->snext)
		continue;
	*tc = c->snext;

	if (c == sel) {
		for (t = stack; t && !ISVISIBLE(t); t = t->snext)
			continue;
		sel = t;
	}
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

