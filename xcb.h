/* See LICENSE file for copyright and license details. */
#ifndef XCB_H
#define XCB_H

#ifdef DEBUG
char * get_atom_name(xcb_atom_t atom);
#endif
bool connection_has_error(void);
void getatom(xcb_atom_t *atom, char *name);
uint32_t getcolor(char *color);
void grabbuttons(Client *c);

#endif

