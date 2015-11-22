include config.mk

SRC = tfwm.c util.c
OBJ = ${SRC:.c=.o}

all: CFLAGS += -Os
all: tfwm

debug: CFLAGS += -O0 -g -DDEBUG
debug: tfwm

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
