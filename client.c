/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <xcb/xcb_icccm.h>
#ifdef SHAPE
#include <xcb/shape.h>
#include <xcb/xcb_image.h>
#include "corner"
#endif
#include "tfwm.h"
#include "list.h"
#include "client.h"

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
fitclient(Client *c) {
	bool update = false;

	if (c->noborder)
		return;
	if (c->w >= sw-2*BORDER_WIDTH) {
		c->w = sw - BORDER_WIDTH * 2;
		update = true;
	}
	if (c->h >= sh-2*BORDER_WIDTH) {
		c->h = sh - BORDER_WIDTH * 2;
		update = true;
	}
	if (update) {
		c->x = c->y = 0;
		moveresize(c, c->x, c->y, c->w, c->h);
	}
}

void
gethints(Client *c) {
	xcb_size_hints_t h;
	xcb_generic_error_t *e;
	xcb_icccm_get_wm_normal_hints_reply(conn,
			xcb_icccm_get_wm_normal_hints_unchecked(conn, c->win), &h, &e);
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
		if (h.min_width > 0 && h.min_width < sw)
			c->minw = h.min_width;
		if (h.min_height > 0 && h.min_height < sh)
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
	if (c->basew < sw)
		c->w = MAX(c->w, c->basew);
	if (c->baseh < sh)
		c->h = MAX(c->h, c->baseh);
	if (c->minw < sw)
		c->w = MAX(c->w, c->minw);
	if (c->minh < sh)
		c->h = MAX(c->h, c->minh);

	resizewin(c->win, c->w, c->h);
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
	c->x = c->oldx = geom->x;
	c->y = c->oldy = geom->y;
	c->w = c->oldw = geom->width;
	c->h = c->oldh = geom->height;
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
	if (c->ws == selws)
		xcb_map_window(conn, w);
	/* set its workspace hint */
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, c->win, ewmh->_NET_WM_DESKTOP,
			XCB_ATOM_CARDINAL, 32, 1, &selws);
	/* set normal state */
	long data[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, w, ewmh->_NET_WM_STATE,
			ewmh->_NET_WM_STATE, 32, 2, data);

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
	uint16_t tw = sw;
	uint16_t th = sh;
	if (!sel->noborder) {
		tw -= BORDER_WIDTH * 2;
		th -= BORDER_WIDTH * 2;
	}
	if (arg->i == MaxVertical) {
		sel->y = 0;
		sel->h = th;
		values[0] = sel->y;
		values[1] = sel->h;
		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_Y
				| XCB_CONFIG_WINDOW_HEIGHT, values);
		sel->isvertmax = true;
	}
	else if (arg->i == MaxHorizontal) {
		sel->x = 0;
		sel->w = tw;
		values[0] = sel->x;
		values[1] = sel->w;
		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_X
				| XCB_CONFIG_WINDOW_WIDTH, values);
		sel->ishormax = true;
	}
	setborder(sel, true);
}

void
maximizeclient(Client *c, bool add) {
	if (!c)
		return;
	if (!add) {
		c->x = c->isvertmax ? c->x : c->oldx;
		c->y = c->ishormax ? c->y : c->oldy;
		c->w = c->oldw;
		c->h = c->oldh;
		c->ismax = c->ishormax = c->isvertmax = 0;
		moveresize(c, c->x, c->y, c->w, c->h);
		setborderwidth(sel, BORDER_WIDTH);
		setborder(sel, true);
	}
	else {
		savegeometry(c);
		c->ismax = true;
		c->isvertmax = c->ishormax = false;
		c->x = 0;
		c->y = 0;
		c->w = sw;
		c->h = sh;
		moveresize(c, c->x, c->y, c->w, c->h);
		focus(NULL);
		setborderwidth(c, 0);
		long data[] = { c->ismax ? ewmh->_NET_WM_STATE_FULLSCREEN : XCB_ICCCM_WM_STATE_NORMAL };
		xcb_change_property(conn, XCB_PROP_MODE_REPLACE, c->win,
				ewmh->_NET_WM_STATE, XCB_ATOM_ATOM, 32, 1, data);
	}
}

void
move(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	switch (arg->i) {
		case MoveDown:  sel->y += MOVE_STEP; break;
		case MoveRight: sel->x += MOVE_STEP; break;
		case MoveUp:    sel->y -= MOVE_STEP; break;
		case MoveLeft:  sel->x -= MOVE_STEP; break;
	}
	movewin(sel->win, sel->x, sel->y);
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

	if (sel->incw > 7 && sel->incw < sw)
		iw = sel->incw;
	if (sel->inch > 7 && sel->inch < sh)
		ih = sel->inch;

	if (arg->i == GrowHeight || arg->i == GrowBoth) {
		sel->h = sel->h + ih;
	}
	if (arg->i == GrowWidth || arg->i == GrowBoth) {
		sel->w = sel->w + iw;
	}
	if (arg->i == ShrinkHeight || arg->i == ShrinkBoth) {
		if (sel->h - ih > sel->minh)
			sel->h = sel->h - ih;
	}
	if (arg->i == ShrinkWidth || arg->i == ShrinkBoth) {
		if (sel->w - iw > sel->minw)
			sel->w = sel->w - iw;
	}
	resizewin(sel->win, sel->w, sel->h);

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
	c->oldx = c->x;
	c->oldy = c->y;
	c->oldh = c->h;
	c->oldw = c->w;
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
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, sel->win,
			ewmh->_NET_WM_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &arg->i);
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
			c->w+BORDER_WIDTH*2, c->h+BORDER_WIDTH*2);
	xcb_create_gc(conn, gc, pmap, 0, NULL);

	/* See: https://github.com/venam/2bwm */
	values[0] = outercol;
	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &values[0]);
	xcb_rectangle_t rect_outer[] = {
		{ c->w+BORDER_WIDTH-half, 0,                      half,                c->h+BORDER_WIDTH*2 },
		{ c->w+BORDER_WIDTH,      0,                      half,                c->h+BORDER_WIDTH*2 },
		{ 0,                      c->h+BORDER_WIDTH-half, c->w+BORDER_WIDTH*2, half },
		{ 0,                      c->h+BORDER_WIDTH,      c->w+BORDER_WIDTH*2, half },
		{ 1, 1, 1, 1 }
	};
	xcb_poly_fill_rectangle(conn, pmap, gc, 5, rect_outer);

	values[0] = focus ? focuscol : unfocuscol;
	xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &values[0]);
	xcb_rectangle_t rect_inner[] = {
		{ c->w,                   0,                      BORDER_WIDTH-half,      c->h+BORDER_WIDTH-half},
		{ c->w+BORDER_WIDTH+half, 0,                      BORDER_WIDTH-half,      c->h+BORDER_WIDTH-half},
		{ 0,                      c->h,                   c->w+BORDER_WIDTH-half, BORDER_WIDTH-half},
		{ 0,                      c->h+BORDER_WIDTH+half, c->w+BORDER_WIDTH-half, BORDER_WIDTH-half},
		{ c->w+BORDER_WIDTH+half, BORDER_WIDTH+c->h+half, BORDER_WIDTH,           BORDER_WIDTH }
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
		movewin(c->win, c->x, c->y);
		showhide(c->snext);
	}
	else {
		showhide(c->snext);
		movewin(c->win, WIDTH(c) * -2, c->y);
	}
}

// TODO
void
sticky(const Arg *arg) {
	(void)arg;
	if (!sel)
		return;
	if (sel->isfixed) {
		sel->isfixed = false;
		sel->ws = selws;
		xcb_change_property(conn, XCB_PROP_MODE_REPLACE, sel->win,
				ewmh->_NET_WM_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &selws);
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
	uint16_t tw = sel->w;
	uint16_t th = sel->h;
	if (!sel->noborder) {
		tw +=  BORDER_WIDTH * 2;
		th +=  BORDER_WIDTH * 2;
	}
	switch (arg->i) {
		case ToCenter:
			sel->x = (sw - tw) / 2;
			sel->y = (sh - th) / 2;
			break;
		case ToTop:
			sel->y = 0;
			break;
		case ToBottom:
			sel->y = sh - th;
			break;
		case ToLeft:
			sel->x = 0;
			break;
		case ToRight:
			sel->x = sw - tw;
			break;
	}
	movewin(sel->win, sel->x, sel->y);
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

