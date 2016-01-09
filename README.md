tfwm is a small, floating window manager for X.

## Installation
Dependencies:
* libx11
* xcb-util
* xcb-util-wm
* xcb-util-keysyms

After cloning the repository, run (as root, if necessary) `make install` to
compile the program and install the executable. This will also install a man
page accessible by running `man tfwm`.

By default, files are installed in `/usr/local`. You can change the installation
directory with, for example, `make PREFIX=/some/path install`.

## Configuration
Configuration settings are documented in `doc/Configuration`. An example
configuration file can be found in the `examples/` subdirectory.

## Compliance
tfwm focuses on EWMH compliance and only supports a few ICCCM properties. For
a list of supported properties, see `doc/Compliance`.

## Todo
* Remanage windows after restart.
* Support for various EWMH atoms.
* Maybe a statusbar
* Explore optional pixmap borders and shaped windows.
* Improve support for window class based rules.
* Add the ability to define exec keybinds in the config.
* Improve config file parsing:
    * Spaces in keybind values
    * Inline comments?
* Much more..

## License
MIT/X Consortium License
