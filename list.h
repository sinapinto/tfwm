/* See LICENSE file for copyright and license details. */
#ifndef LIST_H
#define LIST_H

void attach(Client *c);
void attachstack(Client *c);
void detach(Client *c);
void detachstack(Client *c);
void focusstack(bool next);

#endif
