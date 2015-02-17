CC		?= gcc
PREFIX 	?= /usr
BWM_PATH = ${PREFIX}/bin/bwm

SRC 	 = bwm.c
TARGET 	 = $(SRC:.c=)
OBJ 	 = $(SRC:.c=.o)
CFLAGS 	+= -std=c99 -Os -Wall -pedantic -I. -DBWM_PATH=\"${BWM_PATH}\"
LDFLAGS += `pkg-config --libs xcb xcb-keysyms xcb-icccm xcb-ewmh`

all: $(TARGET)

$(OBJ): config.h events.h

%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) -o $@ $<

$(TARGET): $(OBJ)
	@echo CC -o $@
	@$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $@.c

clean:
	@echo cleaning
	@rm -f *.o $(TARGET)

install: all
	@echo installing binary to ${PREFIX}/bin
	@mkdir -p ${PREFIX}/bin
	@cp -f $(TARGET) ${PREFIX}/bin
	@chmod 755 ${PREFIX}/bin/$(TARGET)

uninstall:
	@echo removing binary from  ${PREFIX}/bin
	@rm -f ${PREFIX}/bin/$(TARGET)

.PHONY: all clean install uninstall
