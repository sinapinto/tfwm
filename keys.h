/* See LICENSE file for copyright and license details. */
#ifndef KEYS_H
#define KEYS_H

#define KEY_MAX    65
#define RULE_MAX   2
#define BUTTON_MAX 2

xcb_keycode_t * getkeycodes(xcb_keysym_t keysym);
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
void grabkeys(void);
void updatenumlockmask(void);

#endif

