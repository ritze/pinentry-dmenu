# pinentry-dmenu - dmenu-like stupid pin entry
# See LICENSE file for copyright and license details.

include config.mk

SRC = pinentry-dmenu.c drw.c util.c
OBJ = ${SRC:.c=.o}
OBJ_PIN = pinentry/pinentry.o pinentry/util.o pinentry/password-cache.o pinentry/argparse.o pinentry/secmem.o

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
	@${CC} -o $@ ${OBJ} ${OBJ_PIN} ${LDFLAGS} -lassuan -lgpgme -lgpg-error -lconfig

clean:
	@echo cleaning
	@rm -f pinentry-dmenu ${OBJ}
	$(MAKE) -C pinentry/ clean

dist: clean
	@echo creating dist tarball
	@mkdir -p dmenu-${VERSION}
	@cp LICENSE Makefile README arg.h config.def.h config.mk dmenu.1 \
		drw.h util.h dmenu_path dmenu_run stest.1 ${SRC} \
		dmenu-${VERSION}
	@tar -cf dmenu-${VERSION}.tar dmenu-${VERSION}
	@gzip dmenu-${VERSION}.tar
	@rm -rf dmenu-${VERSION}

install: all
	@echo installing executable to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f pinentry-dmenu ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/pinentry-dmenu
	@echo installing manual page to ${DESTDIR}${MANPREFIX}/man1
	@mkdir -p ${DESTDIR}${MANPREFIX}/man1
	@sed "s/VERSION/${VERSION}/g;s/DATE/${DATE}/g;s/BUGREPORT/${BUGREPORT}/g" < pinentry-dmenu.1 > ${DESTDIR}${MANPREFIX}/man1/pinentry-dmenu.1
	@chmod 644 ${DESTDIR}${MANPREFIX}/man1/pinentry-dmenu.1

uninstall:
	@echo removing executable from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/pinentry-dmenu
	@echo removing manual page from ${DESTDIR}${MANPREFIX}/man1
	@rm -f ${DESTDIR}${MANPREFIX}/man1/pinentry-dmenu.1

.PHONY: all options clean dist install pinentry uninstall
