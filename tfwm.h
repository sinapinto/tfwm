/* See LICENSE file for copyright and license details. */
#ifndef TFWM_H
#define TFWM_H

#include <xcb/xcb_ewmh.h>
#include "types.h"

void cleanup();
void getatom(xcb_atom_t *atom, char *name);
uint32_t getcolor(char *color);
xcb_keycode_t *getkeycodes(xcb_keysym_t keysym);
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
void grabbuttons(Client *c);
void grabkeys();
void quit(const Arg *arg);
void restart(const Arg *arg);
void run(void);
void setup();
void sigcatch(int sig);
void sigchld();
void spawn(const Arg *arg);
void testcookie(xcb_void_cookie_t cookie, char *errormsg);
void updatenumlockmask();

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
