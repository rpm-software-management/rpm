# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio -I$(top_srcdir)/popt \
	@INCPATH@ -I$(top_srcdir)/misc

EXTRA_DIST =	rpmchecksig.c

EXTRA_PROGRAMS = rpminject

#myLDFLAGS= -L$(top_builddir)/build -L$(top_builddir)/lib \
#	-L$(top_builddir)/rpmio -L$(top_builddir)/popt

myLDADD = $(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la \
	@INTLLIBS@

LIBS =
#LDFLAGS = $(myLDFLAGS)
LDADD = $(myLDADD)

noinst_PROGRAMS = \
	dump dumpdb rpmarchive rpmheader rpmlead rpmsignature rpmsort

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = javadeps

rpmsort_SOURCES =	rpmsort.c
rpmsort_LDFLAGS =

javadeps_SOURCES =	javadeps.c

$(PROGRAMS): $(myLDADD)
