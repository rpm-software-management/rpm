# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = -I../build -I../lib -I../intl -I../misc @INCPATH@

LDFLAGS = -L../popt -L../build -L../lib -L../misc
LDADD = -lrpm

EXTRA_DIST =	fstrcmp.c message.c str-list.c

noinst_PROGRAMS = rpmlead rpmheader rpmarchive rpmsignature dump dumpdb \
	rpmgettext rpmputtext

rpmgettext_SOURCES =	rpmgettext.c
rpmgettext_LDADD =	-lrpmbuild -lpopt -lrpm @LIBMISC@

rpmputtext_SOURCES =	rpmgettext.c
rpmputtext_LDADD =	-lrpmbuild -lpopt -lrpm @LIBMISC@

$(PROGRAMS): ../lib/librpm.a
