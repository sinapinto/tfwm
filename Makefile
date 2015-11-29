VERSION= $(shell git describe --tags 2>/dev/null)
ifeq ($(VERSION),)
  VERSION= 0.1
endif

PREFIX?= /usr/local
BINPREFIX?= $(PREFIX)/bin

CC?= gcc

CFLAGS= -std=c99 -Wall -Wextra -pedantic -I$(PREFIX)/include -DVERSION=\"$(VERSION)\"

LIBS= -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-util -lxcb

# CFLAGS+= -DSHAPE
# LIBS+= -lxcb-shape -lxcb-image -lxcb-shm

OBJ= tfwm.o util.o events.o client.o list.o workspace.o

all: CFLAGS+= -Os
all: tfwm

debug: CFLAGS+= -O0 -g -DDEBUG
debug: tfwm

tfwm.o: tfwm.c list.h client.h workspace.h events.h
events.o: events.c tfwm.h client.h list.h events.h
client.o: client.c tfwm.h list.h client.h
list.o: list.c tfwm.h client.h list.h
workspace.o: workspace.c tfwm.h list.h client.h

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

tfwm: $(OBJ)
	$(CC) $(LIBS) $(CFLAGS) -o $@ $(OBJ)

install: all
	mkdir -p "$(BINPREFIX)"
	cp -pf tfwm "$(BINPREFIX)"

uninstall:
	rm -f "$(BINPREFIX)"/tfwm

clean:
	rm -f $(OBJ) tfwm

.PHONY: all debug install uninstall clean
