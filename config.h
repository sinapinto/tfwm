/* find using `xmodmap`
 * generally: mod 1 = Alt, mod 4 = Windows key*/
#define MOD     XCB_MOD_MASK_1
#define SHIFT   XCB_MOD_MASK_SHIFT
#define CTRL    XCB_MOD_MASK_CONTROL

/* 0: move step, 1: resize step
 * i.e. windows will move by 40px and resize by 40px */
uint8_t steps[2] = { 40, 40 };

#define UNFOCUS      0xafaf87  /* unfocused window border color */
#define FOCUS        0x52C74C  /* focused window border color */
#define BORDER_WIDTH 2

#define DESKTOPCHANGE(K,N) \
{  MOD ,             K,              change_workspace, {.i=N}}, \
{  MOD | SHIFT,      K,              client_to_workspace, {.i=N}},

static const char *term[] = { "urxvt", NULL };  

static workspace workspaces[5]; /* number of workspaces - if you change this make sure to delete the corresponding DESKTOPCHANGE keybind below */

static key keys[] = {
   /* mod               keycode         function            arg   */
    /* spawn a new terminal (specified in the term[] array above) */
    { MOD ,             XK_Return,      spawn,              {.com=term} },
    /* move the window down */
    { MOD,              XK_j,           move,               {.i=0}   },
    /* move the window up */
    { MOD,              XK_k,           move,               {.i=1}   },
    /* move the window left */
    { MOD,              XK_h,           move,               {.i=2}   },
    /* move the window right */
    { MOD,              XK_l,           move,               {.i=3}   },
    /* grow the window vertically */
    { MOD | SHIFT,      XK_j,           resize,             {.i=0}   },
    /* shrink the window vertically */
    { MOD | SHIFT,      XK_k,           resize,             {.i=1}   },
    /* shrink the window horizontally */
    { MOD | SHIFT,      XK_h,           resize,             {.i=2}   },
    /* grow the window horizontally */
    { MOD | SHIFT,      XK_l,           resize,             {.i=3}   },
    /* grow the window maintaining the aspect ratio */
    { MOD | CTRL,       XK_j,           resize,             {.i=4}   },
    /* srhink the window maintaining the aspect ratio */
    { MOD | CTRL,       XK_k,           resize,             {.i=5}   },
    /* fullscreen the window (or unfullscreen it) */
    { MOD,              XK_a,           toggle_maximize,    {.i=0}   },
    /* focus the next window */
    { MOD,              XK_Tab,         nextwin,            {.i=0}   },
    /* focus the previous window */
    { MOD | SHIFT,      XK_Tab,         nextwin,            {.i=1}   },
    /* kill the focused window */
    { MOD | SHIFT,      XK_q,           killwin,            {.i=0}   },
    /* exit bwm */
    { MOD | SHIFT,      XK_e,           cleanup,            {.i=0}   },
    /* switch to various workspaces */
     DESKTOPCHANGE(     XK_1,                          0)
     DESKTOPCHANGE(     XK_2,                          1)
     DESKTOPCHANGE(     XK_3,                          2)
     DESKTOPCHANGE(     XK_4,                          3)
     DESKTOPCHANGE(     XK_5,                          4)
};
