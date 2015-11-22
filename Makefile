VERSION = $(shell git describe --tags 2>/dev/null)

ifeq ($(VERSION),)
  VERSION = 0.1
endif

# the directory where the executable will be installed
BINDIR ?= /usr/local/bin

CC ?= gcc
CFLAGS = -std=c99 -Wall -pedantic -I/usr/include -DVERSION=\"$(VERSION)\"
LIBS = -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-shape -lxcb-image -lxcb-shm -lxcb-util -lxcb

SRC = tfwm.c util.c
OBJ = ${SRC:.c=.o}

all: CFLAGS += -Os
all: tfwm

debug: CFLAGS += -O0 -g -DDEBUG
debug: tfwm

$(OBJ): config.h

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

tfwm: $(OBJ)
	$(CC) $(LIBS) $(CFLAGS) -o $@ $(OBJ)

install: all
	mkdir -p "$(BINDIR)"
	cp -pf tfwm "$(BINDIR)"

uninstall:
	rm -f "$(BINDIR)"/tfwm

clean:
	rm -f $(OBJ) tfwm

.PHONY: all debug install uninstall clean
