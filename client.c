/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <xcb/xcb_icccm.h>
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "keys.h"
#include "ewmh.h"
#include "xcb.h"
#include "config.h"
#include "util.h"

void
applyrules(Client *c) {
	unsigned int                    i;
	const Rule                     *rule;
	xcb_get_property_cookie_t       cookie;
	xcb_icccm_get_wm_class_reply_t  reply;

	ewmh_get_wm_window_type(c);
	ewmh_get_wm_state(c);

	/* custom rules */
	cookie = xcb_icccm_get_wm_class(conn, c->win);

	if (!xcb_icccm_get_wm_class_reply(conn, cookie, &reply, NULL))
		return;

	for (i = 0; i < LENGTH(rules); i++) {
		rule = &rules[i];
		if ((rule->class && strstr(reply.class_name, rule->class))) {
			if (!rule->border)
				c->noborder = true;
		}
	}
	xcb_icccm_get_wm_class_reply_wipe(&reply);
}

void
cycleclients(const Arg *arg) {
	if (arg->i == NextWindow)
		focusstack(true);
	else
		focusstack(false);
}

void
fit_in_screen(Client *c) {
	bool update = false;

	if (c->noborder)
		return;

	if (c->geom.width >= screen->width_in_pixels - 2 * border_width) {
		update = true;
		c->geom.width = screen->width_in_pixels - 2 * border_width;
	}

	if (c->geom.height >= screen->height_in_pixels - 2 * border_width) {
		update = true;
		c->geom.height = screen->height_in_pixels - 2 * border_width;
	}

	if (update) {
		PRINTF("fit_in_screen: resize win %#x to (%d,%d) %dx%d\n",
			c->win, c->geom.x, c->geom.y,
			c->geom.width, c->geom.height);

		c->geom.x = c->geom.y = 0;
		moveresize_win(c->frame, c->geom.x, c->geom.y,
			       c->geom.width, c->geom.height);
	}
}

void
killselected(const Arg *arg) {
	(void)arg;
	if (!sel)
		return;

	if (sel->can_delete)
		send_client_message(sel, WM_DELETE_WINDOW);
	else
		xcb_kill_client(conn, sel->win);
}

void
manage(xcb_window_t w) {
	Client                             *c;
	xcb_get_geometry_reply_t           *gr;
	xcb_get_property_cookie_t           nhc;
	xcb_get_property_cookie_t           hc;
	xcb_get_property_cookie_t           pc;
	xcb_icccm_wm_hints_t                wmh;
	xcb_icccm_get_wm_protocols_reply_t  pr;
	unsigned int                        i;
	uint32_t                            val[1];

	PRINTF("manage: manage window %#x\n", w);

	if (!(c = malloc(sizeof(Client))))
		err("can't allocate memory.");

	c->win = w;

	/* get geometry */
	gr = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, w), NULL);

	if (gr) {
		c->geom.x = c->old_geom.x = gr->x;
		c->geom.y = c->old_geom.y = gr->y;
		c->geom.width = c->old_geom.width = gr->width;
		c->geom.height = c->old_geom.height = gr->height;
#if DEBUG
		if (c->geom.x)
			PRINTF("x: %d\n", c->geom.x);
		if (c->geom.y)
			PRINTF("y: %d\n", c->geom.y);
		if (c->geom.width)
			PRINTF("width: %d\n", c->geom.width);
		if (c->geom.height)
			PRINTF("height: %d\n", c->geom.height);
#endif
		FREE(gr);
	} else {
		warn("xcb_get_geometry failed for win %#x.", w);
	}

	c->size_hints.flags = 0;
	c->size_hints.x = c->size_hints.y = 0;
	c->size_hints.width = c->size_hints.height = 0;
	c->size_hints.min_width = c->size_hints.min_height = 0;
	c->size_hints.max_width = c->size_hints.max_height = 0;
	c->size_hints.width_inc = c->size_hints.height_inc = 0;
	c->size_hints.min_aspect_num = c->size_hints.min_aspect_den = 0;
	c->size_hints.max_aspect_num = c->size_hints.max_aspect_den = 0;
	c->size_hints.base_width = c->size_hints.base_height = 0;
	c->size_hints.win_gravity = 0;
	c->can_focus = c->can_delete =  c->noborder = false;
	c->frame = XCB_NONE;
	c->ewmh_flags = 0;
	c->ws = selws;

	/* get size hints */
	nhc = xcb_icccm_get_wm_normal_hints(conn, c->win);
	xcb_icccm_get_wm_normal_hints_reply(conn, nhc, &c->size_hints, NULL);
#if DEBUG
	if (c->size_hints.x)
		PRINTF("x: %d\n", c->size_hints.x);
	if (c->size_hints.y)
		PRINTF("y: %d\n", c->size_hints.y);
	if (c->size_hints.width)
		PRINTF("width: %d\n", c->size_hints.width);
	if (c->size_hints.height)
		PRINTF("height: %d\n", c->size_hints.height);
	if (c->size_hints.min_height)
		PRINTF("min height: %d\n", c->size_hints.min_height);
	if (c->size_hints.min_width)
		PRINTF("min width: %d\n", c->size_hints.min_width);
	if (c->size_hints.max_height)
		PRINTF("max height: %d\n", c->size_hints.max_height);
	if (c->size_hints.max_width)
		PRINTF("max width: %d\n", c->size_hints.max_width);
	if (c->size_hints.base_height)
		PRINTF("base height: %d\n", c->size_hints.base_height);
	if (c->size_hints.base_width)
		PRINTF("base width: %d\n", c->size_hints.base_width);
#endif

	/* get wm hints */
	hc = xcb_icccm_get_wm_hints(conn, c->win);
	xcb_icccm_get_wm_hints_reply(conn, hc, &wmh, NULL);
	c->wm_hints = wmh.flags;

	if (c->wm_hints & XCB_ICCCM_WM_HINT_X_URGENCY) {
		PRINTF("ICCCM: Urgent win %#x\n", c->win);
	}

	/* get protocols */
	pc = xcb_icccm_get_wm_protocols(conn, c->win, WM_PROTOCOLS);
	if (xcb_icccm_get_wm_protocols_reply(conn, pc, &pr, NULL) == 1) {
		for (i = 0; i < pr.atoms_len; ++i) {
			if (pr.atoms[i] == WM_DELETE_WINDOW)
				c->can_delete = true;
			if (pr.atoms[i] == WM_TAKE_FOCUS)
				c->can_focus = true;
		}
		xcb_icccm_get_wm_protocols_reply_wipe(&pr);
	}

	applyrules(c);

	if (center_new_windows) {
		c->geom.x = (screen->width_in_pixels - BWIDTH(c)) / 2;
		c->geom.y = (screen->height_in_pixels - BHEIGHT(c)) / 2;
		PRINTF("manage: centering to (%d,%d)\n", c->geom.x, c->geom.y);
		movewin(c->win, c->geom.x, c->geom.y);
		warp_pointer(c);
	}

	reparent(c);
	attach(c);
	attachstack(c);
	sel = c;
	fit_in_screen(c);

	if (sloppy_focus) {
		val[0] = CLIENT_EVENT_MASK;
		xcb_change_window_attributes(conn, w, XCB_CW_EVENT_MASK, val);
	}

	if (c->ws == selws)
		xcb_map_window(conn, w);

	warp_pointer(c);
	ewmh_update_client_list(clients);
	focus(NULL);
}

void
reparent(Client *c) {
	int16_t  x, y;
	uint16_t width, height;
	uint32_t vals[3];
	uint32_t mask;

	x = c->geom.x;
	y = c->geom.y;
	width = c->geom.width;
	height = c->geom.height;
#if DEBUG
	if (c->size_hints.flags & XCB_ICCCM_SIZE_HINT_P_WIN_GRAVITY &&
	    c->size_hints.win_gravity > 1)
		PRINTF("reparent: NONSTANDARD GRAVITY: %d\n",
		       c->size_hints.win_gravity);
#endif
	switch (GRAVITY(c)) {
	case XCB_GRAVITY_NORTH_WEST:
	case XCB_GRAVITY_BIT_FORGET:
		break;
	case XCB_GRAVITY_NORTH:
		x -= border_width;
		break;
	case XCB_GRAVITY_NORTH_EAST:
		x -= 2 * border_width;
		break;
	case XCB_GRAVITY_EAST:
		x -= 2 * border_width;
		y -= border_width;
		break;
	case XCB_GRAVITY_SOUTH_EAST:
		x -= 2 * border_width;
		y -= 2 * border_width;
		break;
	case XCB_GRAVITY_SOUTH:
		x -= border_width;
		y -= 2 * border_width;
		break;
	case XCB_GRAVITY_SOUTH_WEST:
		y -= 2 * border_width;
		break;
	case XCB_GRAVITY_WEST:
		y -= border_width;
		break;
	case XCB_GRAVITY_STATIC:
		y -= border_width;
		x -= border_width;
		break;
	case XCB_GRAVITY_CENTER:
		x -= border_width;
		y -= border_width;
		break;
	}

	c->frame = xcb_generate_id(conn);

	mask = XCB_CW_BORDER_PIXEL
		| XCB_CW_OVERRIDE_REDIRECT
		| XCB_CW_EVENT_MASK;
	vals[0] = focuscol;
	vals[1] = true;
	vals[2] = FRAME_EVENT_MASK;

	PRINTF("reparent: creating frame (%d,%d) %dx%d\n", x, y, width, height);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, c->frame, screen->root,
			  x, y, width, height,
			  c->noborder ? 0 : border_width,
			  XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
			  mask, vals);

	xcb_configure_window(conn, c->frame,
			     XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
			     (uint32_t[]){ c->geom.width, c->geom.height });

	xcb_map_window(conn, c->frame);

#ifdef SHAPE
	if (shape_ext)
		roundcorners(c);
#endif
	PRINTF("reparent: reparenting win %#x to %#x\n", c->win, c->frame);
	xcb_reparent_window(conn, c->win, c->frame, 0, 0);
}

void
maximize(const Arg *arg) {
	(void)arg;
	if (!sel)
		return;
	maximizeclient(sel, !ISFULLSCREEN(sel));
}

void
maximizeaxis(const Arg *arg) {
	if (!sel || ISFULLSCREEN(sel))
		return;
	maximizeaxis_client(sel, arg->i);
}

void
maximizeaxis_client(Client *c, uint16_t direction) {
	uint32_t values[3];
	uint16_t tw, th;

	if (ISMAXVERT(c) || ISMAXHORZ(c)) {
		maximizeclient(c, false);
		return;
	}

	savegeometry(c);
	tw = screen->width_in_pixels;
	th = screen->height_in_pixels;

	if (!c->noborder) {
		tw -= border_width * 2;
		th -= border_width * 2;
	}

	if (direction == MaxVertical) {
		c->geom.y = 0;
		c->geom.height = th;
		values[0] = c->geom.y;
		values[1] = c->geom.height;
		xcb_configure_window(conn, c->frame, XCB_CONFIG_WINDOW_Y
				     | XCB_CONFIG_WINDOW_HEIGHT, values);
		xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_Y
				     | XCB_CONFIG_WINDOW_HEIGHT, values);
		change_ewmh_flags(c, ADD_STATE, EWMH_MAXIMIZED_VERT);
		ewmh_update_wm_state(c);
	} else if (direction == MaxHorizontal) {
		c->geom.x = 0;
		c->geom.width = tw;
		values[0] = c->geom.x;
		values[1] = c->geom.width;
		xcb_configure_window(conn, c->frame, XCB_CONFIG_WINDOW_X
				     | XCB_CONFIG_WINDOW_WIDTH, values);
		xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_X
				     | XCB_CONFIG_WINDOW_WIDTH, values);
		change_ewmh_flags(c, ADD_STATE, EWMH_MAXIMIZED_HORZ);
		ewmh_update_wm_state(c);
	}

	setborder(c, true);
	warp_pointer(c);
}

void
maximizeclient(Client *c, bool doit) {
	if (!c)
		return;
	if (ISFULLSCREEN(c) && doit) {
		savegeometry(c);
		return;
	}
	PRINTF("maximizeclient: %s to ", doit ? "maximizing" : "unmaximizing");

	if (doit) {
		PRINTF("fullscreen\n");
		savegeometry(c);
		change_ewmh_flags(c, ADD_STATE, EWMH_FULLSCREEN);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_HORZ);
		ewmh_update_wm_state(c);
		c->geom.x = 0;
		c->geom.y = 0;
		c->geom.width = screen->width_in_pixels;
		c->geom.height = screen->height_in_pixels;
		setborderwidth(c->frame, 0);
		moveresize_win(c->frame, c->geom.x, c->geom.y,
			       c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
		focus(NULL);
	} else {
		c->geom.x = ISMAXVERT(c) ? c->geom.x : MAX(0, c->old_geom.x);
		c->geom.y = ISMAXHORZ(c) ? c->geom.y : MAX(0, c->old_geom.y);
		c->geom.width = c->old_geom.width;
		c->geom.height = c->old_geom.height;
		PRINTF("(%d,%d) %dx%d\n",
		       c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_FULLSCREEN);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_HORZ);
		ewmh_update_wm_state(c);
		if (!c->noborder)
			setborderwidth(c->frame, border_width);
		moveresize_win(c->frame, c->geom.x, c->geom.y,
			       c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
	}

	warp_pointer(c);
}

void
move(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;

	switch (arg->i) {
	case MoveDown:  sel->geom.y += move_step; break;
	case MoveRight: sel->geom.x += move_step; break;
	case MoveUp:    sel->geom.y -= move_step; break;
	case MoveLeft:  sel->geom.x -= move_step; break;
	default: warn("move: bad arg %d\n", arg->i);
	}

	movewin(sel->frame, sel->geom.x, sel->geom.y);
	warp_pointer(sel);
}

void
movewin(xcb_window_t win, int16_t x, int16_t y) {
	const uint32_t values[] = { x, y };
	const uint32_t mask = XCB_CONFIG_WINDOW_X
		| XCB_CONFIG_WINDOW_Y;
	xcb_configure_window(conn, win, mask, values);
}

void
moveresize_win(xcb_window_t win, int16_t x, int16_t y, uint16_t w, uint16_t h) {
	const uint32_t values[] = { x, y, w, h };
	const uint32_t mask = XCB_CONFIG_WINDOW_X
		| XCB_CONFIG_WINDOW_Y
		| XCB_CONFIG_WINDOW_WIDTH
		| XCB_CONFIG_WINDOW_HEIGHT;
	xcb_configure_window(conn, win, mask, values);
}


void
raisewindow(xcb_drawable_t win) {
	if (screen->root == win || !win)
		return;
	PRINTF("raissewindow: %#x\n", win);
	const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

void
resize(const Arg *arg) {
	int32_t iw, ih;

	if (!sel)
		return;

	iw = resize_step;
	ih = resize_step;

	if (sel->size_hints.width_inc > 0)
		iw = sel->size_hints.width_inc;
	if (sel->size_hints.height_inc > 0)
		ih = sel->size_hints.height_inc;

	if (arg->i == GrowHeight || arg->i == GrowBoth) {
		if (sel->size_hints.max_height > 0) {
			sel->geom.height = MIN(sel->geom.height + ih,
					       sel->size_hints.max_height);
		} else {
			sel->geom.height += ih;
		}
	}

	if (arg->i == GrowWidth || arg->i == GrowBoth) {
		if (sel->size_hints.max_width > 0) {
			sel->geom.width = MIN(sel->geom.width + ih,
					       sel->size_hints.max_width);
		} else {
			sel->geom.width += ih;
		}
	}

	if (arg->i == ShrinkHeight || arg->i == ShrinkBoth) {
		if (sel->geom.height - ih > sel->size_hints.min_height)
			sel->geom.height = sel->geom.height - ih;
	}

	if (arg->i == ShrinkWidth || arg->i == ShrinkBoth) {
		if (sel->geom.width - iw > sel->size_hints.min_width)
			sel->geom.width = sel->geom.width - iw;
	}

	resizewin(sel->frame, sel->geom.width, sel->geom.height);
	resizewin(sel->win, sel->geom.width, sel->geom.height);

	if (ISFULLSCREEN(sel)) {
		change_ewmh_flags(sel, REMOVE_STATE, EWMH_FULLSCREEN);
		setborderwidth(sel->frame, border_width);
	}

	if (ISMAXVERT(sel))
		change_ewmh_flags(sel, REMOVE_STATE, EWMH_MAXIMIZED_VERT);

	if (ISMAXHORZ(sel))
		change_ewmh_flags(sel, REMOVE_STATE, EWMH_MAXIMIZED_HORZ);

	ewmh_update_wm_state(sel);
	setborder(sel, true);
	warp_pointer(sel);
}

void
resizewin(xcb_window_t win, uint16_t w, uint16_t h) {
	PRINTF("reszewin: win %#x to %dx%d\n", win, w, h);
	const uint32_t values[] = { w, h };
	const uint32_t mask = XCB_CONFIG_WINDOW_WIDTH
		| XCB_CONFIG_WINDOW_HEIGHT;
	xcb_configure_window(conn, win, mask, values);
}

void
savegeometry(Client *c) {
	PRINTF("savegeom: (%d,%d) %dx%d\n",
	       c->geom.x, c->geom.y, c->geom.width, c->geom.height);
	c->old_geom.x = c->geom.x;
	c->old_geom.y = c->geom.y;
	c->old_geom.width = c->geom.width;
	c->old_geom.height = c->geom.height;
}

void
send_client_message(Client *c, xcb_atom_t proto) {
	xcb_client_message_event_t ev;
#if DEBUG
	char *name = get_atom_name(proto);
	PRINTF("send_client_message: %s to win %#x\n", name, c->win);
	FREE(name);
#endif
	memset(&ev, '\0', sizeof ev);
	ev.response_type = XCB_CLIENT_MESSAGE;
	ev.format = 32;
	ev.sequence = 0;
	ev.window = c->win;
	ev.type = ewmh->WM_PROTOCOLS;
	ev.data.data32[0] = proto;
	ev.data.data32[1] = XCB_CURRENT_TIME;
	xcb_send_event(conn, false, c->win,
		       XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
}

void
setborder(Client *c, bool focus) {
	uint32_t       val[1];
	/* xcb_pixmap_t   pmap; */
	/* xcb_gcontext_t gc; */

	if (ISFULLSCREEN(c) || c->noborder)
		return;

	/* if (!pixmap_border) { */
		val[0] = focus ? focuscol : unfocuscol;
		xcb_change_window_attributes(conn, c->frame,
					     XCB_CW_BORDER_PIXEL, val);
		return;
	/* } */

	/* pmap = xcb_generate_id(conn); */
	/* gc = xcb_generate_id(conn); */
	/* xcb_create_pixmap(conn, screen->root_depth, pmap, screen->root, */
	/* 		  c->geom.width + border_width * 2, */
	/* 		  c->geom.height); */
	/* xcb_create_gc(conn, gc, pmap, 0, NULL); */
	/* val[0] = focuscol; */
	/* xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &val[0]); */

	/* xcb_rectangle_t rectangles[] = { { 0, 0, c->geom.width, 5 } }; */
	/* xcb_poly_fill_rectangle(conn, pmap, gc, 1, rectangles); */

	/* val[0] = pmap; */
	/* xcb_change_window_attributes(conn, c->win, */
	/* 			     XCB_CW_BORDER_PIXMAP, &val[0]); */

	/* xcb_free_pixmap(conn, pmap); */
	/* xcb_free_gc(conn, gc); */
}

void
setborderwidth(xcb_window_t win, uint16_t bw) {
	const uint32_t value[] = { bw };
	const uint32_t mask = XCB_CONFIG_WINDOW_BORDER_WIDTH;
	xcb_configure_window(conn, win, mask, value);
}

void
showhide(Client *c) {
	if (!c)
		return;
	if (ISVISIBLE(c)) {
		movewin(c->frame, c->geom.x, c->geom.y);
		showhide(c->snext);
	}
	else {
		showhide(c->snext);
		movewin(c->frame, WIDTH(c) * -2, c->geom.y);
	}
}

void
spawn(const Arg *arg) {
	run_program(arg->com);
}

void
teleport(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	teleport_client(sel, arg->i);
}

void
teleport_client(Client *c, uint16_t location) {
	switch (location) {
	case Center:
		c->geom.x = (screen->width_in_pixels - BWIDTH(c)) / 2;
		c->geom.y = (screen->height_in_pixels - BHEIGHT(c)) / 2;
		break;
	case TopLeft:
		c->geom.x = 0;
		c->geom.y = 0;
		break;
	case TopRight:
		c->geom.x = screen->width_in_pixels - BWIDTH(c);
		c->geom.y = 0;
		break;
	case BottomLeft:
		c->geom.x = 0;
		c->geom.y = screen->height_in_pixels - BHEIGHT(c);
		break;
	case BottomRight:
		c->geom.x = screen->width_in_pixels - BWIDTH(c);
		c->geom.y = screen->height_in_pixels - BHEIGHT(c);
		break;
	default:
		warn("teleport_client: bad arg %d\n", location);
	}

	PRINTF("teleport_client: win %#x to (%d,%d)\n",
	       c->frame, c->geom.x, c->geom.y);
	if (c->frame)
		movewin(c->frame, c->geom.x, c->geom.y);
	else if (c->win)
		movewin(c->win, c->geom.x, c->geom.y);
	warp_pointer(c);
}

void
maximize_half(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	maximize_half_client(sel, arg->i);
}

void
maximize_half_client(Client *c, uint16_t location) {
	const uint16_t sw = screen->width_in_pixels - 2*border_width;
	const uint16_t sh = screen->height_in_pixels - 2*border_width;
	const uint16_t half_sw = (screen->width_in_pixels/2) - 2*border_width;
	const uint16_t half_sh = (screen->height_in_pixels/2) - 2*border_width;

	savegeometry(c);

	switch (location) {
	case Left:
		c->geom.x = 0;
		c->geom.y = 0;
		c->geom.width = half_sw;
		c->geom.height = sh;
		moveresize_win(c->frame, c->geom.x, c->geom.y,
			       c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
		change_ewmh_flags(c, ADD_STATE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_HORZ);
		break;
	case Right:
		c->geom.y = 0;
		c->geom.width = half_sw;
		c->geom.x = screen->width_in_pixels -
			(c->geom.width + 2*border_width);
		c->geom.height = sh;
		moveresize_win(c->frame, c->geom.x, c->geom.y,
			       c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
		change_ewmh_flags(c, ADD_STATE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_HORZ);
		break;
	case Top:
		c->geom.x = 0;
		c->geom.y = 0;
		c->geom.width = sw;
		c->geom.height = half_sh;
		moveresize_win(c->frame, c->geom.x, c->geom.y,
			       c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
		change_ewmh_flags(c, ADD_STATE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_HORZ);
		break;
	case Bottom:
		c->geom.x = 0;
		c->geom.width = sw;
		c->geom.height = half_sh;
		c->geom.y = screen->height_in_pixels -
			(c->geom.height + 2*border_width);
		moveresize_win(c->frame, c->geom.x, c->geom.y,
			       c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
		change_ewmh_flags(c, ADD_STATE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, REMOVE_STATE, EWMH_MAXIMIZED_HORZ);
		break;
	default:
		warn("maximize_half_client: bad arg %d\n", location);
	}
	change_ewmh_flags(c, REMOVE_STATE, EWMH_FULLSCREEN);
	ewmh_update_wm_state(c);

	PRINTF("maximize_half win %#x to (%d,%d) %dx%d\n",
	       c->frame, c->geom.x, c->geom.y, c->geom.width, c->geom.height);

	setborder(c, true);
	warp_pointer(c);
}

void
unmanage(Client *c) {
	PRINTF("unmanage: %#x\n", c->win);
	detach(c);
	detachstack(c);
	if (c->frame)
		xcb_destroy_window(conn, c->frame);
	FREE(c);
	ewmh_update_client_list(clients);
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
		PRINTF("focus: %#x\n", c->win);
		xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT,
				c->win, XCB_CURRENT_TIME);
		xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
				    ewmh->_NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW,
				    32, 1, &c->win);
		warp_pointer(c);
	} else {
		xcb_delete_property(conn, screen->root,
				    ewmh->_NET_ACTIVE_WINDOW);
		xcb_set_input_focus(conn, XCB_NONE,
				    XCB_INPUT_FOCUS_POINTER_ROOT,
				    XCB_CURRENT_TIME);
	}

	sel = c;
}

