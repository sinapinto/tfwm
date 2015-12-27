/* See LICENSE file for copyright and license details. */
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include "tfwm.h"
#include "events.h"
#include "pointer.h"
#include "keys.h"
#include "client.h"
#include "workspace.h"
#include "util.h"

const Rule rules[RULE_MAX] = {
	/* class           workspace       fullscreen       border */
	{ "chromium",      1,              false,           false  },
	{ "firefox",       0,              false,           false  },
};

static char *terminal[]  = { "urxvt", NULL };
static char *terminal2[] = { "termite", NULL };
static char *browser[]   = { "chromium", NULL };
static char *browser2[]  = { "firefox", NULL };
static char *launcher[]  = { "rofi", "-show", "run",  NULL };
static char *mpctoggle[] = { "mpc", "-q", "toggle", NULL };
static char *mpcseekf[]  = { "mpc", "-q", "seek", "+30", NULL };
static char *mpcseekb[]  = { "mpc", "-q", "seek", "-30", NULL };
static char *mpcnext[]   = { "mpc", "-q", "next", NULL };
static char *mpcprev[]   = { "mpc", "-q", "prev", NULL };
static char *volup[]     = { "amixer", "-q", "set", "Master", "3%+", "unmute", NULL };
static char *voldown[]   = { "amixer", "-q", "set", "Master", "3%-", "unmute", NULL };
static char *voltoggle[] = { "amixer", "-q", "set", "Master", "toggle", NULL };

#define MOD    XCB_MOD_MASK_1
#define SHIFT  XCB_MOD_MASK_SHIFT
#define CTRL   XCB_MOD_MASK_CONTROL
Key keys[KEY_MAX] = {
	/* modifier     key                       function       argument */
	{ MOD,          XK_Return,                spawn,         {.com=terminal}    },
	{ MOD,          XK_t,                     spawn,         {.com=terminal2}   },
	{ MOD,          XK_w,                     spawn,         {.com=browser}     },
	{ MOD,          XK_e,                     spawn,         {.com=browser2}    },
	{ MOD,          XK_space,                 spawn,         {.com=launcher}    },
	{ MOD,          XK_p,                     spawn,         {.com=mpctoggle}   },
	{ XCB_NONE,     XK_Pause,                 spawn,         {.com=mpcseekf}    },
	{ XCB_NONE,     XK_Print,                 spawn,         {.com=mpcseekb}    },
	{ MOD,          XK_o,                     spawn,         {.com=mpcnext}     },
	{ MOD,          XK_i,                     spawn,         {.com=mpcprev}     },
	{ XCB_NONE,     XK_F2,                    spawn,         {.com=volup}       },
	{ XCB_NONE,     XK_F1,                    spawn,         {.com=voldown}     },
	{ XCB_NONE,     XF86XK_AudioRaiseVolume,  spawn,         {.com=volup}       },
	{ XCB_NONE,     XF86XK_AudioLowerVolume,  spawn,         {.com=voldown}     },
	{ XCB_NONE,     XF86XK_AudioMute,         spawn,         {.com=voltoggle}   },
	{ MOD | SHIFT,  XK_j,                     resize,        {.i=GrowHeight}    },
	{ MOD | SHIFT,  XK_l,                     resize,        {.i=GrowWidth}     },
	{ MOD | SHIFT,  XK_k,                     resize,        {.i=ShrinkHeight}  },
	{ MOD | SHIFT,  XK_h,                     resize,        {.i=ShrinkWidth}   },
	{ MOD | CTRL,   XK_j,                     resize,        {.i=GrowBoth}      },
	{ MOD | CTRL,   XK_k,                     resize,        {.i=ShrinkBoth}    },
	{ MOD,          XK_Tab,                   cycleclients,  {.i=PrevWindow}    },
	{ MOD | SHIFT,  XK_Tab,                   cycleclients,  {.i=NextWindow}    },
	{ MOD,          XK_s,                     teleport,      {.i=Center}        },
	{ MOD,          XK_y,                     teleport,      {.i=TopLeft}       },
	{ MOD,          XK_u,                     teleport,      {.i=TopRight}      },
	{ MOD,          XK_b,                     teleport,      {.i=BottomLeft}    },
	{ MOD,          XK_n,                     teleport,      {.i=BottomRight}   },
	{ MOD,          XK_x,                     maximize,      {.i=0}          },
	{ MOD,          XK_m,                     maximizeaxis,  {.i=MaxVertical}   },
	{ MOD | SHIFT,  XK_m,                     maximizeaxis,  {.i=MaxHorizontal} },
	{ MOD,          XK_q,                     killselected,  {.i=0}          },
	{ MOD,          XK_grave,                 selectrws,     {.i=LastWorkspace} },
	{ MOD,          XK_bracketleft,           selectrws,     {.i=PrevWorkspace} },
	{ MOD,          XK_bracketright,          selectrws,     {.i=NextWorkspace} },
	{ MOD | SHIFT,  XK_r,                     restart,       {.i=0}          },
	{ MOD | SHIFT,  XK_e,                     quit,          {.i=0}          },
#define WORKSPACE(K,N) \
	{ MOD,              K,                    selectws,      {.i=N} }, \
	{ MOD | SHIFT,      K,                    sendtows,      {.i=N} },
	WORKSPACE(      XK_1,                                    0 )
	WORKSPACE(      XK_2,                                    1 )
	WORKSPACE(      XK_3,                                    2 )
	WORKSPACE(      XK_4,                                    3 )
	WORKSPACE(      XK_5,                                    4 )
	WORKSPACE(      XK_6,                                    5 )
	WORKSPACE(      XK_7,                                    6 )
	WORKSPACE(      XK_8,                                    7 )
	WORKSPACE(      XK_9,                                    8 )
	WORKSPACE(      XK_0,                                    9 )
	{ MOD,          XK_k,                     move,          {.i=MoveUp}        },
	{ MOD,          XK_j,                     move,          {.i=MoveDown}      },
	{ MOD,          XK_h,                     move,          {.i=MoveLeft}      },
	{ MOD,          XK_l,                     move,          {.i=MoveRight}     },
};

Button buttons[BUTTON_MAX] = {
	{  MOD,          XCB_BUTTON_INDEX_1,      mousemotion,   {.i=MouseMove}     },
	{  MOD,          XCB_BUTTON_INDEX_3,      mousemotion,   {.i=MouseResize}   }
};


xcb_keycode_t *
getkeycodes(xcb_keysym_t keysym) {
	xcb_key_symbols_t *keysyms;
	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err("can't get keycode.");
	xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
	xcb_key_symbols_free(keysyms);
	return keycode;
}

xcb_keysym_t
getkeysym(xcb_keycode_t keycode) {
	xcb_key_symbols_t *keysyms;

	if (!(keysyms = xcb_key_symbols_alloc(conn)))
		err("can't get keysym.");

	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
	xcb_key_symbols_free(keysyms);
	return keysym;
}

void
grabkeys(void) {
	unsigned int   i, j, k;
	xcb_keycode_t *keycode;
	uint16_t       modifiers[4];

	modifiers[0] = 0;
	modifiers[1] = XCB_MOD_MASK_LOCK;
	modifiers[2] = numlockmask;
	modifiers[3] = numlockmask | XCB_MOD_MASK_LOCK;

	xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);

	for (i = 0; i < LENGTH(keys); ++i) {
		keycode = getkeycodes(keys[i].keysym);

		for (j = 0; keycode[j] != XCB_NO_SYMBOL; j++) {
			for (k = 0; k < LENGTH(modifiers); k++) {
				xcb_grab_key(conn, 1, screen->root,
					     keys[i].mod | modifiers[k],
					     keycode[j], XCB_GRAB_MODE_ASYNC,
					     XCB_GRAB_MODE_ASYNC);
			}
		}

		FREE(keycode);
	}
}

void
updatenumlockmask(void) {
	unsigned int                       i, j, n;
	xcb_keycode_t                     *modmap;
	xcb_get_modifier_mapping_reply_t  *mmr;
	xcb_get_modifier_mapping_cookie_t  mmc;
	xcb_keycode_t                      keycode;

	numlockmask = 0;

	mmc = xcb_get_modifier_mapping(conn);
	mmr = xcb_get_modifier_mapping_reply(conn, mmc, NULL);
	if (!mmr)
		err("mod map mmr");

	modmap = xcb_get_modifier_mapping_keycodes(mmr);
	if (!modmap)
		err("mod map keycodes");

	xcb_keycode_t *numlock = getkeycodes(XK_Num_Lock);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < mmr->keycodes_per_modifier; j++) {
			keycode = modmap[i * mmr->keycodes_per_modifier + j];
			if (keycode == XCB_NO_SYMBOL)
				continue;
			if (numlock)
				for (n = 0; numlock[n] != XCB_NO_SYMBOL; n++)
					if (numlock[n] == keycode) {
						numlockmask = 1 << i;
						break;
					}
		}
	}
	FREE(mmr);
	FREE(numlock);
}

