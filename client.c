/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <xcb/xcb_icccm.h>
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "keys.h"
#include "ewmh.h"

void
applyrules(Client *c) {
	unsigned int i;
	const Rule *r;
	xcb_icccm_get_wm_class_reply_t class_reply;

	ewmh_get_wm_window_type(c);
	ewmh_get_wm_state(c);

	/* custom rules */
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
	if (c->geom.width >= screen->width_in_pixels-2*border_width) {
		c->geom.width = screen->width_in_pixels - border_width * 2;
		update = true;
	}
	if (c->geom.height >= screen->height_in_pixels-2*border_width) {
		c->geom.height = screen->height_in_pixels - border_width * 2;
		update = true;
	}
	if (update) {
		c->geom.x = c->geom.y = 0;
		moveresize_win(c->win, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
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
	Client *c = NULL;
	xcb_get_geometry_reply_t *gr;
	xcb_icccm_get_wm_protocols_reply_t pr;
	unsigned int i;

	if (!(c = malloc(sizeof(Client))))
		err("can't allocate memory.");

	c->win = w;

	/* geometry */
	gr = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, w), NULL);
	if (gr) {
		c->geom.x = c->old_geom.x = gr->x;
		c->geom.y = c->old_geom.y = gr->y;
		c->geom.width = c->old_geom.width = gr->width;
		c->geom.height = c->old_geom.height = gr->height;
		free(gr);
	} else
		warn("xcb_get_geometry failed for win %#x.", w);

	/* init members */
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
	c->ws = selws;

	/* get size hints */
	xcb_icccm_get_wm_normal_hints_reply(conn, xcb_icccm_get_wm_normal_hints(conn, c->win), &c->size_hints, NULL);

	/* get wm hints */
	xcb_icccm_get_wm_hints_reply(conn, xcb_icccm_get_wm_hints(conn, c->win), &c->wm_hints, NULL);

	/* get protocols */
	if (xcb_icccm_get_wm_protocols_reply(conn, xcb_icccm_get_wm_protocols(conn, c->win, WM_PROTOCOLS), &pr, NULL) == 1) {
		for (i = 0; i < pr.atoms_len; ++i) {
			if (pr.atoms[i] == WM_DELETE_WINDOW)
				c->can_delete = true;
			if (pr.atoms[i] == WM_TAKE_FOCUS)
				c->can_focus = true;
		}
		xcb_icccm_get_wm_protocols_reply_wipe(&pr);
	}
	applyrules(c);
	reparent(c);
	attach(c);
	attachstack(c);
	sel = c;
	fitclient(c);
	if (sloppy_focus)
		xcb_change_window_attributes(conn, w, XCB_CW_EVENT_MASK, (uint32_t[]){CLIENT_EVENT_MASK});
	if (c->ws == selws)
		xcb_map_window(conn, w);
	warp_pointer(c);
	ewmh_update_client_list(clients);
	focus(NULL);
}

void
reparent(Client *c) {
	int16_t x, y;
	uint16_t width, height;

	x = c->geom.x;
	y = c->geom.y;
	width = c->geom.width;
	height = c->geom.height;

	c->frame = xcb_generate_id(conn);
	xcb_create_window(conn, XCB_COPY_FROM_PARENT, c->frame, screen->root, x, y, width, height,
			  c->noborder ? 0 : border_width, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
			  XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK,
			  (uint32_t[]){ focuscol, true, FRAME_EVENT_MASK });
	xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
			     (uint32_t[]){ c->geom.width, c->geom.height });
	xcb_map_window(conn, c->frame);
#ifdef SHAPE
	if (shape_ext)
		roundcorners(c);
#endif
	PRINTF("reparenting win %#x to %#x\n", c->win, c->frame);
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
	if (ISMAXVERT(sel) || ISMAXHORZ(sel)) {
		maximizeclient(sel, false);
		return;
	}
	savegeometry(sel);
	uint32_t values[3];
	uint16_t tw = screen->width_in_pixels;
	uint16_t th = screen->height_in_pixels;
	if (!sel->noborder) {
		tw -= border_width * 2;
		th -= border_width * 2;
	}
	if (arg->i == MaxVertical) {
		sel->geom.y = 0;
		sel->geom.height = th;
		values[0] = sel->geom.y;
		values[1] = sel->geom.height;
		xcb_configure_window(conn, sel->frame, XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT, values);
		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_HEIGHT, values);
		change_ewmh_flags(sel, XCB_EWMH_WM_STATE_ADD, EWMH_MAXIMIZED_VERT);
	}
	else if (arg->i == MaxHorizontal) {
		sel->geom.x = 0;
		sel->geom.width = tw;
		values[0] = sel->geom.x;
		values[1] = sel->geom.width;
		xcb_configure_window(conn, sel->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH, values);
		xcb_configure_window(conn, sel->win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_WIDTH, values);
		change_ewmh_flags(sel, XCB_EWMH_WM_STATE_ADD, EWMH_MAXIMIZED_HORZ);
	}
	setborder(sel, true);
	warp_pointer(sel);
}

void
maximizeclient(Client *c, bool doit) {
	if (!c)
		return;
	if (ISFULLSCREEN(c) && doit)
		return;
	PRINTF("maximizeclient: %s to ", doit ? "maximizing" : "unmaximizing");

	if (doit) {
		savegeometry(c);
		change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, EWMH_FULLSCREEN);
		change_ewmh_flags(c, XCB_EWMH_WM_STATE_REMOVE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, XCB_EWMH_WM_STATE_REMOVE, EWMH_MAXIMIZED_HORZ);
		c->geom.x = 0;
		c->geom.y = 0;
		c->geom.width = screen->width_in_pixels;
		c->geom.height = screen->height_in_pixels;
		PRINTF("(%d,%d) %dx%d\n", c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		setborderwidth(c->frame, 0);
		moveresize_win(c->frame, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
		focus(NULL);
	}
	else {
		c->geom.x = ISMAXVERT(c) ? c->geom.x : MAX(0, c->old_geom.x);
		c->geom.y = ISMAXHORZ(c) ? c->geom.y : MAX(0, c->old_geom.y);
		c->geom.width = c->old_geom.width;
		c->geom.height = c->old_geom.height;
		change_ewmh_flags(c, XCB_EWMH_WM_STATE_REMOVE, EWMH_FULLSCREEN);
		change_ewmh_flags(c, XCB_EWMH_WM_STATE_REMOVE, EWMH_MAXIMIZED_VERT);
		change_ewmh_flags(c, XCB_EWMH_WM_STATE_REMOVE, EWMH_MAXIMIZED_HORZ);
		PRINTF("(%d,%d) %dx%d\n", c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		setborderwidth(c->frame, border_width);
		moveresize_win(c->frame, c->geom.x, c->geom.y, c->geom.width, c->geom.height);
		moveresize_win(c->win, 0, 0, c->geom.width, c->geom.height);
		/* setborder(c, true); */
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
	PRINTF("raise win %#x\n", win);
	const uint32_t values[] = { XCB_STACK_MODE_ABOVE };
	xcb_configure_window(conn, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
}

void
resize(const Arg *arg) {
	int iw = resize_step, ih = resize_step;
	if (!sel)
		return;

	if (sel->size_hints.width_inc > 7 && sel->size_hints.width_inc < screen->width_in_pixels)
		iw = sel->size_hints.width_inc;
	if (sel->size_hints.height_inc > 7 && sel->size_hints.height_inc < screen->height_in_pixels)
		ih = sel->size_hints.height_inc;

	if (arg->i == GrowHeight || arg->i == GrowBoth) {
		sel->geom.height = sel->geom.height + ih;
	}
	if (arg->i == GrowWidth || arg->i == GrowBoth) {
		sel->geom.width = sel->geom.width + iw;
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
		change_ewmh_flags(sel, XCB_EWMH_WM_STATE_REMOVE, EWMH_FULLSCREEN);
		setborderwidth(sel->frame, border_width);
	}
	change_ewmh_flags(sel, XCB_EWMH_WM_STATE_REMOVE, EWMH_MAXIMIZED_VERT);
	change_ewmh_flags(sel, XCB_EWMH_WM_STATE_REMOVE, EWMH_MAXIMIZED_HORZ);
	setborder(sel, true);
	warp_pointer(sel);
}

void
resizewin(xcb_window_t win, uint16_t w, uint16_t h) {
	const uint32_t values[] = { w, h };
	const uint32_t mask = XCB_CONFIG_WINDOW_WIDTH
		| XCB_CONFIG_WINDOW_HEIGHT;
	xcb_configure_window(conn, win, mask, values);
}

void
savegeometry(Client *c) {
	c->old_geom.x = c->geom.x;
	c->old_geom.y = c->geom.y;
	c->old_geom.height = c->geom.height;
	c->old_geom.width = c->geom.width;
}

void
send_client_message(Client *c, xcb_atom_t proto) {
	xcb_client_message_event_t ev;
#if DEBUG
	char *name = get_atom_name(proto);
	PRINTF("send_client_message: %s to win %#x\n", name, c->win);
	free(name);
#endif
	memset(&ev, '\0', sizeof ev);
	ev.response_type = XCB_CLIENT_MESSAGE;
	ev.format = 32;
	ev.sequence = 0;
	ev.window = c->win;
	ev.type = ewmh->WM_PROTOCOLS;
	ev.data.data32[0] = proto;
	ev.data.data32[1] = XCB_CURRENT_TIME;
	xcb_send_event(conn, false, c->win, XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
}

void
setborder(Client *c, bool focus) {
	if (ISFULLSCREEN(c) || c->noborder)
		return;
	xcb_change_window_attributes(conn, c->frame, XCB_CW_BORDER_PIXEL,
				     (uint32_t[]){ focus ? focuscol : unfocuscol });
	/* uint32_t values[1]; */
	/* int half = outer_border_width; */
	/* values[0] = border_width; */
	/* xcb_configure_window(conn, c->win, XCB_CONFIG_WINDOW_BORDER_WIDTH, values); */
	/* if (!double_border) { */
	/* 	values[0] = focus ? focuscol : unfocuscol; */
	/* 	xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, values); */
	/* 	return; */
	/* } */
	/* xcb_pixmap_t pmap = xcb_generate_id(conn); */
	/* xcb_gcontext_t gc = xcb_generate_id(conn); */
	/* xcb_create_pixmap(conn, screen->root_depth, pmap, screen->root, */
	/* 		  c->geom.width+border_width*2, c->geom.height+border_width*2); */
	/* xcb_create_gc(conn, gc, pmap, 0, NULL); */

	/* values[0] = outercol; */
	/* xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &values[0]); */
	/* xcb_rectangle_t rect_outer[] = { */
	/* 	{ c->geom.width+border_width-half, 0, half, c->geom.height+border_width*2 }, */
	/* 	{ c->geom.width+border_width, 0, half, c->geom.height+border_width*2 }, */
	/* 	{ 0, c->geom.height+border_width-half, c->geom.width+border_width*2, half }, */
	/* 	{ 0, c->geom.height+border_width, c->geom.width+border_width*2, half }, */
	/* 	{ 1, 1, 1, 1 } */
	/* }; */
	/* xcb_poly_fill_rectangle(conn, pmap, gc, 5, rect_outer); */

	/* values[0] = focus ? focuscol : unfocuscol; */
	/* xcb_change_gc(conn, gc, XCB_GC_FOREGROUND, &values[0]); */
	/* xcb_rectangle_t rect_inner[] = { */
	/* 	{ c->geom.width, 0, border_width-half, c->geom.height+border_width-half }, */
	/* 	{ c->geom.width+border_width+half, 0, border_width-half, c->geom.height+border_width-half }, */
	/* 	{ 0, c->geom.height, c->geom.width+border_width-half, border_width-half }, */
	/* 	{ 0, c->geom.height+border_width+half, c->geom.width+border_width-half, border_width-half }, */
	/* 	{ c->geom.width+border_width+half, border_width+c->geom.height+half, border_width, border_width } */
	/* }; */
	/* xcb_poly_fill_rectangle(conn, pmap, gc, 5, rect_inner); */

	/* values[0] = pmap; */
	/* xcb_change_window_attributes(conn,c->win, XCB_CW_BORDER_PIXMAP, &values[0]); */
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
		// TODO: set iconic state
		movewin(c->frame, WIDTH(c) * -2, c->geom.y);
	}
}

void
teleport(const Arg *arg) {
	uint16_t tw;
	uint16_t th;

	if (!sel || sel->frame == screen->root)
		return;

	tw = sel->geom.width;
	th = sel->geom.height;
	if (!sel->noborder) {
		tw += border_width * 2;
		th += border_width * 2;
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
	PRINTF("teleport win %#x to (%d,%d)\n", sel->frame, sel->geom.x, sel->geom.y);
	movewin(sel->frame, sel->geom.x, sel->geom.y);
	warp_pointer(sel);
}

void
unmanage(Client *c) {
	PRINTF("unmanage: win %#x\n", c->win);
	detach(c);
	detachstack(c);
	if (c->frame) {
		xcb_destroy_window(conn, c->frame);
	}
	free(c);
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

