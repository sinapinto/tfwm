VERSION = $(shell git describe --tags 2>/dev/null)

ifeq ($(VERSION),)
  VERSION = 0.1
endif

PREFIX    ?= /usr/local
BINPREFIX ?= $(PREFIX)/bin

CC ?= gcc

CFLAGS  = -std=c99 -Wall -pedantic -I$(PREFIX)/include
CFLAGS += -DVERSION=\"$(VERSION)\"

LIBS    = -lxcb-keysyms -lxcb-icccm -lxcb-ewmh -lxcb-shape \
          -lxcb-image -lxcb-shm -lxcb-util -lxcb
