# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/lib -I$(top_srcdir)/intl -I$(top_srcdir)/misc @INCPATH@

LDFLAGS = -L$(top_builddir)/popt -L$(top_builddir)/build -L$(top_builddir)/lib -L$(top_builddir)/misc
LDADD = -lrpm

EXTRA_DIST =	fstrcmp.c message.c str-list.c

noinst_PROGRAMS = rpmlead rpmheader rpmarchive rpmsignature dump dumpdb \
	rpmgettext rpmputtext

rpmgettext_SOURCES =	rpmgettext.c
rpmgettext_LDADD =	-lrpmbuild -lpopt -lrpm @LIBMISC@

rpmputtext_SOURCES =	rpmgettext.c
rpmputtext_LDADD =	-lrpmbuild -lpopt -lrpm @LIBMISC@

$(PROGRAMS): ../build/librpmbuild.a ../lib/librpm.a ../popt/libpopt.a
