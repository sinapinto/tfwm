/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_icccm.h>
#include "main.h"
#include "list.h"
#include "events.h"
#include "keys.h"
#include "xcb.h"
#include "ewmh.h"
#include "config.h"
#include "client.h"
#include "cursor.h"
#include "util.h"

static void buttonpress(xcb_generic_event_t *ev) {
    xcb_button_press_event_t *e = (xcb_button_press_event_t *)ev;
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

static void clientmessage(xcb_generic_event_t *ev) {
    xcb_client_message_event_t *e = (xcb_client_message_event_t *)ev;
    Client *c;

#ifdef DEBUG
    char *name = get_atom_name(e->type);
    PRINTF("Event: client message: %s win %#x\n", name, e->window);
    FREE(name);
#endif
    if (!(c = wintoclient(e->window)))
        return;

    if (e->type == ewmh->_NET_WM_STATE) {
        handle_wm_state(c, e->data.data32[1], e->data.data32[0]);
        handle_wm_state(c, e->data.data32[2], e->data.data32[0]);
    } else if (e->type == ewmh->_NET_ACTIVE_WINDOW) {
        if (c->can_focus)
            focus(c);
    } else if (e->type == ewmh->_NET_WM_DESKTOP) {
    } else if (e->type == ewmh->_NET_MOVERESIZE_WINDOW) {
    } else if (e->type == ewmh->_NET_REQUEST_FRAME_EXTENTS) {
        xcb_ewmh_set_frame_extents(ewmh, c->win, border_width, border_width,
                                   border_width, border_width);
    } else if (e->type == ewmh->_NET_CLOSE_WINDOW) {
        if (c->can_delete)
            send_client_message(c, WM_DELETE_WINDOW);
    }
}

static void configurerequest(xcb_generic_event_t *ev) {
    xcb_configure_request_event_t *e = (xcb_configure_request_event_t *)ev;
    Client *c;
    uint32_t v[7];
    int i = 0;
    uint16_t mask = 0;

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
        if (e->value_mask & XCB_CONFIG_WINDOW_X) {
            mask |= XCB_CONFIG_WINDOW_X;
            v[i++] = e->x;
            c->geom.x = e->x;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_Y) {
            mask |= XCB_CONFIG_WINDOW_Y;
            v[i++] = e->y;
            c->geom.y = e->y;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            mask |= XCB_CONFIG_WINDOW_WIDTH;
            v[i++] = e->width;
            c->geom.width = e->width;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            mask |= XCB_CONFIG_WINDOW_HEIGHT;
            v[i++] = e->height;
            c->geom.height = e->height;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
            mask |= XCB_CONFIG_WINDOW_SIBLING;
            v[i++] = e->sibling;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            mask |= XCB_CONFIG_WINDOW_STACK_MODE;
            v[i++] = e->stack_mode;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
            /* mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH; */
            /* v[i++] = e->border_width; */
            setborder(c, true);
        }

        if (i > 0) {
            xcb_configure_window(conn, c->win, mask, v);
            xcb_configure_window(conn, c->frame, mask, v);
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
        evt.border_width = 0;
        evt.override_redirect = 0;
        xcb_send_event(conn, false, c->win, XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                       (const char *)&evt);
    } else {
        if (e->value_mask & XCB_CONFIG_WINDOW_X) {
            mask |= XCB_CONFIG_WINDOW_X;
            v[i++] = e->x;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_Y) {
            mask |= XCB_CONFIG_WINDOW_Y;
            v[i++] = e->y;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            mask |= XCB_CONFIG_WINDOW_WIDTH;
            v[i++] = e->width;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            mask |= XCB_CONFIG_WINDOW_HEIGHT;
            v[i++] = e->height;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_SIBLING) {
            mask |= XCB_CONFIG_WINDOW_SIBLING;
            v[i++] = e->sibling;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_STACK_MODE) {
            mask |= XCB_CONFIG_WINDOW_STACK_MODE;
            v[i++] = e->stack_mode;
        }
        if (e->value_mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) {
            mask |= XCB_CONFIG_WINDOW_BORDER_WIDTH;
            v[i++] = e->border_width;
        }

        if (i > 0)
            xcb_configure_window(conn, e->window, mask, v);
    }
}

static void enternotify(xcb_generic_event_t *ev) {
    xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)ev;
    Client *c;

    PRINTF("Event: enter notify: win %#x\n", e->event);

    if (e->mode == XCB_NOTIFY_MODE_NORMAL ||
        e->mode == XCB_NOTIFY_MODE_UNGRAB) {
        if (sel && e->event == sel->win)
            return;
        if ((c = wintoclient(e->event))) {
            raisewindow(c->frame);
            raisewindow(c->win);
            focus(c);
        }
    }
}

static void gravitynotify(xcb_generic_event_t *ev) {
    xcb_gravity_notify_event_t *e = (xcb_gravity_notify_event_t *)ev;

    PRINTF("Event: gravity notify: win %#x\n", e->event);
    (void)e;
}

static void keypress(xcb_generic_event_t *ev) {
    xcb_key_press_event_t *e = (xcb_key_press_event_t *)ev;

    xcb_keysym_t keysym = getkeysym(e->detail);

    for (int i = 0; i < LENGTH(keys); i++) {
        if (keysym == keys[i].keysym &&
            CLEANMASK(keys[i].mod) == CLEANMASK(e->state) && keys[i].func) {
            keys[i].func(&keys[i].arg);
            break;
        }
    }
}

static void mappingnotify(xcb_generic_event_t *ev) {
    xcb_mapping_notify_event_t *e = (xcb_mapping_notify_event_t *)ev;

    PRINTF("Event: mapping notify\n");

    if (e->request != XCB_MAPPING_MODIFIER &&
        e->request != XCB_MAPPING_KEYBOARD)
        return;
    xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
    grabkeys();
}

static void maprequest(xcb_generic_event_t *ev) {
    xcb_map_request_event_t *e = (xcb_map_request_event_t *)ev;

    PRINTF("Event: map request win %#x\n", e->window);

    if (!wintoclient(e->window))
        manage(e->window);
}

static void propertynotify(xcb_generic_event_t *ev) {
    xcb_property_notify_event_t *e = (xcb_property_notify_event_t *)ev;
    Client *c;

#ifdef DEBUG
    char *name = get_atom_name(e->atom);
    PRINTF("Event: property notify: win %#x atom %s", e->window, name);
    FREE(name);
#endif

    if (!(c = wintoclient(e->window)))
        return;

    if (e->atom == XCB_ATOM_WM_NORMAL_HINTS) {
        /* update size hints */
        xcb_get_property_cookie_t nhc =
            xcb_icccm_get_wm_normal_hints(conn, e->window);
        xcb_size_hints_t sh;
        uint8_t nhr = xcb_icccm_get_wm_normal_hints_reply(conn, nhc, &sh, NULL);

        if (nhr == 1 && sh.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
            c->size_hints.min_width = sh.min_width;
            c->size_hints.min_height = sh.min_height;
        }
    }
}

static void requesterror(xcb_generic_event_t *ev) {
    xcb_request_error_t *e = (xcb_request_error_t *)ev;
    warn("Event: FAILED REQUEST: %s, %s: %d\n",
         xcb_event_get_request_label(e->major_opcode),
         xcb_event_get_error_label(e->error_code), e->bad_value);
}

static void unmapnotify(xcb_generic_event_t *ev) {
    xcb_unmap_notify_event_t *e = (xcb_unmap_notify_event_t *)ev;
    Client *c;

    PRINTF("Event: unmap notify: %#x ", e->window);

    if ((c = wintoclient(e->window))) {
        PRINTF("\n");
        unmanage(c);
    } else {
        PRINTF("(not found)\n");
    }
}

static void destroynotify(xcb_generic_event_t *ev) {
    xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)ev;
    Client *c;

    PRINTF("Event: destroy notify: win %#x ", e->window);

    if ((c = wintoclient(e->window))) {
        PRINTF("\n");
        unmanage(c);
    } else {
        PRINTF("(not found)\n");
    }
}

void mousemotion(const Arg *arg) {
    if (!sel || sel->win == screen->root)
        return;

    raisewindow(sel->frame);
    raisewindow(sel->win);
    xcb_query_pointer_reply_t *qpr =
        xcb_query_pointer_reply(conn, xcb_query_pointer(conn, screen->root), 0);

    xcb_cursor_t cursor = XCB_NONE;
    if (arg->i == MouseMove) {
        cursor = cursor_get_id(XC_MOVE);
    } else {
        cursor = cursor_get_id(XC_BOTTOM_RIGHT);
    }

    /* grab pointer */
    xcb_grab_pointer_cookie_t gpc = xcb_grab_pointer(
        conn, 0, screen->root, POINTER_EVENT_MASK, XCB_GRAB_MODE_ASYNC,
        XCB_GRAB_MODE_ASYNC, XCB_NONE, cursor, XCB_CURRENT_TIME);
    xcb_grab_pointer_reply_t *gpr = xcb_grab_pointer_reply(conn, gpc, NULL);
    if (gpr->status != XCB_GRAB_STATUS_SUCCESS) {
        FREE(gpr);
        return;
    }
    FREE(gpr);

    int nx = sel->geom.x;
    int ny = sel->geom.y;
    int nw = sel->geom.width;
    int nh = sel->geom.height;
    xcb_time_t lasttime = 0;
    xcb_generic_event_t *ev;
    xcb_motion_notify_event_t *e;
    bool ungrab = false;
    while ((ev = xcb_wait_for_event(conn)) && !ungrab) {
        switch (ev->response_type & ~0x80) {
        case XCB_CONFIGURE_REQUEST:
        case XCB_MAP_REQUEST:
            maprequest(ev);
            break;
        case XCB_MOTION_NOTIFY:
            e = (xcb_motion_notify_event_t *)ev;
            /* don't update more than 120 times/sec */
            if ((e->time - lasttime) <= (1000 / 120))
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
        FREE(ev);
        xcb_flush(conn);
    }
    change_ewmh_flags(sel, XCB_EWMH_WM_STATE_REMOVE, EWMH_FULLSCREEN);
    ewmh_update_wm_state(sel);
    FREE(ev);
    FREE(qpr);
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
}

void handleevent(xcb_generic_event_t *ev) {
    switch (ev->response_type & ~0x80) {
    case XCB_BUTTON_PRESS:
        buttonpress(ev);
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
    case XCB_GRAVITY_NOTIFY:
        gravitynotify(ev);
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

