# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@WITH_ZLIB_INCLUDE@ \
	@INCPATH@ \
	-I$(top_srcdir)/misc

EXTRA_DIST =	rpmchecksig.c

EXTRA_PROGRAMS = rpminject rpmsort

#myLDFLAGS= -L$(top_builddir)/build -L$(top_builddir)/lib \
#	-L$(top_builddir)/rpmio -L$(top_builddir)/popt

myLDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la \
	@WITH_ZLIB_LIB@ \
	@INTLLIBS@

LIBS =

#LDFLAGS = $(myLDFLAGS)
LDADD = $(myLDADD)

noinst_PROGRAMS = \
	dump dumpdb rpmarchive rpmheader rpmlead rpmsignature

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = javadeps

rpmsort_SOURCES =	rpmsort.c
rpmsort_LDFLAGS =	@LDFLAGS_STATIC@

javadeps_SOURCES =	javadeps.c

$(PROGRAMS): $(myLDADD)

gnash.o: gnash.c
	$(COMPILE) -o $@ -c gnash.c

gnash: gnash.o
	$(LINK) -all-static -o $@ gnash.o $(LDADD)

