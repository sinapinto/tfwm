/* See LICENSE file for copyright and license details. */
#ifndef XCB_H
#define XCB_H

#ifdef DEBUG
char *get_atom_name(xcb_atom_t atom);
#endif
bool connection_has_error(void);
void getatom(xcb_atom_t *atom, const char *name);
uint32_t getcolor(const char *color);
void grabbuttons(Client *c);
void warp_pointer(Client *c);

#endif
