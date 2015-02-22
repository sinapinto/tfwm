WMNAME  = bwm
VERSION = 2015-02
DIST 	= $(WMNAME)-$(VERSION)

CC			?= gcc
RM			 = /bin/rm
PREFIX 		?= /usr/local
BWM_PATH 	 = $(PREFIX)/bin/$(WMNAME)
X11_INC		?= /usr/include/xcb

SRC 	 = $(WMNAME).c
OBJ 	 = $(WMNAME).o
CFLAGS 	+= -std=c99 -Os -Wall -pedantic -I. -I$(X11_INC) \
		   -DBWM_PATH=\"$(BWM_PATH)\"
LDFLAGS += `pkg-config --libs xcb xcb-keysyms xcb-icccm xcb-ewmh`

all: options $(WMNAME)

options:
	@echo $(WMNAME) build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

$(OBJ): config.h events.h

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
	@cp -R Makefile README.md config.h events.h $(SRC) $(DIST)
	@tar -cf $(DIST).tar --exclude .git $(DIST)
	@gzip $(DIST).tar
	@$(RM) -rf $(DIST)

.PHONY: all clean options install uninstall dist
