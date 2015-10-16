#define MOD                           XCB_MOD_MASK_1
#define SHIFT                         XCB_MOD_MASK_SHIFT
#define CTRL                          XCB_MOD_MASK_CONTROL
int steps[2] =                        { 30, 40 }; /* move step, resize step */
#define UNFOCUS                       0x3B3B31
#define FOCUS                         0xafaf87
#define BORDER_WIDTH                  2
char *border_blacklist[] =            { "Chromium" }; /* no borders */
#define CENTER_BY_DEFAULT             1 /* windows start centered */
#define NUM_WORKSPACES                5
#define DESKTOPCHANGE(K,N) \
{  MOD ,             K,               change_workspace, {.i=N}}, \
{  MOD | SHIFT,      K,               client_to_workspace, {.i=N}},

static Key keys[] = {
   /* mod               keycode         function                      arg   */
    { MOD,              XK_j,           move,                         {.i=0}   },
    { MOD,              XK_l,           move,                         {.i=1}   },
    { MOD,              XK_k,           move,                         {.i=2}   },
    { MOD,              XK_h,           move,                         {.i=3}   },
    { MOD | SHIFT,      XK_j,           resize,                       {.i=0}   },
    { MOD | SHIFT,      XK_l,           resize,                       {.i=1}   },
    { MOD | SHIFT,      XK_k,           resize,                       {.i=2}   },
    { MOD | SHIFT,      XK_h,           resize,                       {.i=3}   },
    { MOD | CTRL,       XK_j,           resize,                       {.i=4}   },
    { MOD | CTRL,       XK_k,           resize,                       {.i=5}   },
    { MOD,              XK_m,           vert_max_toggle,              {.i=0}   },
    { MOD,              XK_s,           toggle_centered,              {.i=0}   },
    { MOD,              XK_a,           toggle_maximize,              {.i=0}   },
    { MOD,              XK_Tab,         cycle_win,                    {.i=0}   },
    { MOD | SHIFT,      XK_Tab,         cycle_win,                    {.i=1}   },
    { MOD | CTRL,       XK_Tab,         cycle_win,                    {.i=2}   },
    { MOD,              XK_q,           kill_current,                 {.i=0}   },
    { MOD | SHIFT,      XK_e,           quit,                         {.i=0}   },
    { MOD ,             XK_grave,       change_to_previous_workspace, {.i=0}   },

     DESKTOPCHANGE(     XK_1,           0)
     DESKTOPCHANGE(     XK_2,           1)
     DESKTOPCHANGE(     XK_3,           2)
     DESKTOPCHANGE(     XK_4,           3)
     DESKTOPCHANGE(     XK_5,           4)
};
