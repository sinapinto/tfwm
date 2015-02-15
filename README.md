bwm - bandit window manager
===========================
bwm is a fast, lightweight, floating window manager for X.

Current Features
----------------
* Centered mode
* Automatic window refocusing
* Resizing keeping aspect ratio
* Focus without stacking
* Maximize toggling
* Multiple workspaces
* 100% keyboard driven

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

Configuration is done throught the `config.h` file. should be straightforward.

bwm does not come with a panel.  However, if you want to use your own, bwm
supports the EWMH hints needed to easily find the workspace. Here's a simple example using [dzen2](http://github.com/robm/dzen).

```sh
#!/bin/sh
while :; do
    line=
    current=`xprop -root _NET_CURRENT_DESKTOP | awk '{print $3}'`
    total=`xprop -root _NET_NUMBER_OF_DESKTOPS | awk '{print $3}'`
    for i in `seq 1 $current`; do line="${line} $i "; done
    line="${line}^bg(#333333) $((current + 1)) ^bg()"
    for i in `seq $((current + 2)) $total`; do line="${line} $i "; done
    echo "$line"
    sleep 1
done | dzen2 -p

```

Demo
----
![gif](https://u.teknik.io/62xp0W.gif)

Author
------
Sina Pinto | sina (dot) pinto (at) gmail (dot) com
