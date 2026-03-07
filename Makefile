CC=		cc
CFLAGS_CHECK=	-Wall -Wpedantic
CFLAGS=
CFLAGS_RELEASE=	${CFLAGS_CHECK} -O2
CFLAGS_DBG=	-DDEBUG
CFLAGS_DEBUG=	-g ${CFLAGS_DBG}
CFLAGS_DEV=	${CFLAGS_CHECK} ${CFLAGS_DBG}
INCS=		-I/usr/local/include -I/usr/local/include/freetype2
LIBS=		-L/usr/local/lib -lfontconfig -lX11 -lXft
PREFIX=		/usr/local
BINDIR=		bin
INSTALL=	install
INSTALL_STRIP=	-s
SRC=		xnf.c
HEADERS=	config.h
SRC_ALL=	${SRC} ${HEADERS}
BIN=		xnf

all: dev

config.h: config.def.h
	cp config.def.h config.h

dev: ${SRC_ALL}
	${CC} ${CFLAGS} ${CFLAGS_DEV} -o ${BIN} ${INCS} ${LIBS} ${SRC}

debug: ${SRC_ALL}
	${CC} ${CFLAGS} ${CFLAGS_DEBUG} -o ${BIN} ${INCS} ${LIBS} ${SRC}

release: ${SRC_ALL}
	${CC} ${CFLAGS} ${CFLAGS_RELEASE} -o ${BIN} ${INCS} ${LIBS} ${SRC}

install: release
	mkdir -p ${PREFIX}/${BINDIR}
	${INSTALL} ${INSTALL_STRIP} ${BIN} ${PREFIX}/${BINDIR}

clean:
	rm -f ${BIN}
