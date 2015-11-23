include ../config.mk

SRC = pinentry.c argparse.c password-cache.c
OBJ = ${SRC:.c=.o}

all: pinentry

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: pinentry.h argparse.h password-cache.h memory.h

pinentry: pinentry.o argparse.o password-cache.o secmem.o

clean:
	@echo cleaning
	@rm -f ${OBJ}

.PHONY: all clean pinentry
