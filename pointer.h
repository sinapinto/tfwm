/* See LICENSE file for copyright and license details. */
#ifndef POINTER_H
#define POINTER_H

void mousemotion(const Arg *arg);
void load_cursors(void);
void free_cursors(void);
void warp_pointer(Client *c);

struct cursor_t {
	const char   *name;
	uint8_t       cf_glyph;
	xcb_cursor_t  cid;
} ;

extern struct cursor_t cursors[XC_MAX];

#endif

