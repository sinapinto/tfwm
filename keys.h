#ifndef KEYS_H
#define KEYS_H

xcb_keycode_t * getkeycodes(xcb_keysym_t keysym);
xcb_keysym_t getkeysym(xcb_keycode_t keycode);
void grabkeys(void);
void updatenumlockmask(void);

#endif

