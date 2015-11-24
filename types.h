/* See LICENSE file for copyright and license details. */
#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include "util.h"

typedef union {
	const char **com;
	const int i;
} Arg;

typedef struct {
	const char *class;
	unsigned int workspace;
	bool *fullscreen;
	bool border;
} Rule;

typedef struct Key {
	unsigned int mod;
	xcb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;

typedef struct Client Client;
struct Client{
	// TODO use xcb_rectangle_t
	int16_t x, y, oldx, oldy;
	uint16_t w, h, oldw, oldh;
	int32_t basew, baseh, minw, minh, incw, inch;
	bool ismax, isvertmax, ishormax;
	bool isfixed, noborder;
	Client *next;
	Client *snext;
	xcb_window_t win;
	unsigned int ws;
};

/* enums */
enum { MoveDown, MoveRight, MoveUp, MoveLeft };
enum { GrowHeight, GrowWidth, ShrinkHeight, ShrinkWidth, GrowBoth, ShrinkBoth };
enum { ToCenter, ToTop, ToLeft, ToBottom, ToRight };
enum { MouseMove, MouseResize };
enum { MaxVertical, MaxHorizontal };
enum { LastWorkspace, PrevWorkspace, NextWorkspace };
enum { PrevWindow, NextWindow };

#endif
