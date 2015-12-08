/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <xcb/xcb_icccm.h>
#ifdef SHAPE
# include <xcb/shape.h>
# include <xcb/xcb_image.h>
# include "corner.xbm"
#endif
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "keys.h"

unsigned int selws = 0;
unsigned int prevws = 0;
Client *sel;

void
applyrules(Client *c) {
	unsigned int i;
	xcb_atom_t a = XCB_NONE;

	// WM_WINDOW_TYPE_*
	xcb_ewmh_get_atoms_reply_t win_type;
	if (xcb_ewmh_get_wm_window_type_reply(ewmh, xcb_ewmh_get_wm_window_type(ewmh, c->win), &win_type, NULL) == 1) {
		for (i = 0; i < win_type.atoms_len; i++) {
			a = win_type.atoms[i];
#ifdef DEBUG
			char *name;
			name = get_atom_name(a);
			PRINTF("applyrules: win %#x, atom %s\n", c->win, name);
			free(name);
#endif
		}
		xcb_ewmh_get_atoms_reply_wipe(&win_type);
	}

	// WM_STATE_*
	xcb_ewmh_get_atoms_reply_t win_state;
	if (xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, c->win), &win_state, NULL) == 1) {
		for (i = 0; i < win_state.atoms_len; i++) {
			a = win_state.atoms[i];
#ifdef DEBUG
			char *name;
			name = get_atom_name(a);
			PRINTF("applyrules: win %#x, atom %s\n", c->win, name);
			free(name);
#endif
			if (a == ewmh->_NET_WM_STATE_FULLSCREEN) {
				maximizeclient(c, true);
			}
		}
		xcb_ewmh_get_atoms_reply_wipe(&win_state);
	}

	xcb_size_hints_t size_hints;
	if (xcb_icccm_get_wm_normal_hints_reply(conn, xcb_icccm_get_wm_normal_hints(conn, c->win), &size_hints, NULL) == 1) {
		c->minw = size_hints.min_width;
		c->minh = size_hints.min_height;
	}

	/* custom rules */
	const Rule *r;
	xcb_icccm_get_wm_class_reply_t class_reply;

	if (!xcb_icccm_get_wm_class_reply(conn, xcb_icccm_get_wm_class(conn, c->win), &class_reply, NULL))
		return;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((r->class && strstr(class_reply.class_name, r->class))) {
			if (!r->border)
				c->noborder = true;
		}
	}
	xcb_icccm_get_wm_class_reply_wipe(&class_reply);
}

void
cycleclients(const Arg *arg) {
	if (arg->i == NextWindow)
		focusstack(true);
	else
		focusstack(false);
}

void
fitclient(Client *c) {
	bool update = false;

	if (c->noborder)
		return;
	if (c->geom.width >= screen->width_in_pixels-2*BORDER_WIDTH) {
		c->geom.width = screen->width_in_pixels - BORDER_WIDTH * 2;
		update = true;
	}
	if (c->geom.height >= screen->height_in_pixels-2*BORDER_WIDTH) {
		c->geom.height = screen->height_in_pixels - BORDER_WIDTH * 2;
		update = true;
	}
	if (update) {
		c->geom.x = c->geom.y = 0;
		moveresize(c, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
	}
}

void
killselected(const Arg *arg) {
	(void)arg;
	if (!sel)
		return;
	if (!sendevent(sel, WM_DELETE_WINDOW))
		xcb_kill_client(conn, sel->win);
}

void
manage(xcb_window_t w) {
	Client *c = NULL;
	if (!(c = malloc(sizeof(Client))))
		err("can't allocate memory.");
	c->win = w;
	/* geometry */
	xcb_get_geometry_reply_t *geom;
	geom = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, w), NULL);
	if (!geom)
		err("geometry reply failed.");
	c->geom.x = c->oldgeom.x = geom->x;
	c->geom.y = c->oldgeom.y = geom->y;
	c->geom.width = c->oldgeom.width = geom->width;
	c->geom.height = c->oldgeom.height = geom->height;
	free(geom);
	c->ws = selws;
	c->minw = c->minh = c->basew = c->baseh = c->incw = c->inch = 0;
	c->ismax = c->isvertmax = c->ishormax = c->isfixed = c->isurgent = c->noborder = false;
	applyrules(c);
	attach(c);
	attachstack(c);
	sel = c;
	fitclient(c);

#if SLOPPY_FOCUS
	uint32_t values[] = {CLIENT_EVENT_MASK};
	xcb_change_window_attributes(conn, w, XCB_CW_EVENT_MASK, values);
#endif

	if (c->ws == selws)
		xcb_map_window(conn, w);

	focus(NULL);
}

void
maximize(const Arg *arg) {
	(void)arg;
	if (!sel)
		return;
	maximizeclient(sel, !sel->ismax);
}

void
maximizeaxis(const Arg *arg) {
	if (!sel || sel->ismax)
		return;
	if (sel->isvertmax || sel->ishormax) {
		maximizeclient(sel, false);
		return;
	}
	savegeometry(sel);
	uint32_t values[3];
	uint16_t tw = screen->width_in_pixels;
	uint16_t th = screen->height_in_pixels;
	if (!sel->noborder) {
		tw -= BORDER_WIDTH * 2;
		th -= BORDER_WIDTH * 2;
	}
	if (arg->i == MaxVertical) {
		sel->geom.y = 0;
		sel->geom.height = th;
		values[0] = sel->geom.y;
		values[1] = sel->geom.height;
		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT, values);
		sel->isvertmax = true;
	}
	else if (arg->i == MaxHorizontal) {
		sel->geom.x = 0;
		sel->geom.width = tw;
		values[0] = sel->geom.x;
		values[1] = sel->geom.width;
		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH, values);
		sel->ishormax = true;
	}
	setborder(sel, true);
}

void
maximizeclient(Client *c, bool doit) {
	if (!c)
		return;
	if (c->ismax && doit)
		return;

	PRINTF("maximizeclient: %s, ", doit ? "maximizing" : "unmaximizing");
	if (doit) {
		savegeometry(c);
		c->ismax = true;
		c->isvertmax = c->ishormax = false;
		c->geom.x = 0;
		c->geom.y = 0;
		c->geom.width = screen->width_in_pixels;
		c->geom.height = screen->height_in_pixels;
		PRINTF("x %d y %d w %d h %d\n", c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		moveresize(c, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		focus(NULL);
		setborderwidth(c, 0);
	}
	else {
		c->geom.x = c->isvertmax ? c->geom.x : MAX(0, c->oldgeom.x);
		c->geom.y = c->ishormax ? c->geom.y : MAX(0, c->oldgeom.y);
		c->geom.width = c->oldgeom.width;
		c->geom.height = c->oldgeom.height;
		c->ismax = c->ishormax = c->isvertmax = false;
		PRINTF("x %d y %d w %d h %d\n", c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		moveresize(c, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		setborderwidth(sel, BORDER_WIDTH);
		setborder(sel, true);
	}
}

void
move(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	switch (arg->i) {
		case MoveDown:  sel->geom.y += MOVE_STEP; break;
		case MoveRight: sel->geom.x += MOVE_STEP; break;
		case MoveUp:    sel->geom.y -= MOVE_STEP; break;
		case MoveLeft:  sel->geom.x -= MOVE_STEP; break;
	}
	movewin(sel->win, sel->geom.x, sel->geom.y);
}

void
movewin(xcb_window_t win, int x, int y) {
	unsigned int pos[2] = { x, y };
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_X|XCB_CONFIG_WINDOW_Y, pos);
}

void
moveresize(Client *c, int x, int y, int w, int h) {
	if (!c || !c->win)
		return;
	uint32_t val[5] = { x, y, w, h };
	uint32_t mask = XCB_CONFIG_WINDOW_X |
		XCB_CONFIG_WINDOW_Y |
		XCB_CONFIG_WINDOW_WIDTH |
		XCB_CONFIG_WINDOW_HEIGHT;
	xcb_configure_window(conn, c->win, mask, val);
}


void
raisewindow(xcb_drawable_t win) {
	uint32_t values[] = { XCB_STACK_MODE_ABOVE };
	if (screen->root == win || !win)
		return;
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

void
resize(const Arg *arg) {
	int iw = RESIZE_STEP, ih = RESIZE_STEP;
	if (!sel)
		return;

	if (sel->incw > 7 && sel->incw < screen->width_in_pixels)
		iw = sel->incw;
	if (sel->inch > 7 && sel->inch < screen->height_in_pixels)
		ih = sel->inch;

	if (arg->i == GrowHeight || arg->i == GrowBoth) {
		sel->geom.height = sel->geom.height + ih;
	}
	if (arg->i == GrowWidth || arg->i == GrowBoth) {
		sel->geom.width = sel->geom.width + iw;
	}
	if (arg->i == ShrinkHeight || arg->i == ShrinkBoth) {
		if (sel->geom.height - ih > sel->minh)
			sel->geom.height = sel->geom.height - ih;
	}
	if (arg->i == ShrinkWidth || arg->i == ShrinkBoth) {
		if (sel->geom.width - iw > sel->minw)
			sel->geom.width = sel->geom.width - iw;
	}
	resizewin(sel->win, sel->geom.width, sel->geom.height);

	if (sel->ismax) {
		sel->ismax = false;
		setborderwidth(sel, BORDER_WIDTH);
	}
	sel->ishormax = sel->isvertmax = false;
	setborder(sel, true);
}

void
resizewin(xcb_window_t win, int w, int h) {
	unsigned int values[2] = { w, h };
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_WIDTH |
						 XCB_CONFIG_WINDOW_HEIGHT, values);
}

void
savegeometry(Client *c) {
	c->oldgeom.x = c->geom.x;
	c->oldgeom.y = c->geom.y;
	c->oldgeom.height = c->geom.height;
	c->oldgeom.width = c->geom.width;
}

bool
sendevent(Client *c, xcb_atom_t proto) {
	xcb_client_message_event_t ev;
	xcb_get_property_cookie_t cookie;
	xcb_icccm_get_wm_protocols_reply_t reply;
	bool exists = false;
	cookie = xcb_icccm_get_wm_protocols(conn, c->win, ewmh->WM_PROTOCOLS);
	if (xcb_icccm_get_wm_protocols_reply(conn, cookie, &reply, NULL) == 1) {
		for (unsigned int i = 0; i < reply.atoms_len && !exists; i++)
			if (reply.atoms[i] == proto)
				exists = true;
		xcb_icccm_get_wm_protocols_reply_wipe(&reply);
	}
	if (exists) {
		ev.response_type = XCB_CLIENT_MESSAGE;
		ev.format = 32;
		ev.sequence = 0;
		ev.window = c->win;
		ev.type = ewmh->WM_PROTOCOLS;
		ev.data.data32[0] = proto;
		ev.data.data32[1] = XCB_CURRENT_TIME;
		xcb_send_event(conn, true, c->win, XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
	}
	return exists;
}

void
sendtows(const Arg *arg) {
	if (arg->i == selws)
		return;
	sel->ws = arg->i;
	showhide(stack);
	focus(NULL);
}

void
setborder(Client *c, bool focus) {
	if (c->ismax || c->noborder)
		return;
	uint32_t values[1];
	int half = OUTER_BORDER_WIDTH;
	values[0] = BORDER_WIDTH;
	xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, values);
	if (! DOUBLE_BORDER) {
		values[0] = focus ? focuscol : unfocuscol;
		xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, values);
		return;
	}
	xcb_pixmap_t pmap = xcb_generate_id(conn);
	xcb_gcontext_t gc = xcb_generate_id(conn);
	xcb_create_pixmap(conn, screen->root_depth, pmap, screen->root,
					  c->geom.width+BORDER_WIDTH*2, c->geom.height+BORDER_WIDTH*2);
	xcb_create_gc(conn, gc, pmap, 0, NULL);

	values[0] = outercol;
	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &values[0]);
	xcb_rectangle_t rect_outer[] = {
		{ c->geom.width+BORDER_WIDTH-half, 0, half, c->geom.height+BORDER_WIDTH*2 },
		{ c->geom.width+BORDER_WIDTH, 0, half, c->geom.height+BORDER_WIDTH*2 },
		{ 0, c->geom.height+BORDER_WIDTH-half, c->geom.width+BORDER_WIDTH*2, half },
		{ 0, c->geom.height+BORDER_WIDTH, c->geom.width+BORDER_WIDTH*2, half },
		{ 1, 1, 1, 1 }
	};
	xcb_poly_fill_rectangle(conn, pmap, gc, 5, rect_outer);

	values[0] = focus ? focuscol : unfocuscol;
	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &values[0]);
	xcb_rectangle_t rect_inner[] = {
		{ c->geom.width, 0, BORDER_WIDTH-half, c->geom.height+BORDER_WIDTH-half },
		{ c->geom.width+BORDER_WIDTH+half, 0, BORDER_WIDTH-half, c->geom.height+BORDER_WIDTH-half },
		{ 0, c->geom.height, c->geom.width+BORDER_WIDTH-half, BORDER_WIDTH-half },
		{ 0, c->geom.height+BORDER_WIDTH+half, c->geom.width+BORDER_WIDTH-half, BORDER_WIDTH-half },
		{ c->geom.width+BORDER_WIDTH+half, BORDER_WIDTH+c->geom.height+half, BORDER_WIDTH, BORDER_WIDTH }
	};
	xcb_poly_fill_rectangle(conn, pmap, gc, 5, rect_inner);

	values[0] = pmap;
	xcb_change_window_attributes(conn,c->win, XCB_CW_BORDER_PIXMAP, &values[0]);
	xcb_free_pixmap(conn, pmap);
	xcb_free_gc(conn, gc);
}

void
setborderwidth(Client *c, int width) {
	if (c->noborder)
		return;
	uint32_t value[1] = { width };
	xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, value);
}

void
showhide(Client *c) {
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		movewin(c->win, c->geom.x, c->geom.y);
		showhide(c->snext);
	}
	else {
		showhide(c->snext);
		// TODO: set iconic state
		movewin(c->win, WIDTH(c) * -2, c->geom.y);
	}
}

// FIXME
void
sticky(const Arg *arg) {
	(void)arg;
	if (!sel)
		return;
	if (sel->isfixed) {
		sel->isfixed = false;
		sel->ws = selws;
	}
	else {
		sel->isfixed = true;
		raisewindow(sel->win);
	}
}

void
teleport(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	uint16_t tw = sel->geom.width;
	uint16_t th = sel->geom.height;
	if (!sel->noborder) {
		tw +=  BORDER_WIDTH * 2;
		th +=  BORDER_WIDTH * 2;
	}
	switch (arg->i) {
		case Center:
			sel->geom.x = (screen->width_in_pixels - tw) / 2;
			sel->geom.y = (screen->height_in_pixels - th) / 2;
			break;
		case TopLeft:
			sel->geom.x = 0;
			sel->geom.y = 0;
			break;
		case TopRight:
			sel->geom.x = screen->width_in_pixels - tw;
			sel->geom.y = 0;
			break;
		case BottomLeft:
			sel->geom.x = 0;
			sel->geom.y = screen->height_in_pixels - th;
			break;
		case BottomRight:
			sel->geom.x = screen->width_in_pixels - tw;
			sel->geom.y = screen->height_in_pixels - th;
			break;
	}
	movewin(sel->win, sel->geom.x, sel->geom.y);
}

void
unmanage(Client *c) {
	sendevent(c, WM_DELETE_WINDOW);
	detach(c);
	detachstack(c);
	free(c);
	focus(NULL);
}


Client *
wintoclient(xcb_window_t w) {
	Client *c;
	for (c = clients; c; c = c->next)
		if (w == c->win)
			return c;
	return NULL;
}

#ifdef SHAPE
void
check_shape_extension() {
	xcb_query_extension_reply_t const* ereply = xcb_get_extension_data(conn, &xcb_shape_id);
	if (!ereply)
		err("can't get shape extension data.");
	if (!ereply->present)
		warn("SHAPE extension isn't available");
	xcb_shape_query_version_cookie_t vcookie = xcb_shape_query_version_unchecked(conn);
	xcb_shape_query_version_reply_t* vreply = xcb_shape_query_version_reply(conn, vcookie, 0);
	if (!vreply)
		err("can't get shape extension version.");
	free(vreply);
}


void
roundcorners(Client *c) {
	xcb_pixmap_t pmap = xcb_create_pixmap_from_bitmap_data(conn,
														   screen->root,
														   corner_bits,
														   corner_width, corner_height,
														   1, // depth
														   0, // fg
														   1, // bg
														   NULL); // gc
	if (pmap == XCB_NONE)
		err("xcb_create_pixmap_from_bitmap_data() failed.");

	xcb_shape_mask(conn,
				   XCB_SHAPE_SO_SUBTRACT,
				   XCB_SHAPE_SK_BOUNDING,
				   c->win,
				   0, 0,
				   pmap);
	xcb_free_pixmap(conn, pmap);
}
#endif

