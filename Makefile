include config.mk

SRC = tfwm.c util.c shape.c events.c window.c list.c workspace.c
OBJ = ${SRC:.c=.o}

all: CFLAGS += -Os
all: tfwm

debug: CFLAGS += -O0 -g -DDEBUG
debug: tfwm

tfwm.o: tfwm.c types.h list.h events.h window.h shape.h
shape.o: shape.c types.h util.h tfwm.h
events.o: events.c types.h window.h list.h util.h config.h tfwm.h
window.o: window.c types.h tfwm.h config.h list.h
list.o: list.c types.h window.h tfwm.h config.h
workspace.o: workspace.c types.h tfwm.h list.h window.h

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
