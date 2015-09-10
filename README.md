bwm
===
bwm is a minimalist, keyboard-centric, floating window manager for X

*NB:* this program is not well tested and it is entirely possible that it will crash X. It is suitable for everyday use, though.

features
----------------
* Centered windows (toggleable)
* Fullscreen toggling
* Vertical maximize toggling
* Virtual workspaces
* Switch to previous workspace
* Automatic window refocusing
* Focus without changing stacking order
* Decent EWMH/ICCCM compliance
* Limited mouse support

Installation
------------
If you don't already have the XCB library on your system you will need to install it.

On Debian-based distributions:
`sudo apt-get install xcb libxcb-util0-dev libxcb-keysyms1`

On Arch-based distributions:
`sudo pacman -S libxcb xcb-util xcb-util-keysyms`

When installing bwm, edit Makefile and config.h to your needs (the binary is installed into /usr/local/bin by default).

    $ git clone git://github.com/sinapinto/bwm.git
    $ cd bwm
    $ make
    # make install

Running bwm
-----------
Add the following line to your `~/.xinitrc` to start bwm from the console using `startx`:

    bwm

By default, bwm does not set an X cursor, so the "cross" cursor is usually displayed.
To set the usual left-pointer, add the following line to your `~/.xinitrc`:

    xsetroot -cursor_name left_ptr

why do i use it
---------------
A majority of window managers are focused around window tiling. Once I started using tmux (which provides its own tiling inside terminals), i no longer needed tiling at the wm level and realized that a floating wm would be more suitable. Some window managers are "dynamic" and have a floating mode along with tiling and other modes, which is great, but their floating modes are generally primitive.  For example, windows in dwm's floating mode cannot be resized using the keyboard.

Most floating wm's I tried had the annoyance of not re-focusing windows.  When you killed a window, nothing would be focused and you'd need hit a keybind to regain focus on another window.  I found this to be inefficient and, while it probably would have been much easier to patch this functionality into an existing wm, I decided to make my own.

If you use it and find a bug, feel free to submit an issue and i'll try my best to fix it.
