/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>
#include "tfwm.h"
#include "events.h"
#include "pointer.h"

void
load_cursors(void) {
	xcb_font_t font = XCB_NONE;
	unsigned int i;

	for (i = 0; i < LENGTH(cursors); ++i) {
		cursors[i].cid = XcursorLibraryLoadCursor(display, cursors[i].name);

		if (cursors[i].cid == XCB_CURSOR_NONE) {
			warn("couldn't load xcursor %s, falling back to cursorfont\n", cursors[i].name);
			if (font == XCB_NONE) {
				font = xcb_generate_id(conn);
				xcb_void_cookie_t fontcookie = xcb_open_font_checked(conn, font, strlen("cursor"), "cursor");
				testcookie(fontcookie, "can't open font.");
			}

			cursors[i].cid = xcb_generate_id(conn);
			xcb_create_glyph_cursor(conn, cursors[i].cid, font, font, cursors[i].cf_glyph, cursors[i].cf_glyph + 1, 0, 0, 0, 0xffff, 0xffff, 0xffff);
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
}

void
mousemotion(const Arg *arg) {
	xcb_cursor_t cursor = XCB_NONE;

	if (!sel || sel->win == screen->root)
		return;
	xcb_time_t lasttime = 0;
	raisewindow(sel->win);
	xcb_query_pointer_reply_t *pointer;
	pointer = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, screen->root), 0);

	if (arg->i == MouseMove)
		cursor = cursors[XC_FLEUR].cid;
	else
		cursor = cursors[XC_BOTTOM_RIGHT_CORNER].cid;
	/* grab pointer */
	xcb_grab_pointer_reply_t *grab_reply = xcb_grab_pointer_reply(conn, xcb_grab_pointer(conn, 0, screen->root, POINTER_EVENT_MASK, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, cursor, XCB_CURRENT_TIME), NULL);
	if (grab_reply->status != XCB_GRAB_STATUS_SUCCESS) {
		free(grab_reply);
		return;
	}
	free(grab_reply);

	xcb_generic_event_t *ev;
	xcb_motion_notify_event_t *e;
	bool ungrab = false;
	int nx = sel->geom.x;
	int ny = sel->geom.y;
	int nw = sel->geom.width;
	int nh = sel->geom.height;
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
					nx = sel->geom.x + e->root_x - pointer->root_x;
					ny = sel->geom.y + e->root_y - pointer->root_y;
					movewin(sel->win, nx, ny);
				}
				else {
					nw = MAX(sel->geom.width + e->root_x - pointer->root_x, sel->minw + 40);
					nh = MAX(sel->geom.height + e->root_y - pointer->root_y, sel->minh + 40);
					resizewin(sel->win, nw, nh);
				}
				break;
			case XCB_BUTTON_RELEASE:
				if (arg->i == MouseMove) {
					sel->geom.x = nx;
					sel->geom.y = ny;
				}
				else {
					sel->geom.width = nw;
					sel->geom.height = nh;
				}
				ungrab = true;
				setborder(sel, true);
		}
		free(ev);
		xcb_flush(conn);
	}
	sel->ismax = false;
	free(ev);
	free(pointer);
	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
}

