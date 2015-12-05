/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_icccm.h>
#include "tfwm.h"
#include "list.h"
#include "events.h"

static void buttonpress(xcb_generic_event_t *ev);
static void circulaterequest(xcb_generic_event_t *ev);
static void clientmessage(xcb_generic_event_t *ev);
static void configurerequest(xcb_generic_event_t *ev);
static void destroynotify(xcb_generic_event_t *ev);
static void enternotify(xcb_generic_event_t *ev);
static void keypress(xcb_generic_event_t *ev);
static void mappingnotify(xcb_generic_event_t *ev);
static void propertynotify(xcb_generic_event_t *ev);
static void requesterror(xcb_generic_event_t *ev);
static void unmapnotify(xcb_generic_event_t *ev);
#ifdef DEBUG
static char *get_atom_name(xcb_atom_t atom);
#endif

void
handleevent(xcb_generic_event_t *ev) {
	switch (ev->response_type & ~0x80) {
		case XCB_BUTTON_PRESS:
			buttonpress(ev);
			break;
		case XCB_CIRCULATE_REQUEST:
			circulaterequest(ev);
			break;
		case XCB_CLIENT_MESSAGE:
			clientmessage(ev);
			break;
		case XCB_CONFIGURE_REQUEST:
			configurerequest(ev);
			break;
		case XCB_DESTROY_NOTIFY:
			destroynotify(ev);
			break;
		case XCB_ENTER_NOTIFY:
			enternotify(ev);
			break;
		case XCB_KEY_PRESS:
			keypress(ev);
			break;
		case XCB_MAPPING_NOTIFY:
			mappingnotify(ev);
			break;
		case XCB_MAP_REQUEST:
			maprequest(ev);
			break;
		case XCB_PROPERTY_NOTIFY:
			propertynotify(ev);
			break;
		case XCB_UNMAP_NOTIFY:
			unmapnotify(ev);
			break;
		case 0:
			requesterror(ev);
			break;
	}
}

void
buttonpress(xcb_generic_event_t *ev) {
	xcb_button_press_event_t *e = (xcb_button_press_event_t*)ev;
	Client *c;

	PRINTF("Event: button press: %#x\n", e->event);

	if ((c = wintoclient(e->event))) {
		if (c->win != sel->win) {
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
	PRINTF("Event: circulate request win %#x\n", e->window);
	xcb_circulate_window(conn, e->window, e->place);
}

void
clientmessage(xcb_generic_event_t *ev) {
	xcb_client_message_event_t *e = (xcb_client_message_event_t*)ev;
	Client *c;

#ifdef DEBUG
	char *name;
	name = get_atom_name(e->type);
	PRINTF("Event: client message win %#x, atom %s (%u)\n", e->window, name, e->type);
	free(name);
#endif

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
		focus(c);
	} else if (e->type == ewmh->_NET_WM_DESKTOP) {
	} else if (e->type == ewmh->_NET_MOVERESIZE_WINDOW) {
	} else if (e->type == ewmh->_NET_CLOSE_WINDOW) {
		unmanage(c);
	}
}

#ifdef DEBUG
char *
get_atom_name(xcb_atom_t atom) {
	char *name = NULL;
	xcb_get_atom_name_reply_t *reply;
	int len;

	reply = xcb_get_atom_name_reply(conn, xcb_get_atom_name(conn, atom), NULL);
	if (!reply)
		return NULL;

	len = xcb_get_atom_name_name_length(reply);
	if (len) {
		if (!(name = malloc(len + 1)))
			err("can't allocate memory.");
		memcpy(name, xcb_get_atom_name_name(reply), len);
		name[len] = '\0';
	}

	free(reply);

	return name;
}
#endif

void
configurerequest(xcb_generic_event_t *ev) {
	xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
	Client *c;
	unsigned int v[7];
	int i = 0;

#ifdef DEBUG
	PRINTF("Event: configure request: win %#x: ", e->window);
	if (e->value_mask & XCB_CONFIG_WINDOW_X)
		PRINTF("x: %d ", e->x);
	if (e->value_mask & XCB_CONFIG_WINDOW_Y)
		PRINTF("y: %d ", e->y);
	if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH)
		PRINTF("w: %u ", e->width);
	if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
		PRINTF("h: %u ", e->height);
	if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH)
		PRINTF("border: %u ", e->border_width);
	if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING)
		PRINTF("sibling: %#x ", e->sibling);
	if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE)
		PRINTF("stack mode: %u", e->stack_mode);
	PRINTF("\n");
#endif

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
		if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
			v[i++] = e->border_width;
			/* setborder(c, true); */
		}

		xcb_configure_notify_event_t evt;
		memset(&evt, '\0', sizeof evt);

		evt.response_type = XCB_CONFIGURE_NOTIFY;
		evt.event = c->win;
		evt.window = c->win;
		evt.above_sibling = XCB_NONE;
		evt.x = c->geom.x;
		evt.y = c->geom.y;
		evt.width = c->geom.width;
		evt.height = c->geom.height;
		evt.border_width = c->noborder ? 0 : BORDER_WIDTH;
		evt.override_redirect = false;

		xcb_send_event(conn, false, c->win, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char *)&evt);

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

	PRINTF("Event: destroy notify: win %#x\n", e->window);

	if ((c = wintoclient(e->window)))
		unmanage(c);
}

void
enternotify(xcb_generic_event_t *ev) {
	xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
	Client *c;

	PRINTF("Event: enter notify: win %#x\n", e->event);

	if (e->mode == XCB_NOTIFY_MODE_NORMAL || e->mode == XCB_NOTIFY_MODE_UNGRAB) {
		if (sel && e->event == sel->win)
			return;
		if ((c = wintoclient(e->event))) {
			raisewindow(c->win);
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

	PRINTF("Event: map request win %#x\n", e->window);

	if (!wintoclient(e->window))
		manage(e->window);
}

void
propertynotify(xcb_generic_event_t *ev) {
	xcb_property_notify_event_t *e = (xcb_property_notify_event_t *)ev;
	Client *c;

#ifdef DEBUG
	char *name;
	name = get_atom_name(e->atom);
	PRINTF("Event: property notify: win %#x atom %s", e->window, name);
	free(name);
#endif

	if (!(c = wintoclient(e->window)))
		return;

	if (e->atom == XCB_ATOM_WM_HINTS) { /* update urgency */
		xcb_icccm_wm_hints_t hints;
		if (xcb_icccm_get_wm_hints_reply(conn, xcb_icccm_get_wm_hints(conn, e->window), &hints, NULL) == 1 &&
			hints.flags & XCB_ICCCM_WM_HINT_X_URGENCY) {
			if ((c->isurgent = xcb_icccm_wm_hints_get_urgency(&hints)) &&
				c != sel) {
				/* set red border */
				const uint32_t value[] = { 0xff0000 } ;
				xcb_change_window_attributes(conn, c->win, XCB_CW_BORDER_PIXEL, value);
			}
		}
	}
	else if (e->atom == XCB_ATOM_WM_NORMAL_HINTS) { /* update size hints */
		xcb_size_hints_t hints;
		if (xcb_icccm_get_wm_normal_hints_reply(conn, xcb_icccm_get_wm_normal_hints(conn, e->window), &hints, NULL) == 1 &&
			hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
			c->minw = hints.min_width;
			c->minh = hints.min_height;
		}
	}
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

	PRINTF("Event: unmap notify: %#x\n", e->window);

	Client *c;
	if (e->window == screen->root)
		return;
	if ((c = wintoclient(e->window)))
		unmanage(c);
}

