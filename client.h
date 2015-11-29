#ifndef WINDOW_H
#define WINDOW_H

#include <xcb/xcb.h>

extern void applyrules(Client *c);
extern void cycleclients(const Arg *arg);
extern void fitclient(Client *c);
extern void gethints(Client *c);
extern void killselected(const Arg *arg);
extern void manage(xcb_window_t w);
extern void maximize(const Arg *arg);
extern void maximizeaxis(const Arg *arg);
extern void maximizeclient(Client *c, bool doit);
extern void move(const Arg *arg);
extern void movewin(xcb_window_t win, int x, int y);
extern void moveresize(Client *c, int w, int h, int x, int y);
extern void raisewindow(xcb_drawable_t win);
extern void resize(const Arg *arg);
extern void resizewin(xcb_window_t win, int w, int h);
extern void savegeometry(Client *c);
extern bool sendevent(Client *c, xcb_atom_t proto);
extern void sendtows(const Arg *arg);
extern void setborder(Client *c, bool focus);
extern void setborderwidth(Client *c, int width);
extern void showhide(Client *c);
extern void sticky(const Arg *arg);
extern void teleport(const Arg *arg);
extern void unmanage(Client *c);
extern Client *wintoclient(xcb_window_t w);

#endif

