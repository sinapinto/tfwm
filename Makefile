VERSION = $(shell git describe --tags 2>/dev/null)
ifeq ($(VERSION),)
  VERSION = 0.1
endif

PREFIX ?= /usr/local
BINPREFIX ?= $(PREFIX)/bin

CC ?= gcc

CFLAGS = -std=c99 -Wall -pedantic -I$(PREFIX)/include
CFLAGS += -DVERSION=\"$(VERSION)\"

LIBS = -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-util -lxcb

CFLAGS += -DSHAPE
LIBS += -lxcb-shape -lxcb-image -lxcb-shm

OBJ = tfwm.o util.o events.o client.o list.o workspace.o

all: CFLAGS += -Os
all: tfwm

debug: CFLAGS += -O0 -g -DDEBUG
debug: tfwm

$(OBJ): config.h

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
