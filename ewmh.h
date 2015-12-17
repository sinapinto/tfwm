/* See LICENSE file for copyright and license details. */
#ifndef EWMH_H
#define EWMH_H

void ewmh_setup();
void ewmh_teardown();
void change_ewmh_flags(Client *c, xcb_ewmh_wm_state_action_t op, uint32_t mask);
void handle_wm_state(Client *c, xcb_atom_t state, xcb_ewmh_wm_state_action_t action);
void ewmh_get_wm_state(Client *c);
void ewmh_update_wm_state(Client *c);
void ewmh_update_client_list(Client *list);
void ewmh_get_wm_window_type(Client *c);

#endif


