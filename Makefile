WMNAME = tfwm

# the directory where the executable will be installed
BINDIR ?= /usr/local/bin

CC ?= gcc
CFLAGS += -std=c99 -Wall -pedantic -I/usr/include
LIBS = -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-shape -lxcb-image -lxcb-shm -lxcb-util -lxcb

SRC = $(WMNAME).c
OBJ = ${SRC:.c=.o}

all: CFLAGS += -Os
all: $(WMNAME)

debug: CFLAGS += -O0 -g -DDEBUG
debug: $(WMNAME)

$(OBJ): config.h util.c

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(WMNAME): $(OBJ)
	$(CC) $(LIBS) $(CFLAGS) -o $@ $@.c

install: all
	mkdir -p $(BINDIR)
	cp -f $(WMNAME) $(BINDIR)
	chmod 755 $(BINDIR)/$(WMNAME)

uninstall:
	rm -f $(BINDIR)/$(WMNAME)

clean:
	rm -f *.o $(WMNAME)

.PHONY: all debug install uninstall clean
