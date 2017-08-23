include ../config.mk

SRC = util.c pinentry.c argparse.c password-cache.c secmem.c
OBJ = ${SRC:.c=.o}
CFLAGS += -DHAVE_MLOCK

all: pinentry

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: pinentry.h argparse.h password-cache.h memory.h util.h

pinentry: pinentry.o argparse.o password-cache.o secmem.o util.o

clean:
	@echo cleaning
	@rm -f ${OBJ}

.PHONY: all clean pinentry
