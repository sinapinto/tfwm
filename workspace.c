/* See LICENSE file for copyright and license details. */
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "workspace.h"
#include "util.h"

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
	xcb_ewmh_set_current_desktop(ewmh, scrno, i);
	prevws = selws;
	selws = i;
	focus(NULL);
	showhide(stack);
}

void
sendtows(const Arg *arg) {
	if (arg->i == selws)
		return;
	sel->ws = arg->i;
	showhide(stack);
	focus(NULL);
}

