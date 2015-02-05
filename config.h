/* find using `xmodmap`*/
#define MOD     XCB_MOD_MASK_1
#define SHIFT   XCB_MOD_MASK_SHIFT

/* 0: move step, 1: resize step */
uint8_t steps[2] = { 40, 40 };

#define NORMCOLOR    0xafaf87  /* unfocused window border color */
#define SELCOLOR     0x52C74C  /* focused window border color */
#define BORDER_WIDTH 2

#define DESKTOPCHANGE(K,N) \
{  MOD ,             K,              change_workspace, {.i=N}}, \
{  MOD | SHIFT,      K,              client_to_workspace, {.i=N}},

static const char *term[] = { "urxvt", NULL };

static workspace workspaces[6]; /* number of workspaces */

/* find keycodes using `xev` */
static key keys[] = {
   /* mod               keycode         function            arg */
    { MOD ,             36, /*Return*/  spawn,              {.com=term} },
    { MOD,              44, /*j*/       move,               {.i=0}   },
    { MOD,              45, /*k*/       move,               {.i=1}   },
    { MOD,              43, /*h*/       move,               {.i=2}   },
    { MOD,              46, /*l*/       move,               {.i=3}   },
    { MOD | SHIFT,      44, /*j*/       resize,             {.i=0}   },
    { MOD | SHIFT,      45, /*k*/       resize,             {.i=1}   },
    { MOD | SHIFT,      43, /*h*/       resize,             {.i=2}   },
    { MOD | SHIFT,      46, /*l*/       resize,             {.i=3}   },
    { MOD | SHIFT,      58, /*m*/       resize,             {.i=4}   },
    { MOD | SHIFT,      57, /*n*/       resize,             {.i=5}   },
    { MOD,              38, /*a*/       toggle_maximize,    {.i=0}   },
    { MOD,              23, /*Tab*/     nextwin,            {.i=0}   },
    { MOD | SHIFT,      23, /*Tab*/     nextwin,            {.i=1}   },
    { MOD | SHIFT,      24, /*q*/       killwin,            {.i=0}   },
    { MOD | SHIFT,      26, /*e*/       cleanup,            {.i=0}   },
     DESKTOPCHANGE(     10, /*1*/                            0)
     DESKTOPCHANGE(     11, /*2*/                            1)
     DESKTOPCHANGE(     12, /*3*/                            2)
     DESKTOPCHANGE(     13, /*4*/                            3)
     DESKTOPCHANGE(     14, /*5*/                            4)
     DESKTOPCHANGE(     15, /*6*/                            5)
};
