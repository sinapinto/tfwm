bwm - bandit window manager
===========================
bwm is a fast, lightweight, floating window manager for X.

Current Features
----------------
* Automatic window refocusing
* Resizing keeping aspect ratio
* Maximize toggling
* Multiple workspaces
* 100% keyboard driven
... more to come

Installation
------------
If you don't already have xcb on your system you will need to install it.

On Debian-based distributions:
`sudo apt-get install xcb libxcb-util0-dev libxcb-keysyms1`

On Arch-based distributions:
`sudo pacman -S libxcb xcb-util xcb-util-keysyms`

Edit Makefile and config.h to your needs (bwm is installed into /usr/bin by default).

    $ git clone git://github.com/sinapinto/bwm.git
    $ cd bwm
    $ make
    # make install

Running bwm
-----------
Add the following line to your `~/.xinitrc` to start bwm using `startx`:

    exec bwm

By default, bwm does not set an X cursor, so the "cross" cursor is usually displayed.  To set the usual left-pointer, add the following line to your `~/.xinitrc`:

    xsetroot -cursor_name left_ptr

Exiting bwm
-----------
To end the current bwm session, press `Mod+Shift+E`. By default, `Mod` is the `Alt` key.

Configuration
-------------
Configuration of bwm is done by modifying `config.h` and recompiling the source code.

Note: To change a keybind, you can find the keycode number using the `xev` command.

Author
------
Sina Pinto | sina (dot) pinto (at) gmail (dot) com
