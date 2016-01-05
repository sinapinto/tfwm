/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <string.h>
#include "tfwm.h"
#include "xcb.h"
#include "util.h"

#ifdef DEBUG
char *
get_atom_name(xcb_atom_t atom) {
	char *name = NULL;
	xcb_get_atom_name_reply_t *r;
	xcb_get_atom_name_cookie_t c;
	int len;

	c = xcb_get_atom_name(conn, atom);
	r = xcb_get_atom_name_reply(conn, c, NULL);
	if (!r)
		return NULL;

	len = xcb_get_atom_name_name_length(r);
	if (len) {
		if (!(name = malloc(len + 1)))
			err("can't allocate memory.");
		memcpy(name, xcb_get_atom_name_name(r), len);
		name[len] = '\0';
	}

	FREE(r);
	return name;
}
#endif

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
getatom(xcb_atom_t *atom, const char *name) {
	xcb_intern_atom_reply_t  *r;
	xcb_intern_atom_cookie_t  c;

	c = xcb_intern_atom(conn, 0, strlen(name), name);
	r = xcb_intern_atom_reply(conn, c, NULL);

	if (r) {
		*atom = r->atom;
		FREE(r);
	} else {
		*atom = XCB_NONE;
	}
}

uint32_t
getcolor(const char *color) {
	uint32_t                        pixel;
	unsigned int                    r, g, b;
	xcb_colormap_t                  map;
	xcb_alloc_color_cookie_t        cc;
	xcb_alloc_color_reply_t        *cr;
	xcb_alloc_named_color_cookie_t  ncc;
	xcb_alloc_named_color_reply_t  *ncr;

	map = screen->default_colormap;

	if (color[0] == '#') {
		if (sscanf(color + 1, "%02x%02x%02x", &r, &g, &b) != 3)
			err("bad color: %s.", color);
		/* convert from 8-bit to 16-bit */
		r = r << 8 | r;
		g = g << 8 | g;
		b = b << 8 | b;
		cc = xcb_alloc_color(conn, map, r, g, b);
		cr = xcb_alloc_color_reply(conn, cc, NULL);
		if (!cr)
			err("can't alloc color.");
		pixel = cr->pixel;
		FREE(cr);
	} else {
		ncc = xcb_alloc_named_color(conn, map, strlen(color), color);
		ncr = xcb_alloc_named_color_reply(conn, ncc, NULL);
		if (!ncr)
			err("can't alloc named color.");
		pixel = ncr->pixel;
		FREE(ncr);
	}
	return pixel;
}

void
grabbuttons(Client *c) {
	unsigned int i, j;
	unsigned int modifiers[] = { 0, XCB_MOD_MASK_LOCK, numlockmask,
		numlockmask | XCB_MOD_MASK_LOCK };

	for (i = 0; i < LENGTH(buttons); i++) {
		for (j = 0; j < LENGTH(modifiers); j++) {
			xcb_grab_button(conn, 1, c->win,
					XCB_EVENT_MASK_BUTTON_PRESS,
					XCB_GRAB_MODE_ASYNC,
					XCB_GRAB_MODE_ASYNC, screen->root,
					XCB_NONE, buttons[i].button,
					buttons[i].mask|modifiers[j]);
		}
	}
}

