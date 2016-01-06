/* See LICENSE file for copyright and license details. */
#ifndef CURSOR_H
#define CURSOR_H

enum xcursor_t {
    XC_POINTER,
    XC_BOTTOM_RIGHT,
    XC_MOVE,
    XC_MAX
};

void cursor_load_cursors(void);
xcb_cursor_t cursor_get_xcursor(enum xcursor_t c);
int cursor_get_cursorfont(enum xcursor_t c);
void cursor_set_window_cursor(xcb_window_t win, enum xcursor_t c);
void cursor_free_cursors(void);

#endif

