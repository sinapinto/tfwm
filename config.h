/* See LICENSE file for copyright and license details. */
#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_FOCUS_COLOR   "sky blue"
#define DEFAULT_UNFOCUS_COLOR "rebecca purple"

char *find_config(const char *);
int parse_config(const char *);

extern bool pixmap_border;
extern bool center_new_windows;
extern int border_width;
extern char *focus_color;
extern char *unfocus_color;
extern int move_step;
extern int resize_step;
extern bool sloppy_focus;
extern bool java_workaround;
extern int cursor_position;
extern uint32_t focuscol;
extern uint32_t unfocuscol;

#endif

