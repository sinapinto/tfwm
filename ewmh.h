/* See LICENSE file for copyright and license details. */
#ifndef EWMH_H
#define EWMH_H

void ewmh_setup();
void ewmh_teardown();
bool ewmh_add_wm_state(Client *c, xcb_atom_t state);
bool ewmh_remove_wm_state(Client *c, xcb_atom_t state);
void ewmh_update_client_list(Client *list);

#endif


