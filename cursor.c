/* See LICENSE file for copyright and license details. */
#include <xcb/xcb_cursor.h>
#include "main.h"
#include "util.h"
#include "cursor.h"

static xcb_cursor_context_t *ctx;
static xcb_cursor_t cursors[XC_MAX];

/* definitions in /usr/include/X11/cursorfont.h */
static const int xcb_cursors[XC_MAX] = {
	68, /* XC_left_ptr */
	14, /* XC_bottom_right_corner */
	52, /* XC_fleur */
};

void
cursor_load_cursors(void) {
	if (xcb_cursor_context_new(conn, screen, &ctx) < 0) {
		warn("xcursor support unavailable\n");
		/* xcursor_supported = false; */
		return;
	}

	cursors[XC_POINTER] = xcb_cursor_load_cursor(ctx, "left_ptr");
}

xcb_cursor_t
cursor_get_xcursor(enum xcursor_t c) {
	if (c < XC_MAX)
		return cursors[c];
	return -1;
}

int
cursor_get_cursorfont(enum xcursor_t c) {
	if (c < XC_MAX)
		return xcb_cursors[c];
	return -1;
}

void
cursor_set_window_cursor(xcb_window_t win, enum xcursor_t c) {
	xcb_change_window_attributes_checked(conn, win, XCB_CW_CURSOR,
					     (uint32_t[]){cursors[c]});
}

void
cursor_free_cursors(void) {
}
