bwm - bandit window manager
===========================
bwm is an extremely fast, lightweight, floating window manager for X.

Features
--------
* Automatic window refocusing
* Multiple workspaces (currently buggy)
* Window resizing keeping aspect ratio
* Window maximize toggling
* 100% keyboard driven

Installation
------------
If you don't already have xcb on your system you will need to install it.

On Debian-based distributions:
`sudo apt-get install xcb libxcb-util0-dev`

On Arch-based distributions:
`sudo pacman -S libxcb xcb-util`

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

TODO
-----
bwm has some minor bugs for now, mostly to do with multiple workspaces:

* windows sometimes become unable to focus until switching workspace.
* windows sometimes incorrectly appear on multiple workspaces
* windows changing focus ordering when switching workspaces
* "XIO: fatal IO error..."

Any contributions are welcome.  Feel free to open a pull request, or open an issue if you find other bugs.

Author
------
Sina Pinto | sina (dot) pinto (at) gmail (dot) com
