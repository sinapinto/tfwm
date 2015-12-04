/* See LICENSE file for copyright and license details. */
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include "tfwm.h"
#include "list.h"
#include "client.h"
#include "events.h"
#include "workspace.h"

const Rule rules[2] = {
	/* class           workspace       fullscreen       border */
	{ "chromium",      1,              false,           false  },
	{ "firefox",       0,              false,           false  },
};

static const char *terminal[]  = { "urxvt", NULL };
static const char *terminal2[] = { "termite", NULL };
static const char *browser[]   = { "chromium", NULL };
static const char *browser2[]  = { "firefox", NULL };
static const char *launcher[]  = { "rofi", "-show", "run",  NULL };
static const char *mpctoggle[] = { "mpc", "toggle", NULL };
static const char *mpcseekf[]  = { "mpc", "seek", "+30", NULL };
static const char *mpcseekb[]  = { "mpc", "seek", "-30", NULL };
static const char *mpcnext[]   = { "mpc", "next", NULL };
static const char *mpcprev[]   = { "mpc", "prev", NULL };
static const char *volup[]     = { "amixer", "-q", "set", "Master", "3%+", "unmute", NULL };
static const char *voldown[]   = { "amixer", "-q", "set", "Master", "3%-", "unmute", NULL };
static const char *voltoggle[] = { "amixer", "-q", "set", "Master", "toggle", NULL };

#define MOD					XCB_MOD_MASK_1
#define SHIFT				XCB_MOD_MASK_SHIFT
#define CTRL				XCB_MOD_MASK_CONTROL
Key keys[62] = {
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
	{ MOD,          XK_j,                     move,          {.i=MoveDown}      },
	{ MOD,          XK_l,                     move,          {.i=MoveRight}     },
	{ MOD,          XK_k,                     move,          {.i=MoveUp}        },
	{ MOD,          XK_h,                     move,          {.i=MoveLeft}      },
	{ MOD | SHIFT,  XK_j,                     resize,        {.i=GrowHeight}    },
	{ MOD | SHIFT,  XK_l,                     resize,        {.i=GrowWidth}     },
	{ MOD | SHIFT,  XK_k,                     resize,        {.i=ShrinkHeight}  },
	{ MOD | SHIFT,  XK_h,                     resize,        {.i=ShrinkWidth}   },
	{ MOD | CTRL,   XK_j,                     resize,        {.i=GrowBoth}      },
	{ MOD | CTRL,   XK_k,                     resize,        {.i=ShrinkBoth}    },
	{ MOD,          XK_Tab,                   cycleclients,  {.i=PrevWindow}    },
	{ MOD | SHIFT,  XK_Tab,                   cycleclients,  {.i=NextWindow}    },
	{ MOD,          XK_s,                     teleport,      {.i=ToCenter}      },
	{ MOD | SHIFT,  XK_p,                     teleport,      {.i=ToTop}         },
	{ MOD | SHIFT,  XK_n,                     teleport,      {.i=ToBottom}      },
	{ MOD | SHIFT,  XK_y,                     teleport,      {.i=ToLeft}        },
	{ MOD | SHIFT,  XK_u,                     teleport,      {.i=ToRight}       },
	{ MOD,          XK_a,                     maximize,      {.i=NULL}          },
	{ MOD,          XK_m,                     maximizeaxis,  {.i=MaxVertical}   },
	{ MOD,          XK_n,                     maximizeaxis,  {.i=MaxHorizontal} },
	{ MOD | CTRL,   XK_s,                     sticky,        {.i=NULL}          },
	{ MOD,          XK_q,                     killselected,  {.i=NULL}          },
	{ MOD,          XK_grave,                 selectrws,     {.i=LastWorkspace} },
	{ MOD,          XK_bracketleft,           selectrws,     {.i=PrevWorkspace} },
	{ MOD,          XK_bracketright,          selectrws,     {.i=NextWorkspace} },
	{ MOD | SHIFT,  XK_r,                     restart,       {.i=NULL}          },
	{ MOD | SHIFT,  XK_e,                     quit,          {.i=NULL}          },
#define workspace(K,N) \
	{ MOD,              K,                    selectws,      {.i=N} }, \
	{ MOD | SHIFT,      K,                    sendtows,      {.i=N} },
	workspace(      XK_1,                                    0 )
	workspace(      XK_2,                                    1 )
	workspace(      XK_3,                                    2 )
	workspace(      XK_4,                                    3 )
	workspace(      XK_5,                                    4 )
	workspace(      XK_6,                                    5 )
	workspace(      XK_7,                                    6 )
	workspace(      XK_8,                                    7 )
	workspace(      XK_9,                                    8 )
	workspace(      XK_0,                                    9 )
#undef workspace
};

Button buttons[2] = {
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
	unsigned int i, j, k;
	unsigned int modifiers[] = { 0, XCB_MOD_MASK_LOCK, numlockmask, numlockmask|XCB_MOD_MASK_LOCK };
	xcb_keycode_t *keycode;

	xcb_ungrab_key(conn, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);
	for (i = 0; i < LENGTH(keys); ++i) {
		keycode = getkeycodes(keys[i].keysym);
		for (j = 0; keycode[j] != XCB_NO_SYMBOL; j++)
			for (k = 0; k < LENGTH(modifiers); k++)
				xcb_grab_key(conn, 1, screen->root, keys[i].mod | modifiers[k],
						keycode[j], XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
		free(keycode);
	}
}

void
updatenumlockmask(void) {
	unsigned int i, j, n;
	numlockmask = 0;
	xcb_keycode_t *modmap ;
	xcb_get_modifier_mapping_reply_t *reply;

	reply = xcb_get_modifier_mapping_reply(conn, xcb_get_modifier_mapping_unchecked(conn), NULL);
	if (!reply)
		err("mod map reply");
	modmap = xcb_get_modifier_mapping_keycodes(reply);
	if (!modmap)
		err("mod map keycodes");

	xcb_keycode_t *numlock = getkeycodes(XK_Num_Lock);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < reply->keycodes_per_modifier; j++) {
			xcb_keycode_t keycode = modmap[i * reply->keycodes_per_modifier + j];
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
	free(reply);
	free(numlock);
}

