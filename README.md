bwm
===
bwm is a minimalist, keyboard-centric, floating window manager for X

this has...
----------------
* A cool "centered mode"
* Automatic window refocusing
* Focus without stacking
* Fullscreen toggling
* Vertical maximize toggling
* Virtual workspaces
* Switch to previous workspace
* Decent EWMH/ICCCM compliance

it does NOT have
-------------------
* Support for multiple screens
* A panel
* Stability

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

    exec bwm

By default, bwm does not set an X cursor, so the "cross" cursor is usually displayed.
To set the usual left-pointer, add the following line to your `~/.xinitrc`:

    xsetroot -cursor_name left_ptr

Author
------
Sina Pinto | sina.pinto@gmail.com


Licensed under MIT/X Consortium License
