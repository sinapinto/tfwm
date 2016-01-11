/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <unistd.h>
#include <xcb/xcb_ewmh.h>
#include "main.h"
#include "ewmh.h"
#include "config.h"
#include "client.h"
#include "xcb.h"
#include "log.h"

void ewmh_setup() {
    ewmh = malloc(sizeof(xcb_ewmh_connection_t));
    if ((xcb_ewmh_init_atoms_replies(ewmh, xcb_ewmh_init_atoms(conn, ewmh),
                                     NULL)) == 0)
        err("can't initialize ewmh.");

    xcb_atom_t net_atoms[] = {
        ewmh->_NET_WM_NAME,
        ewmh->_NET_WM_PID,
        ewmh->_NET_SUPPORTED,
        ewmh->_NET_SUPPORTING_WM_CHECK,
        ewmh->_NET_NUMBER_OF_DESKTOPS,
        ewmh->_NET_CURRENT_DESKTOP,
        ewmh->_NET_WM_DESKTOP,
        ewmh->_NET_ACTIVE_WINDOW,
        ewmh->_NET_CLOSE_WINDOW,
        ewmh->_NET_CLIENT_LIST,
        ewmh->_NET_CLIENT_LIST_STACKING,
        ewmh->_NET_MOVERESIZE_WINDOW,
        ewmh->_NET_REQUEST_FRAME_EXTENTS,
        ewmh->_NET_FRAME_EXTENTS,
        ewmh->_NET_WM_STATE,
        ewmh->_NET_WM_STATE_MODAL,
        ewmh->_NET_WM_STATE_STICKY,
        ewmh->_NET_WM_STATE_MAXIMIZED_VERT,
        ewmh->_NET_WM_STATE_MAXIMIZED_HORZ,
        /* ewmh->_NET_WM_STATE_HIDDEN, */
        ewmh->_NET_WM_STATE_FULLSCREEN,
        ewmh->_NET_WM_STATE_ABOVE,
        /* ewmh->_NET_WM_STATE_BELOW, */
        ewmh->_NET_WM_STATE_DEMANDS_ATTENTION,
        ewmh->_NET_WM_WINDOW_TYPE,
        ewmh->_NET_WM_WINDOW_TYPE_DESKTOP,
        ewmh->_NET_WM_WINDOW_TYPE_DOCK,
        ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR,
        ewmh->_NET_WM_WINDOW_TYPE_MENU,
        ewmh->_NET_WM_WINDOW_TYPE_UTILITY,
        ewmh->_NET_WM_WINDOW_TYPE_SPLASH,
        ewmh->_NET_WM_WINDOW_TYPE_DIALOG,
        ewmh->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
        ewmh->_NET_WM_WINDOW_TYPE_POPUP_MENU,
        ewmh->_NET_WM_WINDOW_TYPE_TOOLTIP,
        ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION,
        ewmh->_NET_WM_WINDOW_TYPE_COMBO,
        ewmh->_NET_WM_WINDOW_TYPE_DND,
        ewmh->_NET_WM_WINDOW_TYPE_NORMAL,
    };
    xcb_ewmh_set_supported(ewmh, scrno, LENGTH(net_atoms), net_atoms);

    xcb_window_t recorder = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, recorder, screen->root, 0, 0,
                      screen->width_in_pixels, screen->height_in_pixels, 0,
                      XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT,
                      XCB_NONE, NULL);

    /* _NET_WM_PID */
    xcb_ewmh_set_wm_pid(ewmh, recorder, getpid());

    /* _NET_WM_NAME */
    if (java_workaround)
        xcb_ewmh_set_wm_name(ewmh, screen->root, strlen("LG3D"), "LG3D");
    else
        xcb_ewmh_set_wm_name(ewmh, screen->root, strlen(__WM_NAME__),
                             __WM_NAME__);

    /* _NET_NUMBER_ODESKTOPS */
    xcb_ewmh_set_number_of_desktops(ewmh, scrno, 10);
    xcb_ewmh_set_current_desktop(ewmh, scrno, 0);

    /* _NET_SUPPORTING_WM_CHECK */
    xcb_ewmh_set_supporting_wm_check(ewmh, recorder, recorder);
    xcb_ewmh_set_supporting_wm_check(ewmh, screen->root, recorder);
}

void ewmh_teardown() {
    /* delete _NET_SUPPORTING_WM_CHECK */
    xcb_get_property_cookie_t cookie =
        xcb_get_property(conn, 0, screen->root, ewmh->_NET_SUPPORTING_WM_CHECK,
                         XCB_ATOM_WINDOW, 0, 1);
    xcb_get_property_reply_t *pr = xcb_get_property_reply(conn, cookie, NULL);
    if (pr) {
        if (pr->format == ewmh->_NET_SUPPORTING_WM_CHECK) {
            xcb_window_t id = *((xcb_window_t *)xcb_get_property_value(pr));
            PRINTF("deleting supporting wm check window %#x\n", id);
            xcb_destroy_window(conn, id);
            xcb_delete_property(conn, screen->root,
                                ewmh->_NET_SUPPORTING_WM_CHECK);
            xcb_delete_property(conn, screen->root, ewmh->_NET_SUPPORTED);
        }
        FREE(pr);
    }

    xcb_ewmh_connection_wipe(ewmh);
    FREE(ewmh);
}

void change_ewmh_flags(Client *c, xcb_ewmh_wm_state_action_t op,
                       uint32_t mask) {
    PRINTF("EWMH: change_ewmh_flags: win %#x, mask: %d, action: %d\n", c->win,
           mask, op);

    switch (op) {
    case XCB_EWMH_WM_STATE_ADD:
        c->ewmh_flags |= mask;
        break;
    case XCB_EWMH_WM_STATE_REMOVE:
        c->ewmh_flags &= ~mask;
        break;
    case XCB_EWMH_WM_STATE_TOGGLE:
        c->ewmh_flags ^= mask;
        break;
    }
}

void handle_wm_state(Client *c, xcb_atom_t state,
                     xcb_ewmh_wm_state_action_t action) {
#ifdef DEBUG
    char *name = get_atom_name(state);
    PRINTF("EWMH: handle_wm_state: win %#x, state: %s, action: %d %s\n", c->win,
           name, action, c->win ? "" : "(win not found)");
    FREE(name);
#endif
    if (!c->win)
        return;

    if (state == ewmh->_NET_WM_STATE_MAXIMIZED_VERT) {
        if (action == XCB_EWMH_WM_STATE_ADD) {
            maximizeaxis_client(c, MaxVertical);
        } else if (action == XCB_EWMH_WM_STATE_REMOVE) {
            maximizeaxis_client(c, MaxVertical);
        } else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
            maximizeaxis_client(c, MaxVertical);
        }
    } else if (state == ewmh->_NET_WM_STATE_MAXIMIZED_HORZ) {
        if (action == XCB_EWMH_WM_STATE_ADD) {
            maximizeaxis_client(c, MaxHorizontal);
        } else if (action == XCB_EWMH_WM_STATE_REMOVE) {
            maximizeaxis_client(c, MaxHorizontal);
        } else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
            maximizeaxis_client(c, MaxHorizontal);
        }
    } else if (state == ewmh->_NET_WM_STATE_FULLSCREEN) {
        if (action == XCB_EWMH_WM_STATE_ADD) {
            maximizeclient(c, true);
        } else if (action == XCB_EWMH_WM_STATE_REMOVE) {
            maximizeclient(c, false);
        } else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
            maximizeclient(c, c->ewmh_flags & EWMH_FULLSCREEN);
        }
    } else if (state == ewmh->_NET_WM_STATE_ABOVE) {
        if (action == XCB_EWMH_WM_STATE_ADD) {
        } else if (action == XCB_EWMH_WM_STATE_REMOVE) {
        } else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
        }
    } else if (state == ewmh->_NET_WM_STATE_STICKY) {
        if (action == XCB_EWMH_WM_STATE_ADD) {
        } else if (action == XCB_EWMH_WM_STATE_REMOVE) {
        } else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
        }
    } else if (state == ewmh->_NET_WM_STATE_DEMANDS_ATTENTION) {
        if (action == XCB_EWMH_WM_STATE_ADD) {
        } else if (action == XCB_EWMH_WM_STATE_REMOVE) {
        } else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
        }
    } else if (state == ewmh->_NET_WM_STATE_MODAL) {
        if (action == XCB_EWMH_WM_STATE_ADD) {
        } else if (action == XCB_EWMH_WM_STATE_REMOVE) {
        } else if (action == XCB_EWMH_WM_STATE_TOGGLE) {
        }
    }
}

void ewmh_update_wm_state(Client *c) {
    xcb_atom_t v[MAX_STATE];
    int i = 0;

    if (c->ewmh_flags & EWMH_MAXIMIZED_VERT)
        v[i++] = ewmh->_NET_WM_STATE_MAXIMIZED_VERT;
    if (c->ewmh_flags & EWMH_MAXIMIZED_HORZ)
        v[i++] = ewmh->_NET_WM_STATE_MAXIMIZED_HORZ;
    if (c->ewmh_flags & EWMH_FULLSCREEN)
        v[i++] = ewmh->_NET_WM_STATE_FULLSCREEN;
    if (c->ewmh_flags & EWMH_STICKY)
        v[i++] = ewmh->_NET_WM_STATE_STICKY;
    if (c->ewmh_flags & EWMH_DEMANDS_ATTENTION)
        v[i++] = ewmh->_NET_WM_STATE_DEMANDS_ATTENTION;
    if (c->ewmh_flags & EWMH_ABOVE)
        v[i++] = ewmh->_NET_WM_STATE_ABOVE;

    if (i > 0)
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, c->win,
                            ewmh->_NET_WM_STATE, XCB_ATOM_ATOM, 32, i, v);
    else
        xcb_delete_property(conn, c->win, ewmh->_NET_WM_STATE);
}

void ewmh_get_wm_state(Client *c) {
    xcb_ewmh_get_atoms_reply_t win_state;

    if (xcb_ewmh_get_wm_state_reply(ewmh, xcb_ewmh_get_wm_state(ewmh, c->win),
                                    &win_state, NULL) != 1)
        return;

    for (unsigned int i = 0; i < win_state.atoms_len; i++) {
        xcb_atom_t a = win_state.atoms[i];
#ifdef DEBUG
        char *name = get_atom_name(a);
        PRINTF("EWMH: state: win %#x, atom %s\n", c->win, name);
        FREE(name);
#endif
        change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, a);
        if (a == ewmh->_NET_WM_STATE_FULLSCREEN) {
            change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, EWMH_FULLSCREEN);
        } else if (a == ewmh->_NET_WM_STATE_MAXIMIZED_VERT) {
            change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, EWMH_MAXIMIZED_VERT);
        } else if (a == ewmh->_NET_WM_STATE_MAXIMIZED_HORZ) {
            change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, EWMH_MAXIMIZED_HORZ);
        } else if (a == ewmh->_NET_WM_STATE_STICKY) {
            change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, EWMH_STICKY);
        } else if (a == ewmh->_NET_WM_STATE_DEMANDS_ATTENTION) {
            change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, EWMH_DEMANDS_ATTENTION);
        } else if (a == ewmh->_NET_WM_STATE_ABOVE) {
            change_ewmh_flags(c, XCB_EWMH_WM_STATE_ADD, EWMH_ABOVE);
        }
    }
    xcb_ewmh_get_atoms_reply_wipe(&win_state);
}

void ewmh_update_client_list(Client *list) {
    Client *t;
    int count = 0;

    for (t = list; t; t = t->next)
        count++;

    if (count == 0)
        return;

    PRINTF("EWMH: client list: %d windows\n", count);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, screen->root,
                        ewmh->_NET_CLIENT_LIST, XCB_ATOM_WINDOW, 32, count,
                        list);
}

void ewmh_get_wm_window_type(Client *c) {
    xcb_ewmh_get_atoms_reply_t win_type;

    if (xcb_ewmh_get_wm_window_type_reply(
            ewmh, xcb_ewmh_get_wm_window_type(ewmh, c->win), &win_type, NULL) ==
        1) {
        for (unsigned int i = 0; i < win_type.atoms_len; i++) {
            xcb_atom_t a = win_type.atoms[i];
/* (void)a; */
#ifdef DEBUG
            char *name = get_atom_name(a);
            PRINTF("EWMH: window type: win %#x, atom %s\n", c->win, name);
            FREE(name);
#endif
            if (a == ewmh->_NET_WM_WINDOW_TYPE_DIALOG ||
                a == ewmh->_NET_WM_WINDOW_TYPE_SPLASH ||
                a == ewmh->_NET_WM_WINDOW_TYPE_NOTIFICATION) {
                teleport_client(c, Center);
            }

            if (a == ewmh->_NET_WM_WINDOW_TYPE_DESKTOP ||
                ewmh->_NET_WM_WINDOW_TYPE_DOCK ||
                ewmh->_NET_WM_WINDOW_TYPE_TOOLBAR ||
                ewmh->_NET_WM_WINDOW_TYPE_MENU ||
                ewmh->_NET_WM_WINDOW_TYPE_UTILITY ||
                ewmh->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU ||
                ewmh->_NET_WM_WINDOW_TYPE_POPUP_MENU ||
                ewmh->_NET_WM_WINDOW_TYPE_TOOLTIP ||
                ewmh->_NET_WM_WINDOW_TYPE_COMBO ||
                ewmh->_NET_WM_WINDOW_TYPE_DND ||
                ewmh->_NET_WM_WINDOW_TYPE_NORMAL) {
            }
        }
        xcb_ewmh_get_atoms_reply_wipe(&win_type);
    }
}

bool ewmh_get_supporting_wm_check(xcb_window_t *win) {
    xcb_get_property_cookie_t cookie;
    cookie = xcb_ewmh_get_supporting_wm_check_unchecked(ewmh, screen->root);
    if (xcb_ewmh_get_supporting_wm_check_reply(ewmh, cookie, win, NULL) == 0) {
        return false;
    }
    return true;
}
