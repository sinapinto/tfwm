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
LIBS    = -lX11 -lX11-xcb -lXcursor -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-util -lxcb

# uncomment to enable shape extension
# CFLAGS += -DSHAPE
# LIBS   += -lxcb-shape -lxcb-image -lxcb-shm

OBJ = tfwm.o util.o events.o client.o list.o workspace.o keys.o pointer.o ewmh.o

all: CFLAGS+= -Os
all: tfwm

debug: CFLAGS+= -O0 -g -DDEBUG
debug: tfwm

tfwm.o: tfwm.c list.h client.h workspace.h events.h keys.h pointer.h ewmh.h
events.o: events.c tfwm.h client.h list.h events.h
client.o: client.c tfwm.h list.h client.h keys.h ewmh.h
list.o: list.c tfwm.h client.h list.h
workspace.o: workspace.c tfwm.h list.h client.h
keys.o: keys.c tfwm.h list.h client.h events.h workspace.h pointer.h
pointer.o: pointer.c tfwm.h events.h
ewmh.o: ewmh.c tfwm.h

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

tfwm: $(OBJ)
	$(CC) $(LIBS) $(CFLAGS) -o $@ $(OBJ)

install: all
	install -D -m 0755 tfwm $(DESTDIR)$(BINPREFIX)
	install -D -m 0644 tfwm.1 $(DESTDIR)$(MANPREFIX)/man1

uninstall:
	rm -f $(DESTDIR)$(BINPREFIX)/tfwm
	rm -f $(DESTDIR)$(MANPREFIX)/man1/tfwm.1

clean:
	rm -f $(OBJ) tfwm

.PHONY: all debug install uninstall clean
