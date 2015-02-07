/* find using `xmodmap`
 * generally: mod 1 = Alt, mod 4 = Windows key*/
#define MOD     XCB_MOD_MASK_1
#define SHIFT   XCB_MOD_MASK_SHIFT

/* 0: move step, 1: resize step
 * i.e. windows will move by 40px and resize by 40px */
uint8_t steps[2] = { 40, 40 };

#define UNFOCUS    0xafaf87  /* unfocused window border color */
#define FOCUS     0x52C74C  /* focused window border color */
#define BORDER_WIDTH 2

#define DESKTOPCHANGE(K,N) \
{  MOD ,             K,              change_workspace, {.i=N}}, \
{  MOD | SHIFT,      K,              client_to_workspace, {.i=N}},

static const char *term[] = { "urxvt", NULL };  

static workspace workspaces[5]; /* number of workspaces - if you change this make sure to delete the corresponding DESKTOPCHANGE keybing below */

/* find keycodes using `xev` */
static key keys[] = {
   /* mod               keycode         function            arg */
    { MOD ,             XK_Return,      spawn,              {.com=term} },
    { MOD,              XK_j,           move,               {.i=0}   },
    { MOD,              XK_k,           move,               {.i=1}   },
    { MOD,              XK_h,           move,               {.i=2}   },
    { MOD,              XK_l,           move,               {.i=3}   },
    { MOD | SHIFT,      XK_j,           resize,             {.i=0}   },
    { MOD | SHIFT,      XK_k,           resize,             {.i=1}   },
    { MOD | SHIFT,      XK_h,           resize,             {.i=2}   },
    { MOD | SHIFT,      XK_l,           resize,             {.i=3}   },
    { MOD | SHIFT,      XK_m,           resize,             {.i=4}   },
    { MOD | SHIFT,      XK_n,           resize,             {.i=5}   },
    { MOD,              XK_a,           toggle_maximize,    {.i=0}   },
    { MOD,              XK_Tab,         nextwin,            {.i=0}   },
    { MOD | SHIFT,      XK_Tab,         nextwin,            {.i=1}   },
    { MOD | SHIFT,      XK_q,           killwin,            {.i=0}   },
    { MOD | SHIFT,      XK_e,           cleanup,            {.i=0}   },
     DESKTOPCHANGE(     XK_1,                          0)
     DESKTOPCHANGE(     XK_2,                          1)
     DESKTOPCHANGE(     XK_3,                          2)
     DESKTOPCHANGE(     XK_4,                          3)
     DESKTOPCHANGE(     XK_5,                          4)
};
