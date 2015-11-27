#include <string.h>
#include <xcb/xcb_event.h>
#include "tfwm.h"
#include "client.h"
#include "list.h"

void
buttonpress(xcb_generic_event_t *ev) {
	xcb_button_press_event_t *e = (xcb_button_press_event_t*)ev;
	Client *c;
	if ((c = wintoclient(e->event))) {
		if (c->win != sel->win) {
			setborder(sel, false);
			focus(c);
		}
	}
	for (unsigned int i = 0; i < LENGTH(buttons); i++)
		if (buttons[i].button == e->detail &&
				CLEANMASK(buttons[i].mask) == CLEANMASK(e->state) &&
				buttons[i].func)
			if (sel != NULL && e->event != screen->root)
				buttons[i].func(&buttons[i].arg);
}

void
circulaterequest(xcb_generic_event_t *ev) {
	xcb_circulate_request_event_t *e = (xcb_circulate_request_event_t *)ev;
	PRINTF("Event: circulate request: %X\n", e->window);
	xcb_circulate_window(conn, e->window, e->place);
}

void
clientmessage(xcb_generic_event_t *ev) {
	xcb_client_message_event_t *e = (xcb_client_message_event_t*)ev;
	Client *c;

	PRINTF("Event: client message %u: %X\n", e->type, e->window);

	if (!(c = wintoclient(e->window)))
		return;

	if (e->type == ewmh->_NET_WM_STATE) {
		if (e->data.data32[1] == ewmh->_NET_WM_STATE_FULLSCREEN ||
				e->data.data32[2] == ewmh->_NET_WM_STATE_FULLSCREEN) {
			if (e->data.data32[0] == XCB_EWMH_WM_STATE_ADD)
				maximizeclient(c, true);
			else if (e->data.data32[0] == XCB_EWMH_WM_STATE_REMOVE)
				maximizeclient(c, false);
			else if (e->data.data32[0] == XCB_EWMH_WM_STATE_TOGGLE)
				maximizeclient(c, !c->ismax);
		}
	} else if (e->type == ewmh->_NET_ACTIVE_WINDOW) {
		setborder(sel, false);
		focus(c);
	} else if (e->type == ewmh->_NET_CLOSE_WINDOW) {
		unmanage(c);
	}
}

void
configurerequest(xcb_generic_event_t *ev) {
	xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
	Client *c;
	unsigned int v[7];
	int i = 0;

	PRINTF("Event: configure request x %d y %d w %d h %d bw %d: %X\n",
			e->x, e->y, e->width, e->height, e->border_width, e->window);
	if ((c = wintoclient(e->window))) {
		if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
			if (!c->ismax && !c->ishormax)
				v[i++] = c->geom.width = MIN(e->width, screen->width_in_pixels-2*BORDER_WIDTH);
		if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
			if (!c->ismax && !c->isvertmax)
				v[i++] = c->geom.height = MIN(e->height, screen->height_in_pixels-2*BORDER_WIDTH);
		if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
			v[i++] = e->sibling;
		if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
			v[i++] = e->stack_mode;
		if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
			v[i++] = e->border_width;
		setborder(c, true);
		if (!c->ismax && !c->isvertmax && !c->ishormax)
			xcb_configure_window(conn, e->window, e->value_mask, v);
	}
	else {
		if (e->value_mask & XCB_CONFIG_WINDOW_X)
			v[i++] = e->x;
		if (e->value_mask & XCB_CONFIG_WINDOW_Y)
			v[i++] = e->y;
		if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
			v[i++] = e->width;
		if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
			v[i++] = e->height;
		if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
			v[i++] = e->sibling;
		if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
			v[i++] = e->stack_mode;
		if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
			v[i++] = e->border_width;
		xcb_configure_window(conn, e->window, e->value_mask, v);
	}
}

void
destroynotify(xcb_generic_event_t *ev) {
	xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
	Client *c;

	if ((c = wintoclient(e->window))) {
		PRINTF("Event: destroy notify: %X\n", e->window);
		unmanage(c);
	}
}

void
enternotify(xcb_generic_event_t *ev) {
	xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
	Client *c;
	PRINTF("Event: enter notify: %d\n", e->event);
	if (e->mode == XCB_NOTIFY_MODE_NORMAL || e->mode == XCB_NOTIFY_MODE_UNGRAB) {
		if (sel != NULL && e->event == sel->win)
			return;
		if ((c = wintoclient(e->event))) {
			focus(c);
		}
	}
}

void
keypress(xcb_generic_event_t *ev) {
	xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;
	xcb_keysym_t keysym = getkeysym(e->detail);
	for (unsigned int i = 0; i < LENGTH(keys); i++) {
		if (keysym == keys[i].keysym &&
				CLEANMASK(keys[i].mod) == CLEANMASK(e->state) &&
				keys[i].func) {
			keys[i].func(&keys[i].arg);
			break;
		}
	}
}

void
mappingnotify(xcb_generic_event_t *ev) {
	xcb_mapping_notify_event_t *e = (xcb_mapping_notify_event_t *)ev;
	PRINTF("Event: mapping notify\n");
	if (e->request != XCB_MAPPING_MODIFIER && e->request != XCB_MAPPING_KEYBOARD)
		return;
	xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
	grabkeys();
}

void
maprequest(xcb_generic_event_t *ev) {
	xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;
	PRINTF("Event: map request: %X\n", e->window);
	if (sel && sel->win != e->window)
		setborder(sel, false);
	if (!wintoclient(e->window))
		manage(e->window);
}

void
mousemotion(const Arg *arg) {
	if (!sel || sel->win == screen->root)
		return;
	xcb_time_t lasttime = 0;
	raisewindow(sel->win);
	xcb_query_pointer_reply_t *pointer;
	pointer = xcb_query_pointer_reply(conn, xcb_query_pointer(conn, screen->root), 0);
	/* create cursor */
	xcb_font_t font = xcb_generate_id(conn);
	xcb_void_cookie_t fontcookie =
		xcb_open_font_checked(conn, font, strlen("cursor"), "cursor");
	testcookie(fontcookie, "can't open font.");
	xcb_cursor_t cursor = xcb_generate_id(conn);
	int cursorid;
	if (arg->i == MouseMove)
		cursorid = 52;
	else
		cursorid = 14;
	xcb_create_glyph_cursor(conn, cursor, font, font,
			cursorid, cursorid + 1, 0x0000, 0x0000, 0x0000, 0xffff, 0xffff, 0xffff);
	/* grab pointer */
	xcb_grab_pointer_reply_t *grab_reply = xcb_grab_pointer_reply(conn,
			xcb_grab_pointer(conn, 0, screen->root,
				XCB_EVENT_MASK_BUTTON_PRESS |
				XCB_EVENT_MASK_BUTTON_RELEASE |
				XCB_EVENT_MASK_BUTTON_MOTION |
				XCB_EVENT_MASK_POINTER_MOTION, XCB_GRAB_MODE_ASYNC,
				XCB_GRAB_MODE_ASYNC, XCB_NONE, cursor, XCB_CURRENT_TIME), NULL);
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
				handler[ev->response_type & ~0x80](ev);
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
	fontcookie = xcb_close_font_checked(conn, font);
	testcookie(fontcookie, "can't close font.");
	xcb_free_cursor(conn, cursor);
	xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
}

void
requesterror(xcb_generic_event_t *ev) {
	xcb_request_error_t *e = (xcb_request_error_t *)ev;
	warn("Event: failed request: %s, %s: %d\n",
			xcb_event_get_request_label(e->major_opcode),
			xcb_event_get_error_label(e->error_code),
			e->bad_value);
}

void
unmapnotify(xcb_generic_event_t *ev) {
	xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
	PRINTF("Event: unmap notify: %X\n", e->window);
	Client *c;
	if (e->window == screen->root)
		return;
	if ((c = wintoclient(e->window)))
		unmanage(c);
}

