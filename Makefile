include config.mk

SRC = tfwm.c util.c shape.c
OBJ = ${SRC:.c=.o}

all: CFLAGS += -Os
all: tfwm

debug: CFLAGS += -O0 -g -DDEBUG
debug: tfwm

tfwm.o: tfwm.c tfwm.h types.h util.h
shape.o: shape.c types.h util.h tfwm.h

$(OBJ): config.h config.mk

.c.o:
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
