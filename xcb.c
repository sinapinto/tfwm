/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "xcb.h"
#include "config.h"
#include "util.h"

#ifdef DEBUG
char *get_atom_name(xcb_atom_t atom) {
    xcb_get_atom_name_reply_t *r =
        xcb_get_atom_name_reply(conn, xcb_get_atom_name(conn, atom), NULL);
    if (!r)
        return NULL;

    char *name;
    int len = xcb_get_atom_name_name_length(r);
    if (len) {
        if (!(name = malloc(len + 1)))
            err("can't allocate memory.");
        memcpy(name, xcb_get_atom_name_name(r), len);
        name[len] = '\0';
    }

    FREE(r);
    return name;
}
#endif

bool connection_has_error(void) {
    int err = 0;

    if ((err = xcb_connection_has_error(conn)) > 0) {
        warn("X connection error: ");
        switch (err) {
        case XCB_CONN_ERROR:
            warn("socket, pipe, or other stream error.\n");
            break;
        case XCB_CONN_CLOSED_EXT_NOTSUPPORTED:
            warn("extension not supported.\n");
            break;
        case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
            warn("memory not available.\n");
            break;
        case XCB_CONN_CLOSED_REQ_LEN_EXCEED:
            warn("exceeding request length.\n");
            break;
        case XCB_CONN_CLOSED_PARSE_ERR:
            warn("error parsing display string.\n");
            break;
        case XCB_CONN_CLOSED_INVALID_SCREEN:
            warn("no screen matching the display.\n");
            break;
        default:
            warn("unknown error.\n");
            break;
        }
        return true;
    }

    return false;
}

void getatom(xcb_atom_t *atom, const char *name) {
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(
        conn, xcb_intern_atom(conn, 0, strlen(name), name), NULL);

    if (r) {
        *atom = r->atom;
        FREE(r);
    } else {
        *atom = XCB_NONE;
    }
}

uint32_t getcolor(const char *color) {
    uint32_t pixel;

    xcb_colormap_t map = screen->default_colormap;

    if (color[0] == '#') {
        unsigned int r, g, b;
        if (sscanf(color + 1, "%02x%02x%02x", &r, &g, &b) != 3)
            err("bad color: %s.", color);
        /* convert from 8-bit to 16-bit */
        r = (r << 8) | r;
        g = (g << 8) | g;
        b = (b << 8) | b;
        xcb_alloc_color_reply_t *cr = xcb_alloc_color_reply(
            conn, xcb_alloc_color(conn, map, r, g, b), NULL);
        if (!cr)
            err("can't alloc color.");
        pixel = cr->pixel;
        FREE(cr);
    } else {
        xcb_alloc_named_color_reply_t *ncr = xcb_alloc_named_color_reply(
            conn, xcb_alloc_named_color(conn, map, strlen(color), color), NULL);
        if (!ncr)
            err("can't alloc named color.");
        pixel = ncr->pixel;
        FREE(ncr);
    }
    return pixel;
}

void grabbuttons(Client *c) {
    uint16_t modifiers[4];
    modifiers[0] = 0;
    modifiers[1] = XCB_MOD_MASK_LOCK;
    modifiers[2] = numlockmask;
    modifiers[3] = numlockmask | XCB_MOD_MASK_LOCK;

    for (int i = 0; i < LENGTH(buttons); i++) {
        for (int j = 0; j < LENGTH(modifiers); j++) {
            xcb_grab_button(conn, 1, c->win, XCB_EVENT_MASK_BUTTON_PRESS,
                            XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
                            screen->root, XCB_NONE, buttons[i].button,
                            buttons[i].mask | modifiers[j]);
        }
    }
}

void warp_pointer(Client *c) {
    int16_t x = 0;
    int16_t y = 0;

    switch (cursor_position) {
    case 0:
        return;
    case 1:
        break;
    case 2:
        x += c->geom.width;
        break;
    case 3:
        y += c->geom.height;
        break;
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
