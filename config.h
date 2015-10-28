#define MOD                           XCB_MOD_MASK_1
#define SHIFT                         XCB_MOD_MASK_SHIFT
#define CTRL                          XCB_MOD_MASK_CONTROL
#define UNFOCUS                       0xFF0000
#define FOCUS                         0x00FF00
#define BORDER_WIDTH                  0
static const int steps[2] =           { 30, 40 }; /* move step, resize step */

#define WORKSPACE(K,N) \
	{ MOD,              K,           selectws,               {.i=N} }, \
	{ MOD | SHIFT,      K,           sendtows,               {.i=N} },

static const char *terminal[] = { "urxvt", NULL };

static Key keys[] = {
	/* modifier         key          function               argument */
	{ MOD,              XK_Return,   spawn,                 {.com=terminal}    },
	{ MOD,              XK_j,        move,                  {.i=MoveDown}      },
	{ MOD,              XK_l,        move,                  {.i=MoveRight}     },
	{ MOD,              XK_k,        move,                  {.i=MoveUp}        },
	{ MOD,              XK_h,        move,                  {.i=MoveLeft}      },
	{ MOD | SHIFT,      XK_j,        resize,                {.i=GrowHeight}    },
	{ MOD | SHIFT,      XK_l,        resize,                {.i=GrowWidth}     },
	{ MOD | SHIFT,      XK_k,        resize,                {.i=ShrinkHeight}  },
	{ MOD | SHIFT,      XK_h,        resize,                {.i=ShrinkWidth}   },
	{ MOD | CTRL,       XK_j,        resize,                {.i=GrowBoth}      },
	{ MOD | CTRL,       XK_k,        resize,                {.i=ShrinkBoth}    },
	{ MOD,              XK_Tab,      focusstack,            {.i=+1}            },
	{ MOD | SHIFT,      XK_Tab,      focusstack,            {.i=-1}            },
	{ MOD,              XK_s,        centerwin,             {.i=NULL}          },
	{ MOD,              XK_a,        maximize,              {.i=NULL}          },
	{ MOD,              XK_m,        maximizeaxis,          {.i=MaxVertical}   },
	{ MOD,              XK_n,        maximizeaxis,          {.i=MaxHorizontal} },
	{ MOD,              XK_x,        fix,                   {.i=NULL}          },
	{ MOD,              XK_q,        killclient,            {.i=NULL}          },
	{ MOD,              XK_grave,    selectprevws,          {.i=NULL}          },
	WORKSPACE(          XK_1,                               0 )
	WORKSPACE(          XK_2,                               1 )
	WORKSPACE(          XK_3,                               2 )
	WORKSPACE(          XK_4,                               3 )
	WORKSPACE(          XK_5,                               4 )
	{ MOD | SHIFT,      XK_e,        quit,                  {.i=NULL}          },
};
static Button buttons[] = {
	{  MOD,     XCB_BUTTON_INDEX_1,  mousemotion,           {.i=MouseMove}     },
	{  MOD,     XCB_BUTTON_INDEX_3,  mousemotion,           {.i=MouseResize}   }
};
