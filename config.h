#define MOD                           XCB_MOD_MASK_1
#define SHIFT                         XCB_MOD_MASK_SHIFT
#define CTRL                          XCB_MOD_MASK_CONTROL
#define UNFOCUS                       0x3B3B31
#define FOCUS                         0xafaf87
#define BORDER_WIDTH                  2
int steps[2] =                        { 30, 40 }; /* move step, resize step */

#define WORKSPACE(K,N)                { MOD, K, selectws, {.i=N} },

static Key keys[] = {
	/* modifier         key          function                argument */
	{ MOD,              XK_j,        move,                   {.i=MoveDown}    },
	{ MOD,              XK_l,        move,                   {.i=MoveRight}   },
	{ MOD,              XK_k,        move,                   {.i=MoveUp}      },
	{ MOD,              XK_h,        move,                   {.i=MoveLeft}    },
	{ MOD | SHIFT,      XK_j,        resize,                 {.i=ResizeDown}  },
	{ MOD | SHIFT,      XK_l,        resize,                 {.i=ResizeRight} },
	{ MOD | SHIFT,      XK_k,        resize,                 {.i=ResizeUp}    },
	{ MOD | SHIFT,      XK_h,        resize,                 {.i=ResizeLeft}  },
	{ MOD,              XK_Tab,      focusstack,             {.i=+1}           },
	{ MOD | SHIFT,      XK_Tab,      focusstack,             {.i=-1}           },
	{ MOD,              XK_a,        togglefullscreen,       {.i=0}            },
	{ MOD,              XK_q,        killclient,             {.i=0}            },
	WORKSPACE(          XK_1,                                0 )
	WORKSPACE(          XK_2,                                1 )
	WORKSPACE(          XK_3,                                2 )
	WORKSPACE(          XK_4,                                3 )
	WORKSPACE(          XK_5,                                4 )
	{ MOD | SHIFT,      XK_e,        quit,                   {.i=0}            },
};
