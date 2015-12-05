/* See LICENSE file for copyright and license details. */
#ifndef EVENTS_H
#define EVENTS_H

void handleevent(xcb_generic_event_t *ev);
void mousemotion(const Arg *arg);
void maprequest(xcb_generic_event_t *ev);

#endif
