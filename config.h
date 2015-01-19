#ifndef _CONFIG_H
#define _CONFIG_H

/* find using `xmodmap`*/
#define MOD     XCB_MOD_MASK_1
#define SHIFT   XCB_MOD_MASK_SHIFT

#define NORMCOLOR    0xafaf87  /* unfocused window border color */
#define SELCOLOR     0x2aa198  /* focused window border color */
#define BORDER_WIDTH 4         /* border width in pixels */

/* find keysyms using `xev` */
static Key keys[] = {
   /* MOD               KEYSYM          FUNCTION       X    Y     */
    { MOD,              44, /*j*/       move,          0,   50    },
    { MOD,              45, /*k*/       move,          0,  -50    },
    { MOD,              43, /*h*/       move,        -50,   0     },
    { MOD,              46, /*l*/       move,         50,   0     },
    { MOD | SHIFT,      44, /*j*/       resize,        0,   50    },
    { MOD | SHIFT,      45, /*k*/       resize,        0,  -50    },
    { MOD | SHIFT,      43, /*h*/       resize,      -50,   0     },
    { MOD | SHIFT,      46, /*l*/       resize,       50,   0     },
    { MOD,              38, /*a*/       maximize,      0,   0     },
    { MOD,              23, /*Tab*/     nextwin,       0,   0     },
    { MOD | SHIFT,      24, /*q*/       killwin,       0,   0     },
    { MOD | SHIFT,      26, /*e*/       cleanup,       0,   0     },
};

#endif
