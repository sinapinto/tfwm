#ifndef LIST_H
#define LIST_H

#include "types.h"

void attach(Client *c);
void attachstack(Client *c);
void detach(Client *c);
void detachstack(Client *c);
void focus(Client *c);
void focusstack(const Arg *arg);

#endif

