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
LIBS    = -lxcb -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-util \
          -lX11 -lX11-xcb -lXcursor

OBJ = tfwm.o util.o events.o client.o list.o workspace.o keys.o pointer.o \
      ewmh.o config.o xcb.o

all: CFLAGS += -Os
all: tfwm

debug: CFLAGS += -O0 -g -DDEBUG
debug: tfwm

tfwm.o: tfwm.c list.h client.h workspace.h events.h keys.h pointer.h ewmh.h \
        config.h util.h
util.o: util.c tfwm.h util.h
events.o: events.c tfwm.h client.h list.h events.h ewmh.h config.h util.h
client.o: client.c tfwm.h list.h client.h keys.h ewmh.h xcb.h config.h util.h
list.o: list.c tfwm.h client.h list.h xcb.h util.h
workspace.o: workspace.c tfwm.h list.h client.h util.h
keys.o: keys.c tfwm.h list.h client.h events.h workspace.h pointer.h util.h
pointer.o: pointer.c tfwm.h events.h config.h util.h
ewmh.o: ewmh.c tfwm.h config.h util.h
config.o: config.c tfwm.h util.h
shape.o: shape.c tfwm.h util.h
xcb.o: xcb.c tfwm.h util.h

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

tfwm: $(OBJ)
	$(CC) $(LIBS) $(CFLAGS) -o $@ $(OBJ)

install: all
	mkdir -p $(DESTDIR)$(BINPREFIX)
	install -D -m 0755 tfwm $(DESTDIR)$(BINPREFIX)
	mkdir -p $(DESTDIR)$(MANPREFIX)
	install -D -m 0644 doc/tfwm.1 $(DESTDIR)$(MANPREFIX)/man1/tfwm.1

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/tfwm
	rm -f $(DESTDIR)$(MANPREFIX)/man1/tfwm.1

clean:
	rm -f $(OBJ) tfwm

.PHONY: all debug install uninstall clean

