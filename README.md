tfwm is a minimal floating, reparenting window manager for X.

# Installation
After cloning the repository, run (as root, if necessary) `make install` to
compile the program and install the executable. This will also install a man
page accessible by running `man tfwm`.

By default, files are installed in `/usr/local`. You can change the installation
directory with, for example, `make PREFIX=/some/path install`.

# Configuration
Configuration settings are documented in `doc/Configuration`. An example
configuration file can be found in the `examples/` subdirectory.

# Compliance
tfwm focuses on EWMH compliance and only supports a few ICCCM properties. For
a list of supported properties, see `doc/Compliance`.

# Todo
- Remanage windows after restart.
- Improve support for window class based rules.
- Add the ability to define exec keybinds in the config.
- Support for various EWMH atoms.
- Explore optional pixmap borders and shaped windows.
- Improve config file parsing:
    - Spaces in keybind values
    - Inline comments?
- Much more..
