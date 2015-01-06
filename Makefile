CC = gcc
CFLAGS = -std=c99 -Os -Wall -pedantic -I.
LDFLAGS = -lxcb -lxcb-keysyms
SRC = bwm.c
OBJ = ${SRC:.c=.o}

all: bwm

$(OBJ): config.h events.h

.c.o:
	$(CC) -c $< $(CFLAGS)

bwm: $(OBJ)
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.c

clean:
	rm -f bwm $(OBJ)
