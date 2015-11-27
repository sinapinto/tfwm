/* See LICENSE file for copyright and license details. */
#ifndef CONFIG_H
#define CONFIG_H

#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include "events.h"

#define MOD                           XCB_MOD_MASK_1
#define SHIFT                         XCB_MOD_MASK_SHIFT
#define CTRL                          XCB_MOD_MASK_CONTROL
#define DOUBLE_BORDER                 false
#define BORDER_WIDTH                  6
#define OUTER_BORDER_WIDTH            4
#define FOCUS_COLOR                   "tomato"
#define OUTER_COLOR                   "black"
#define UNFOCUS_COLOR                 "slate gray"
#define MOVE_STEP                     30
#define RESIZE_STEP                   30

static const Rule rules[] = {
	/* class           workspace       fullscreen       border */
	{ "chromium",      1,              false,           false  },
	{ "firefox",       0,              false,           false  },
};

#define WORKSPACE(K,N) \
	{ MOD,              K,           selectws,               {.i=N} }, \
	{ MOD | SHIFT,      K,           sendtows,               {.i=N} },

static const char *terminal[]  = { "urxvt", NULL };
static const char *terminal2[] = { "termite", NULL };
static const char *browser[]   = { "chromium", NULL };
static const char *browser2[]  = { "firefox", NULL };
static const char *launcher[]  = { "rofi", "-show", "run",  NULL };
static const char *mpctoggle[] = { "mpc", "toggle", NULL };
static const char *mpcseekf[]  = { "mpc", "seek", "+30", NULL };
static const char *mpcseekb[]  = { "mpc", "seek", "-30", NULL };
static const char *mpcnext[]   = { "mpc", "next", NULL };
static const char *mpcprev[]   = { "mpc", "prev", NULL };
static const char *volup[]     = { "amixer", "set", "Master", "3%+", "unmute", NULL };
static const char *voldown[]   = { "amixer", "set", "Master", "3%-", "unmute", NULL };
static const char *voltoggle[] = { "amixer", "set", "Master", "toggle", NULL };

static Key keys[] = {
	/* modifier     key                       function       argument */
	{ MOD,          XK_Return,                spawn,         {.com=terminal}    },
	{ MOD,          XK_t,                     spawn,         {.com=terminal2}   },
	{ MOD,          XK_w,                     spawn,         {.com=browser}     },
	{ MOD,          XK_e,                     spawn,         {.com=browser2}    },
	{ MOD,          XK_space,                 spawn,         {.com=launcher}    },
	{ MOD,          XK_p,                     spawn,         {.com=mpctoggle}   },
	{ XCB_NONE,     XK_Pause,                 spawn,         {.com=mpcseekf}    },
	{ XCB_NONE,     XK_Print,                 spawn,         {.com=mpcseekb}    },
	{ MOD,          XK_o,                     spawn,         {.com=mpcnext}     },
	{ MOD,          XK_i,                     spawn,         {.com=mpcprev}     },
	{ XCB_NONE,     XK_F2,                    spawn,         {.com=volup}       },
	{ XCB_NONE,     XK_F1,                    spawn,         {.com=voldown}     },
	{ XCB_NONE,     XF86XK_AudioRaiseVolume,  spawn,         {.com=volup}       },
	{ XCB_NONE,     XF86XK_AudioLowerVolume,  spawn,         {.com=voldown}     },
	{ XCB_NONE,     XF86XK_AudioMute,         spawn,         {.com=voltoggle}   },
	{ MOD,          XK_j,                     move,          {.i=MoveDown}      },
	{ MOD,          XK_l,                     move,          {.i=MoveRight}     },
	{ MOD,          XK_k,                     move,          {.i=MoveUp}        },
	{ MOD,          XK_h,                     move,          {.i=MoveLeft}      },
	{ MOD | SHIFT,  XK_j,                     resize,        {.i=GrowHeight}    },
	{ MOD | SHIFT,  XK_l,                     resize,        {.i=GrowWidth}     },
	{ MOD | SHIFT,  XK_k,                     resize,        {.i=ShrinkHeight}  },
	{ MOD | SHIFT,  XK_h,                     resize,        {.i=ShrinkWidth}   },
	{ MOD | CTRL,   XK_j,                     resize,        {.i=GrowBoth}      },
	{ MOD | CTRL,   XK_k,                     resize,        {.i=ShrinkBoth}    },
	{ MOD,          XK_Tab,                   focusstack,    {.i=PrevWindow}    },
	{ MOD | SHIFT,  XK_Tab,                   focusstack,    {.i=NextWindow}    },
	{ MOD,          XK_s,                     teleport,      {.i=ToCenter}      },
	{ MOD | SHIFT,  XK_p,                     teleport,      {.i=ToTop}         },
	{ MOD | SHIFT,  XK_n,                     teleport,      {.i=ToBottom}      },
	{ MOD | SHIFT,  XK_y,                     teleport,      {.i=ToLeft}        },
	{ MOD | SHIFT,  XK_u,                     teleport,      {.i=ToRight}       },
	{ MOD,          XK_a,                     maximize,      {.i=NULL}          },
	{ MOD,          XK_m,                     maximizeaxis,  {.i=MaxVertical}   },
	{ MOD,          XK_n,                     maximizeaxis,  {.i=MaxHorizontal} },
	{ MOD | CTRL,   XK_s,                     sticky,        {.i=NULL}          },
	{ MOD,          XK_q,                     killselected,  {.i=NULL}          },
	{ MOD,          XK_grave,                 selectrws,     {.i=LastWorkspace} },
	{ MOD,          XK_bracketleft,           selectrws,     {.i=PrevWorkspace} },
	{ MOD,          XK_bracketright,          selectrws,     {.i=NextWorkspace} },
	WORKSPACE(      XK_1,                                    0 )
	WORKSPACE(      XK_2,                                    1 )
	WORKSPACE(      XK_3,                                    2 )
	WORKSPACE(      XK_4,                                    3 )
	WORKSPACE(      XK_5,                                    4 )
	WORKSPACE(      XK_6,                                    5 )
	WORKSPACE(      XK_7,                                    6 )
	WORKSPACE(      XK_8,                                    7 )
	WORKSPACE(      XK_9,                                    8 )
	WORKSPACE(      XK_0,                                    9 )
	{ MOD | SHIFT,  XK_r,                     restart,       {.i=NULL}          },
	{ MOD | SHIFT,  XK_e,                     quit,          {.i=NULL}          },
};
static Button buttons[] = {
	{  MOD,          XCB_BUTTON_INDEX_1,      mousemotion,   {.i=MouseMove}     },
	{  MOD,          XCB_BUTTON_INDEX_3,      mousemotion,   {.i=MouseResize}   }
};

#endif

