# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio -I$(top_srcdir)/popt \
	@INCPATH@ -I$(top_srcdir)/misc

EXTRA_DIST =	fstrcmp.c message.c str-list.c rpmchecksig.c

EXTRA_PROGRAMS = rpmgettext rpmputtext rpminject

#myLDFLAGS= -L$(top_builddir)/build -L$(top_builddir)/lib \
#	-L$(top_builddir)/rpmio -L$(top_builddir)/popt

myLDADD = $(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la \
	@INTLLIBS@

#LDFLAGS = $(myLDFLAGS)
LDADD = $(myLDADD)

noinst_PROGRAMS = \
	dump dumpdb javadeps rpmarchive rpmheader rpmlead rpmsignature

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = @BUILD_RPMNLSTOOLS@

rpmgettext_SOURCES =	rpmgettext.c

rpmputtext_SOURCES =	rpmgettext.c

$(PROGRAMS): $(myLDADD)
