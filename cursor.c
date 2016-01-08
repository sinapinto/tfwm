/* See LICENSE file for copyright and license details. */
#include <xcb/xcb_cursor.h>
#include <string.h>
#include "main.h"
#include "util.h"
#include "cursor.h"

static xcb_cursor_context_t *ctx;
static xcb_cursor_t cursors[XC_MAX];

void cursor_load_cursors(void) {
    if (xcb_cursor_context_new(conn, screen, &ctx) < 0) {
        warn("xcursor support unavailable\n");
        xcb_font_t font = xcb_generate_id(conn);
        xcb_open_font(conn, font, strlen("cursor"), "cursor");

#define LOAD_CURSORFONT(_c, _char)                                             \
    do {                                                                       \
        cursors[_c] = xcb_generate_id(conn);                                   \
        xcb_create_glyph_cursor(conn, cursors[_c], font, font, _char,          \
                                (_char + 1), 0, 0, 0, 0xffff, 0xffff, 0xffff); \
    } while (0)
        /* cursorfont defs: /usr/include/X11/cursorfont.h */
        LOAD_CURSORFONT(XC_POINTER, 68);
        LOAD_CURSORFONT(XC_BOTTOM_RIGHT, 14);
        LOAD_CURSORFONT(XC_MOVE, 52);
#undef LOAD_CURSOR
        if (font != XCB_NONE)
            xcb_close_font(conn, font);
    } else {
        cursors[XC_POINTER] = xcb_cursor_load_cursor(ctx, "left_ptr");
        cursors[XC_BOTTOM_RIGHT] =
            xcb_cursor_load_cursor(ctx, "bottom_right_corner");
        cursors[XC_MOVE] = xcb_cursor_load_cursor(ctx, "fleur");
    }
}

xcb_cursor_t cursor_get_id(enum a_cursor_t c) {
    if (c >= XC_MAX)
        return XCB_NONE;
    return cursors[c];
}

void cursor_set_window_cursor(xcb_window_t win, enum a_cursor_t c) {
    if (c < XC_MAX)
        xcb_change_window_attributes(conn, win, XCB_CW_CURSOR,
                                     (uint32_t[]){cursors[c]});
}

void cursor_free_context(void) {
    for (int i = 0; i < LENGTH(cursors); ++i)
        xcb_free_cursor(conn, cursors[i]);
    xcb_cursor_context_free(ctx);
}
