# pinentry-dmenu - dmenu-like stupid pin entry
# See LICENSE file for copyright and license details.

include config.mk

SRC = pinentry-dmenu.c drw.c util.c
OBJ = ${SRC:.c=.o}

all: options pinentry-dmenu

options:
	@echo pinentry-dmenu build options:
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

pinentry:
	$(MAKE) -C pinentry

pinentry-dmenu: pinentry pinentry-dmenu.o drw.o util.o
	@echo CC -o $@
	@${CC} -o $@ pinentry-dmenu.o drw.o util.o pinentry/pinentry.o pinentry/util.o pinentry/password-cache.o pinentry/argparse.o pinentry/secmem.o ${LDFLAGS} -lassuan -lgpgme -lgpg-error

clean:
	@echo cleaning
	@rm -f pinentry-dmenu ${OBJ}
	$(MAKE) -C pinentry/ clean

.PHONY: all options clean pinentry
