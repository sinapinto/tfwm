SRC 	= bwm.c
TARGET 	= $(SRC:.c=)
OBJ 	= $(SRC:.c=.o)
CFLAGS 	+= -std=c99 -Os -Wall -pedantic -I.
LDFLAGS += -lxcb
CC 		:= gcc
PREFIX 	:= /usr

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
	@echo installing binary to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${PREFIX}/bin
	@cp -f $(TARGET) ${PREFIX}/bin
	@chmod 755 ${PREFIX}/bin/$(TARGET)

uninstall:
	@echo removing binary from  ${PREFIX}/bin
	@rm -f ${PREFIX}/bin/$(TARGET)

.PHONY: all clean install uninstall
