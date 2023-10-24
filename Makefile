# $OpenBSD: Makefile,v 1.35 2019/07/18 05:57:48 lum Exp $

PROG=	mg

LDADD+=	`pkg-config --libs ncurses` -lutil
DPADD+=	${LIBUTIL}

# (Common) compile-time options:
#
#	REGEX		-- create regular expression functions.
#	STARTUPFILE	-- look for and handle initialization file.
#	MGLOG		-- debug mg internals to a log file.
#
CFLAGS+=-Wall -DREGEX `pkg-config --cflags-only-I ncurses`

SRCS=	autoexec.c basic.c bell.c buffer.c cinfo.c dir.c display.c \
	echo.c extend.c file.c fileio.c funmap.c help.c kbd.c keymap.c \
	line.c macro.c main.c match.c modes.c paragraph.c \
	re_search.c region.c search.c spawn.c tty.c ttyio.c ttykbd.c \
	undo.c util.c version.c window.c word.c yank.c

#
# More or less standalone extensions.
#
SRCS+=	cmode.c cscope.c dired.c grep.c interpreter.c tags.c

#
# -DMGLOG source file.
#
#SRCS+=	log.c

afterinstall:
	${INSTALL} -d -o root -g wheel ${DESTDIR}${DOCDIR}/mg
	${INSTALL} ${INSTALL_COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} \
	    ${.CURDIR}/tutorial ${DESTDIR}${DOCDIR}/mg

.include <bsd.prog.mk>

# Leave room for makedepend(1).
# DO NOT DELETE

autoexec.o: def.h chrdef.h funmap.h
basic.o: def.h chrdef.h
bell.o: def.h chrdef.h macro.h
buffer.o: def.h chrdef.h kbd.h
cinfo.o: def.h chrdef.h
cmode.o: def.h chrdef.h funmap.h kbd.h
cscope.o: def.h chrdef.h
def.o: chrdef.h
dir.o: def.h chrdef.h
dired.o: def.h chrdef.h funmap.h kbd.h
display.o: def.h chrdef.h kbd.h
echo.o: def.h chrdef.h funmap.h key.h macro.h
extend.o: chrdef.h def.h funmap.h kbd.h key.h macro.h
file.o: def.h chrdef.h
fileio.o: def.h chrdef.h kbd.h pathnames.h
funmap.o: def.h chrdef.h funmap.h kbd.h
grep.o: def.h chrdef.h kbd.h funmap.h
help.o: def.h chrdef.h funmap.h kbd.h key.h macro.h
interpreter.o: def.h chrdef.h funmap.h
kbd.o: def.h chrdef.h kbd.h key.h macro.h
keymap.o: def.h chrdef.h kbd.h
line.o: def.h chrdef.h
log.o: def.h chrdef.h key.h kbd.h funmap.h log.h
macro.o: def.h chrdef.h key.h macro.h
main.o: def.h chrdef.h kbd.h funmap.h macro.h
match.o: def.h chrdef.h key.h
modes.o: def.h chrdef.h kbd.h
paragraph.o: def.h chrdef.h
region.o: def.h chrdef.h
search.o: def.h chrdef.h macro.h
spawn.o: def.h chrdef.h
tags.o: def.h chrdef.h
tty.o: def.h chrdef.h
ttyio.o: def.h chrdef.h
ttykbd.o: def.h chrdef.h kbd.h
undo.o: def.h chrdef.h kbd.h
util.o: def.h chrdef.h
version.o: def.h chrdef.h
window.o: def.h chrdef.h
word.o: def.h chrdef.h
yank.o: def.h chrdef.h
