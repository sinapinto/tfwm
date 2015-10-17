#define MOD                           XCB_MOD_MASK_1
#define SHIFT                         XCB_MOD_MASK_SHIFT
#define CTRL                          XCB_MOD_MASK_CONTROL
int steps[2] =                        { 30, 40 }; /* move step, resize step */
#define UNFOCUS                       0x3B3B31
#define FOCUS                         0xafaf87
#define BORDER_WIDTH                  2
static Key keys[] = {
	/* modifier         key             function                      argument */
	{ MOD,              XK_j,           move,                         {.i=0}   },
	{ MOD,              XK_l,           move,                         {.i=1}   },
	{ MOD,              XK_k,           move,                         {.i=2}   },
	{ MOD,              XK_h,           move,                         {.i=3}   },
	{ MOD | SHIFT,      XK_j,           resize,                       {.i=0}   },
	{ MOD | SHIFT,      XK_l,           resize,                       {.i=1}   },
	{ MOD | SHIFT,      XK_k,           resize,                       {.i=2}   },
	{ MOD | SHIFT,      XK_h,           resize,                       {.i=3}   },
	{ MOD,              XK_Tab,         focusstack,                   {.i=+1}  },
	{ MOD | SHIFT,      XK_Tab,         focusstack,                   {.i=-1}  },
	{ MOD,              XK_a,           togglefullscreen,             {.i=0}   },
	{ MOD,              XK_q,           killclient,                   {.i=0}   },
	{ MOD | SHIFT,      XK_e,           quit,                         {.i=0}   },
};
