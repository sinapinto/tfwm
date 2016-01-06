VERSION = $(shell git describe --tags 2>/dev/null)
ifeq ($(VERSION),)
VERSION = 0.1.0
endif

PREFIX   ?= /usr/local
MANPREFIX = $(PREFIX)/share/man
BINPREFIX = $(PREFIX)/bin

CC     ?= gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wshadow -Wno-uninitialized -pedantic
CFLAGS += -I$(PREFIX)/include -DVERSION=\"$(VERSION)\"
LIBS    = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-util -lxcb-cursor

UNAME := $(shell uname -s)

ifeq ($(UNAME),Linux)
CFLAGS += -D_GNU_SOURCE
endif

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)

all: CFLAGS += -Os
all: acidwm

debug: CFLAGS += -O0 -g -DDEBUG
debug: acidwm

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

acidwm: $(OBJ)
	$(CC) $(LIBS) $(CFLAGS) -o $@ $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(BINPREFIX)
	install -D -m 0755 acidwm $(DESTDIR)$(BINPREFIX)
	mkdir -p $(DESTDIR)$(MANPREFIX)
	install -D -m 0644 doc/acidwm.1 $(DESTDIR)$(MANPREFIX)/man1/acidwm.1

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/acidwm
	rm -f $(DESTDIR)$(MANPREFIX)/man1/acidwm.1

clean:
	rm -f $(OBJ) acidwm

.PHONY: all debug install uninstall clean

