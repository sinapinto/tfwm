#ifndef _CONFIG_H
#define _CONFIG_H

/* find using `xmodmap`*/
#define MOD     XCB_MOD_MASK_1
#define SHIFT   XCB_MOD_MASK_SHIFT

/* 0: move, 1: resize */
uint8_t steps[2] = { 40, 40 };

#define NORMCOLOR    0x002933  /* unfocused window border color */
#define SELCOLOR     0xafaf87  /* focused window border color */
#define BORDER_WIDTH 2         /* border width in pixels */

/* find keysyms using `xev` */
static key keys[] = {
   /* MOD               KEYSYM          FUNCTION     arg */
    { MOD,              44, /*j*/       move,        {.i=0}   },
    { MOD,              45, /*k*/       move,        {.i=1}   },
    { MOD,              43, /*h*/       move,        {.i=2}   },
    { MOD,              46, /*l*/       move,        {.i=3}   },
    { MOD | SHIFT,      44, /*j*/       resize,      {.i=0}   },
    { MOD | SHIFT,      45, /*k*/       resize,      {.i=1}   },
    { MOD | SHIFT,      43, /*h*/       resize,      {.i=2}   },
    { MOD | SHIFT,      46, /*l*/       resize,      {.i=3}   },
    { MOD,              38, /*a*/       maximize,    {.i=0}   },
    { MOD,              23, /*Tab*/     nextwin,     {.i=0}   },
    { MOD | SHIFT,      23, /*Tab*/     nextwin,     {.i=1}   },
    { MOD | SHIFT,      24, /*q*/       killwin,     {.i=0}   },
    { MOD | SHIFT,      26, /*e*/       cleanup,     {.i=0}   },
};

#endif
