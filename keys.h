/* See LICENSE file for copyright and license details. */
#ifndef KEYS_H
#define KEYS_H

xcb_keycode_t *getkeycodes(xcb_keysym_t keysym);
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
void grabkeys(void);
void updatenumlockmask(void);

extern unsigned int numlockmask;
extern const Rule rules[RULE_MAX];
extern Key keys[KEY_MAX];
extern Button buttons[BUTTON_MAX];

#endif
