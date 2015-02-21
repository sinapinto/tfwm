bwm
===
bwm is a minimalist, keyboard-centric, floating window manager for X.

Current Features
----------------
* Centered mode
* Automatic refocusing
* Focus without stacking
* Resize keeping aspect ratio
* Maximize toggling
* Multiple workspaces

### Centered Mode
It isn't really a mode, it's more of an attribute.  A 'centered' window will stay fixed in the middle of the screen even when it is resized.  When a centered window is moved it will lose it's centered attribute until it is re-activated.  You can configure whether windows start out in centered mode with the `CENTER_BY_DEFAULT` macro in config.h

### Automatic refocusing
Something that bugged me when using some other floating window managers was focus being lost after killing a window, making the user have to alt-tab or whatever to focus the next window. bwm does this for you.

### Focus without stacking
As easy as this is to implement, most floating window managers don't have this feature.  Focusing a window without changing the stacking order means that your current window won't get covered up by whatever you are changing focus to.  This is useful when you have a large window and a small window in front of it, and you want to switch focus to the large one, without covering up the smaller one, if that makes any sense.  if not, just try it and you will understand.

Installation
------------
If you don't already have the XCB library on your system you will need to install it.

On Debian-based distributions:
`sudo apt-get install xcb libxcb-util0-dev libxcb-keysyms1`

On Arch-based distributions:
`sudo pacman -S libxcb xcb-util xcb-util-keysyms`

When installing bwm, edit Makefile and config.h to your needs (the binary is installed into /usr/bin by default).

    $ git clone git://github.com/sinapinto/bwm.git
    $ cd bwm
    $ make
    # make install

Running bwm
-----------
Add the following line to your `~/.xinitrc` to start bwm using `startx`:

    exec bwm

By default, bwm does not set an X cursor, so the "cross" cursor is usually displayed.
To set the usual left-pointer, add the following line to your `~/.xinitrc`:

    xsetroot -cursor_name left_ptr

Configuration is done throught the `config.h` file. The documentation in there should make configuration pretty straightforward.

bwm does not come with a panel.  However, if you want to use your own, bwm supports the EWMH hints needed to easily find the workspace (and some others, too). Here's a simple example using [dzen2](http://github.com/robm/dzen).

```sh
#!/bin/sh
while :; do
    line=
    current=`xprop -root _NET_CURRENT_DESKTOP | awk '{print $3}'`
    total=`xprop -root _NET_NUMBER_OF_DESKTOPS | awk '{print $3}'`
    for i in `seq 1 $current`; do line="$line $i "; done
    line="${line}^bg(#333333) $((current + 1)) ^bg()"
    for i in `seq $((current + 2)) $total`; do line="$line $i "; done
    echo "$line"
    sleep 1
done | dzen2 -p
```

bwm also does not have multi screen support.  I don't own an external monitor, so, unless I obtain one, I will/can not add support.

Author
------
Sina Pinto | sina.pinto@gmail.com
