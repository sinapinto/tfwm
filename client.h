/* See LICENSE file for copyright and license details. */
#ifndef WINDOW_H
#define WINDOW_H

#include <xcb/xcb.h>

void applyrules(Client *c);
void cycleclients(const Arg *arg);
void fitclient(Client *c);
void killselected(const Arg *arg);
void manage(xcb_window_t w);
void maximize(const Arg *arg);
void maximizeaxis(const Arg *arg);
void maximizeclient(Client *c, bool doit);
void move(const Arg *arg);
void movewin(xcb_window_t win, int x, int y);
void moveresize(Client *c, int w, int h, int x, int y);
void raisewindow(xcb_drawable_t win);
void reparent(Client *c);
void resize(const Arg *arg);
void resizewin(xcb_window_t win, int w, int h);
void savegeometry(Client *c);
void send_client_message(Client *c, xcb_atom_t proto);
void setborder(Client *c, bool focus);
void setborderwidth(Client *c, int width);
void showhide(Client *c);
void teleport(const Arg *arg);
void unmanage(Client *c);
Client *wintoclient(xcb_window_t w);

#endif

