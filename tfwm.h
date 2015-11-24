/* See LICENSE file for copyright and license details. */
#ifndef TFWM_H
#define TFWM_H

#include <xcb/xcb_ewmh.h>
#include "types.h"

xcb_connection_t *conn;
xcb_screen_t *screen;
unsigned int sw, sh;
unsigned int selws;
unsigned int prevws;
unsigned int numlockmask;
int scrno;
Client *clients;
Client *sel;
Client *stack;
int sigcode;
void (*handler[XCB_NO_OPERATION])(xcb_generic_event_t *ev);
xcb_ewmh_connection_t *ewmh;
uint32_t focuscol, unfocuscol, outercol;
bool dorestart;
xcb_atom_t WM_DELETE_WINDOW;

#endif
