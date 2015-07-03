#define MOD     XCB_MOD_MASK_1 /* XCB_MOD_MASK_1 = Alt; XCB_MOD_MASK_4 = Windows key */
#define SHIFT   XCB_MOD_MASK_SHIFT
#define CTRL    XCB_MOD_MASK_CONTROL

/* 0: move step, 1: resize step
 * i.e. windows will move by increments of 25px and resize by 30px */
uint8_t steps[2] = { 30, 40 };

#define UNFOCUS      0x3B3B31  /* unfocused window border color */
#define FOCUS        0xafaf87  /* focused window border color */
#define BORDER_WIDTH 2         /* in pixels */

#define CENTER_BY_DEFAULT 1    /* windows start centered (0 to turn off) */

#define DESKTOPCHANGE(K,N) \
{  MOD ,             K,              change_workspace, {.i=N}}, \
{  MOD | SHIFT,      K,              client_to_workspace, {.i=N}},

/* commands */
static const char *terminal[] = { "urxvt", NULL };
static const char *browser[] = { "firefox", NULL };

#define NUM_WORKSPACES 5 /* number of workspaces (up to 10) */

static key keys[] = {
   /* mod               keycode         function            arg   */
    /* launch a new terminal emulator */
    { MOD,             XK_Return,       spawn,              {.com=terminal} },
    /* launch a browser */
    { MOD,             XK_w,            spawn,              {.com=browser} },
    /* move the window down */
    { MOD,              XK_j,           move,               {.i=0}   },
    /* move the window right */
    { MOD,              XK_l,           move,               {.i=1}   },
    /* move the window up */
    { MOD,              XK_k,           move,               {.i=2}   },
    /* move the window left */
    { MOD,              XK_h,           move,               {.i=3}   },
    /* grow the window vertically */
    { MOD | SHIFT,      XK_j,           resize,             {.i=0}   },
    /* grow the window horizontally */
    { MOD | SHIFT,      XK_l,           resize,             {.i=1}   },
    /* shrink the window vertically */
    { MOD | SHIFT,      XK_k,           resize,             {.i=2}   },
    /* shrink the window horizontally */
    { MOD | SHIFT,      XK_h,           resize,             {.i=3}   },
    /* grow the window height and width */
    { MOD | CTRL,       XK_j,           resize,             {.i=4}   },
    /* srhink the window height and width */
    { MOD | CTRL,       XK_k,           resize,             {.i=5}   },
    /* window remains centered while resizing (toggle) */
    { MOD,              XK_s,           toggle_centered,    {.i=0}   },
    /* fullscreen the window (toggle) */
    { MOD,              XK_a,           toggle_maximize,    {.i=0}   },
    /* focus the next window */
    { MOD,              XK_Tab,         cycle_win,          {.i=0}   },
    /* focus the previous window */
    { MOD | SHIFT,      XK_Tab,         cycle_win,          {.i=1}   },
    /* focus the next window without changing stacking order */
    { MOD | CTRL,       XK_Tab,         cycle_win,          {.i=2}   },
    /* kill the focused window */
    { MOD,              XK_q,           kill_current,       {.i=0}   },
    /* restart bwm */
    { MOD | SHIFT,      XK_r,           bwm_restart,        {.i=0}   },
    /* exit bwm */
    { MOD | SHIFT,      XK_e,           bwm_exit,           {.i=0}   },

    {  MOD ,            XK_grave,       change_to_previous_workspace, {.i=0}   },
    /* change workspace */
     DESKTOPCHANGE(     XK_1,                          0)
     DESKTOPCHANGE(     XK_2,                          1)
     DESKTOPCHANGE(     XK_3,                          2)
     DESKTOPCHANGE(     XK_4,                          3)
     DESKTOPCHANGE(     XK_5,                          4)
     DESKTOPCHANGE(     XK_6,                          5)
     DESKTOPCHANGE(     XK_7,                          6)
     DESKTOPCHANGE(     XK_8,                          7)
     DESKTOPCHANGE(     XK_9,                          8)
     DESKTOPCHANGE(     XK_0,                          9)
};
