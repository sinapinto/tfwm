#ifndef EVENTS_H
#define EVENTS_H

void buttonpress(xcb_generic_event_t *ev);
void circulaterequest(xcb_generic_event_t *ev);
void clientmessage(xcb_generic_event_t *ev);
void configurerequest(xcb_generic_event_t *ev);
void destroynotify(xcb_generic_event_t *ev);
void enternotify(xcb_generic_event_t *ev);
void keypress(xcb_generic_event_t *ev);
void mappingnotify(xcb_generic_event_t *ev);
void maprequest(xcb_generic_event_t *ev);
void mousemotion(const Arg *arg);
void requesterror(xcb_generic_event_t *ev);
void unmapnotify(xcb_generic_event_t *ev);

#endif

