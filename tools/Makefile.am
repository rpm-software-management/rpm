# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/intl \
	-I$(top_srcdir)/misc \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST =	fstrcmp.c message.c str-list.c rpmchecksig.c

mylibs=	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/popt/libpopt.la

LDADD = $(top_builddir)/lib/librpm.la

noinst_PROGRAMS = \
	rpmlead rpmheader rpmarchive rpmsignature dump dumpdb javadeps

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = rpmgettext rpmputtext

rpmgettext_SOURCES =	rpmgettext.c
rpmgettext_LDADD =	$(mylibs) @LIBMISC@

rpmputtext_SOURCES =	rpmgettext.c
rpmputtext_LDADD =	$(mylibs) @LIBMISC@

$(PROGRAMS): $(mylibs)
