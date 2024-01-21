# Makefile for mg

# This Makefile has been written by Han Boetes
# <hboetes@gmail.com> and is released in Public Domain.

# *sigh* Those debian folks are really tidy on their licenses.

name=		mg

prefix=		/usr/local
bindir=		$(prefix)/bin
libdir=		$(prefix)/lib
includedir=	$(prefix)/include
mandir=		$(prefix)/man

PKG_CONFIG=	/usr/bin/pkg-config --silence-errors
INSTALL=	/usr/bin/install
STRIP=		/usr/bin/strip

UNAME:=		$(shell uname)
ifeq ($(UNAME),FreeBSD)
  BSD_CPPFLAGS:= -DHAVE_UTIL_H
  BSD_LIBS:=	 -lutil
else
  BSD_CPPFLAGS:= $(shell $(PKG_CONFIG) --cflags libbsd-overlay) -DHAVE_PTY_H
  BSD_LIBS:=	 $(shell $(PKG_CONFIG) --libs libbsd-overlay) -lutil
endif

# Test if required libraries are installed. Rather bummer that they
# are also required to run make clean or uninstall. Oh well... Who
# does that?
ifeq ($(BSD_LIBS),)
  $(error You probably need to install "libbsd-dev" or "libbsd-devel" or something like that.)
endif

CURSES_LIBS:= $(shell $(PKG_CONFIG) --libs ncurses)
ifeq ($(CURSES_LIBS),)
  $(error You probably need to install "libncurses5-dev" or "libncurses6-devel" or something like that.)
endif

ifdef STATIC
  LDFLAGS=-static -static-libgcc
endif

CC?=		gcc
CFLAGS?=	-O2 -pipe
CFLAGS+=	-g -Wall
CPPFLAGS=	-DREGEX
CPPFLAGS+=	-D_GNU_SOURCE
CPPFLAGS+=	$(BSD_CPPFLAGS)
LIBS=		$(CURSES_LIBS) $(BSD_LIBS)


OBJS=	autoexec.o basic.o bell.o buffer.o cinfo.o dir.o display.o \
	echo.o extend.o file.o fileio.o funmap.o interpreter.o help.o \
	kbd.o keymap.o line.o macro.o main.o match.o modes.o paragraph.o \
	re_search.o region.o search.o spawn.o tty.o ttyio.o ttykbd.o \
	undo.o util.o version.o window.o word.o yank.o
OBJS+=	cmode.o cscope.o dired.o grep.o tags.o


# Portability stuff.
EXE_EXT=

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<

all: $(name)

$(name): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $(name) $(LIBS)

distclean: clean
	-rm -f *.core core.* .#*

clean:
	-rm -f *.o $(name)$(EXE_EXT)


install: $(name) $(name).1
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -d $(DESTDIR)$(mandir)/man1
	$(INSTALL) -m 755 $(name)		$(DESTDIR)$(bindir)/$(name)
	$(INSTALL) -m 444 $(name).1		$(DESTDIR)$(mandir)/man1/$(name).1

install-strip: install
	$(STRIP) $(DESTDIR)$(bindir)/$(name)

uninstall:
	rm -f \
	$(DESTDIR)$(bindir)/$(name)$(EXE_EXT) \
	$(DESTDIR)$(mandir)/man1/$(name).1

rebuild:
	make clean all

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
