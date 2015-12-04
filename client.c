/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <xcb/xcb_icccm.h>
#ifdef SHAPE
#include <xcb/shape.h>
#include <xcb/xcb_image.h>
#include "corner.xbm"
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
	const Rule *r;
	xcb_icccm_get_wm_class_reply_t ch;

	if (!xcb_icccm_get_wm_class_reply(conn, xcb_icccm_get_wm_class(conn, c->win), &ch, NULL))
		return;

	for (i = 0; i < LENGTH(rules); i++) {
		r = &rules[i];
		if ((r->class && strstr(ch.class_name, r->class))) {
			if (!r->border)
				c->noborder = true;
			/* if (r->fullscreen) */
			// TODO
			/* const Arg a = { .i = r->workspace }; */
			/* sendtows(&a); */
		}
	}
	xcb_icccm_get_wm_class_reply_wipe(&ch);
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
gethints(Client *c) {
	xcb_size_hints_t h;
	xcb_generic_error_t *e;
	xcb_icccm_get_wm_normal_hints_reply(conn, xcb_icccm_get_wm_normal_hints_unchecked(conn, c->win), &h, &e);
	if (e) {
		free(e);
		return;
	}
	free(e);

	// TODO
	/* if (h.flags & XCB_ICCCM_SIZE_HINT_US_POSITION) */
	/* 	PRINTF("Hint: US_POSITION: x: %d y: %d\n", h.x, h.y); */
	/* if (h.flags & XCB_ICCCM_SIZE_HINT_US_SIZE) */
	/* 	PRINTF("Hint: US_SIZE: width %d height %d\n", h.width, h.height); */
	/* if (h.flags & XCB_ICCCM_SIZE_HINT_P_POSITION) */
	/* 	PRINTF("Hint: P_POSITION: x: %d y: %d\n", h.x, h.y); */
	/* if (h.flags & XCB_ICCCM_SIZE_HINT_P_SIZE) */
	/* 	PRINTF("Hint: P_SIZE: width %d height %d\n", h.width, h.height); */
	/* if (h.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) */
	/* 	PRINTF("Hint: P_MAX_SIZE max_width %d max_height %d\n", h.max_width, h.max_width); */

	if (h.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
		if (h.min_width > 0 && h.min_width < screen->width_in_pixels)
			c->minw = h.min_width;
		if (h.min_height > 0 && h.min_height < screen->height_in_pixels)
			c->minh = h.min_height;
	}
	if (h.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
		c->basew = h.base_width;
		c->baseh = h.base_height;
	}
	if (h.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC) {
		c->incw = h.width_inc;
		c->inch = h.height_inc;
	}
	/* configure win */
	if (c->basew < screen->width_in_pixels)
		c->geom.width = MAX(c->geom.width, c->basew);
	if (c->baseh < screen->height_in_pixels)
		c->geom.height = MAX(c->geom.height, c->baseh);
	if (c->minw < screen->width_in_pixels)
		c->geom.width = MAX(c->geom.width, c->minw);
	if (c->minh < screen->height_in_pixels)
		c->geom.height = MAX(c->geom.height, c->minh);

	resizewin(c->win, c->geom.width, c->geom.height);
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
	c->ismax = c->isvertmax = c->ishormax = c->isfixed = c->noborder = false;
	gethints(c);
	attach(c);
	attachstack(c);
	sel = c;
	applyrules(c);
	fitclient(c);

#if SLOPPY_FOCUS
	uint32_t values[] = {CLIENT_EVENT_MASK};
	xcb_change_window_attributes(conn, w, XCB_CW_EVENT_MASK, values);
#endif

	if (c->ws == selws)
		xcb_map_window(conn, w);

	xcb_get_property_reply_t *reply;
	xcb_get_property_cookie_t cookie;
	cookie = xcb_get_property(conn, 0, w, ewmh->_NET_WM_STATE, XCB_ATOM_ATOM, 0, 1);

	if ((reply = xcb_get_property_reply(conn, cookie, NULL))) {
		if (xcb_get_property_value_length(reply) != 0) {
			if (reply->format == 32) {
				xcb_atom_t *v = xcb_get_property_value(reply);
				if (v[0] == ewmh->_NET_WM_STATE_FULLSCREEN)
					maximize(NULL);
			}
		}
		free(reply);
	}
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
	PRINTF("maximizeclient: %s\n", doit ? "max" : "unmax");
	if (doit) {
		savegeometry(c);
		c->ismax = true;
		c->isvertmax = c->ishormax = false;
		c->geom.x = 0;
		c->geom.y = 0;
		c->geom.width = screen->width_in_pixels;
		c->geom.height = screen->height_in_pixels;
		moveresize(c, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		focus(NULL);
		setborderwidth(c, 0);
	}
	else {
		c->geom.x = c->isvertmax ? c->geom.x : c->oldgeom.x;
		c->geom.y = c->ishormax ? c->geom.y : c->oldgeom.y;
		c->geom.width = c->oldgeom.width;
		c->geom.height = c->oldgeom.height;
		c->ismax = false;
		c->ishormax = c->isvertmax = 0;
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
		case ToCenter:
			sel->geom.x = (screen->width_in_pixels - tw) / 2;
			sel->geom.y = (screen->height_in_pixels - th) / 2;
			break;
		case ToTop:
			sel->geom.y = 0;
			break;
		case ToBottom:
			sel->geom.y = screen->height_in_pixels - th;
			break;
		case ToLeft:
			sel->geom.x = 0;
			break;
		case ToRight:
			sel->geom.x = screen->width_in_pixels - tw;
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

