UNAME := $(shell uname -s)
__WM_NAME__ = tfwm
__WM_VERSION__ = $(shell git describe --tags 2>/dev/null)
ifeq ($(__WM_VERSION__),)
  __WM_VERSION__ = 0.1
endif

PREFIX   ?= /usr/local
MANPREFIX = $(PREFIX)/share/man
BINPREFIX = $(PREFIX)/bin

CC     ?= gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wshadow -Wno-uninitialized -pedantic -I$(PREFIX)/include \
	  -D__WM_VERSION__=\"$(__WM_VERSION__)\" \
	  -D__WM_NAME__=\"$(__WM_NAME__)\"
LIBS    = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-util -lxcb-cursor

CFLAGS += -I/usr/include/startup-notification-1.0 -DSN_API_NOT_YET_FROZEN=1
LIBS += -lstartup-notification-1

ifeq ($(UNAME),Linux)
CFLAGS += -D_GNU_SOURCE
endif

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

all: CFLAGS += -Os
all: $(__WM_NAME__)

debug: CFLAGS += -O0 -g -DDEBUG
debug: $(__WM_NAME__)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(__WM_NAME__): $(OBJ)
	$(CC) $(LIBS) $(CFLAGS) -o $@ $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(BINPREFIX)
	install -D -m 0755 $(__WM_NAME__) $(DESTDIR)$(BINPREFIX)
	mkdir -p $(DESTDIR)$(MANPREFIX)
	install -D -m 0644 doc/$(__WM_NAME__).1 $(DESTDIR)$(MANPREFIX)/man1/$(__WM_NAME__).1

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/$(__WM_NAME__)
	rm -f $(DESTDIR)$(MANPREFIX)/man1/$(__WM_NAME__).1

clean:
	rm -f $(OBJ) $(__WM_NAME__)

.PHONY: all debug install uninstall clean

