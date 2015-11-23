# spine - dmenu-like stupid pin entry
# See LICENSE file for copyright and license details.

include config.mk

SRC = spine.c drw.c util.c
OBJ = ${SRC:.c=.o}

all: options spine

options:
	@echo spine build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

${OBJ}: config.h config.mk drw.h

spine: spine.o drw.o util.o
	@echo CC -o $@
	@${CC} -o $@ spine.o drw.o util.o pinentry/pinentry.o pinentry/util.o pinentry/password-cache.o pinentry/argparse.o pinentry/secmem.o ${LDFLAGS} -lassuan -lgpgme -lgpg-error

clean:
	@echo cleaning
	@rm -f spine ${OBJ}

.PHONY: all options clean
