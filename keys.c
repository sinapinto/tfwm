/* See LICENSE file for copyright and license details. */
#include <X11/keysym.h>
#include "tfwm.h"
#include "events.h"
#include "pointer.h"
#include "keys.h"
#include "util.h"

const Rule rules[RULE_MAX] = {
	/* class           workspace       fullscreen       border */
	{ "chromium",      1,              false,           false  },
	{ "firefox",       0,              false,           false  },
};

#define MOD    XCB_MOD_MASK_1
Key keys[KEY_MAX];
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
				/* PRINTF("grabkeys: key: %u, mod %d\n", */
				/*        keycode[j], keys[i].mod); */
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

