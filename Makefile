WMNAME = tfwm
VERSION = 2015-10
DIST = $(WMNAME)-$(VERSION)
CC ?= gcc
RM ?= /bin/rm
PREFIX ?= /usr/local
X11_INC ?= /usr/include/xcb
SRC = $(WMNAME).c
OBJ = $(WMNAME).o
CFLAGS += -std=c99 -Os -Wall -pedantic -I. -I$(X11_INC)
LDFLAGS += `pkg-config --libs xcb xcb-keysyms xcb-icccm xcb-ewmh`

all: options $(WMNAME)

options:
	@echo $(WMNAME) build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

$(OBJ): config.h

%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) -o $@ $<

$(WMNAME): $(OBJ)
	@echo CC -o $@
	@$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.c

clean:
	@echo cleaning
	@$(RM) -f *.o $(WMNAME) *.tar.gz

install: all
	@echo installing binary to $(PREFIX)/bin
	@mkdir -p $(PREFIX)/bin
	@cp -f $(WMNAME) $(PREFIX)/bin
	@chmod 755 $(PREFIX)/bin/$(WMNAME)

uninstall:
	@echo removing binary from $(PREFIX)/bin
	@$(RM) -f $(PREFIX)/bin/$(WMNAME)

dist: clean
	@echo creating dist tarball
	@mkdir -p $(DIST)
	@cp -R Makefile LICENSE README.md config.h $(SRC) $(DIST)
	@tar -cf $(DIST).tar --exclude .git $(DIST)
	@gzip $(DIST).tar
	@$(RM) -rf $(DIST)

.PHONY: all clean options install uninstall dist
