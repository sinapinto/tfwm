/* See LICENSE file for copyright and license details. */
#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_FOCUS_COLOR "sky blue"
#define DEFAULT_UNFOCUS_COLOR "slate gray"

char *find_config(const char *);
int parse_config(const char *);

extern bool center_new_windows;
extern int border_width;
extern char *focus_color;
extern char *unfocus_color;
extern int move_step;
extern int resize_step;
extern bool java_workaround;
extern int cursor_position;
extern uint32_t focus_pixel;
extern uint32_t unfocus_pixel;

#endif
