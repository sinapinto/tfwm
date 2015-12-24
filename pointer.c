/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>
#include "tfwm.h"
#include "events.h"
#include "pointer.h"
#include "ewmh.h"
#include "config.h"

cursor_t cursors[XC_MAX] = {
	{"left_ptr",            XC_left_ptr,            XCB_CURSOR_NONE},
	{"fleur",               XC_fleur,               XCB_CURSOR_NONE},
	{"bottom_right_corner", XC_bottom_right_corner, XCB_CURSOR_NONE}
};

void
load_cursors(void) {
	unsigned int i;
	xcb_font_t   font = XCB_NONE;

	for (i = 0; i < LENGTH(cursors); ++i) {
		cursors[i].cid = XcursorLibraryLoadCursor(display,
							  cursors[i].name);

		if (cursors[i].cid == XCB_CURSOR_NONE) {
			warn("couldn't load xcursor %s, "
			     "falling back to cursorfont\n", cursors[i].name);

			if (font == XCB_NONE) {
				font = xcb_generate_id(conn);
				xcb_open_font_checked(conn, font,
						      strlen("cursor"), "cursor");
			}

			cursors[i].cid = xcb_generate_id(conn);
			xcb_create_glyph_cursor(conn, cursors[i].cid, font,
						font, cursors[i].cf_glyph,
						cursors[i].cf_glyph + 1,
						0, 0, 0,
						0xffff, 0xffff, 0xffff);
		}
	}

	if (font != XCB_NONE)
		xcb_close_font(conn, font);
}

void
free_cursors(void) {
	unsigned int i;

	for (i = 0; i < LENGTH(cursors); ++i)
		xcb_free_cursor(conn, cursors[i].cid);
	PRINTF("freeing %d cursors\n", i);
}

void
mousemotion(const Arg *arg) {
	xcb_cursor_t                cursor = XCB_NONE;
	xcb_time_t                  lasttime = 0;
	xcb_generic_event_t        *ev;
	xcb_motion_notify_event_t  *e;
	bool                        ungrab = false;
	int                         nx, ny, nw, nh;
	xcb_query_pointer_cookie_t  qpc;
	xcb_query_pointer_reply_t  *qpr;
	xcb_grab_pointer_reply_t   *gpr;
	xcb_grab_pointer_cookie_t   gpc;

	if (!sel || sel->win == screen->root)
		return;

	raisewindow(sel->frame);
	raisewindow(sel->win);
	qpc = xcb_query_pointer(conn, screen->root);
	qpr = xcb_query_pointer_reply(conn, qpc, 0);

	if (arg->i == MouseMove)
		cursor = cursors[XC_FLEUR].cid;
	else
		cursor = cursors[XC_BOTTOM_RIGHT_CORNER].cid;

	/* grab pointer */
	gpc = xcb_grab_pointer(conn, 0, screen->root, POINTER_EVENT_MASK,
			       XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
			       XCB_NONE, cursor, XCB_CURRENT_TIME);
	gpr = xcb_grab_pointer_reply(conn, gpc, NULL);
	if (gpr->status != XCB_GRAB_STATUS_SUCCESS) {
		free(gpr);
		return;
	}
	free(gpr);

	nx = sel->geom.x;
	ny = sel->geom.y;
	nw = sel->geom.width;
	nh = sel->geom.height;
	while ((ev = xcb_wait_for_event(conn)) && !ungrab) {
		switch (ev->response_type & ~0x80) {
		case XCB_CONFIGURE_REQUEST:
		case XCB_MAP_REQUEST:
			maprequest(ev);
			break;
		case XCB_MOTION_NOTIFY:
			e = (xcb_motion_notify_event_t*)ev;
			if ((e->time - lasttime) <= (1000 / 60))
				continue;
			lasttime = e->time;

			if (arg->i == MouseMove) {
				nx = sel->geom.x + e->root_x - qpr->root_x;
				ny = sel->geom.y + e->root_y - qpr->root_y;
				movewin(sel->frame, nx, ny);
			} else {
				nw = MAX(sel->geom.width + e->root_x - qpr->root_x,
					 sel->size_hints.min_width + 40);
				nh = MAX(sel->geom.height + e->root_y - qpr->root_y,
					 sel->size_hints.min_height + 40);
				resizewin(sel->win, nw, nh);
				resizewin(sel->frame, nw, nh);
			}
			break;
		case XCB_BUTTON_RELEASE:
			if (arg->i == MouseMove) {
				sel->geom.x = nx;
				sel->geom.y = ny;
			} else {
				sel->geom.width = nw;
				sel->geom.height = nh;
			}
			ungrab = true;
			setborder(sel, true);
		}
		free(ev);
		xcb_flush(conn);
	}
	change_ewmh_flags(sel, REMOVE_STATE, EWMH_FULLSCREEN);
	free(ev);
	free(qpr);
	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
}

void
warp_pointer(Client *c) {
	int16_t x = 0;
	int16_t y = 0;

	switch(cursor_position) {
	case 0: return;
	case 1: break;
	case 2: x += c->geom.width; break;
	case 3: y += c->geom.height; break;
	case 4:
		x += c->geom.width;
		y += c->geom.height;
		break;
	case 5:
		x = c->geom.width / 2;
		y = c->geom.height / 2;
		break;
	default:
		warn("warp_pointer: bad setting: %d\n", cursor_position);
	}

	xcb_warp_pointer(conn, XCB_NONE, c->win, 0, 0, 0, 0, x, y);
}

