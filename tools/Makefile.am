# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@WITH_LIBELF_INCLUDE@ \
	@WITH_ZLIB_INCLUDE@ \
	@INCPATH@ \
	-I$(top_srcdir)/misc

EXTRA_DIST =	rpminject.c rpmsort.c sections.h utils.h

EXTRA_PROGRAMS = rpminject rpmsort

LDADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la \
	@WITH_LIBELF_LIB@ \
	@INTLLIBS@

noinst_PROGRAMS = \
	dump dumpdb rpmarchive rpmheader rpmlead rpmsignature

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = javadeps rpmcache striptofile unstripfile

bin_PROGRAMS =	rpmgraph

javadeps_SOURCES =	javadeps.c

rpmcache_SOURCES =	rpmcache.c
#rpmcache_LDFLAGS =	@LDFLAGS_STATIC@

rpmgraph_SOURCES =	rpmgraph.c
#rpmgraph_LDFLAGS =	@LDFLAGS_STATIC@

rpminject_SOURCES =	rpminject.c
rpminject_LDFLAGS =	@LDFLAGS_STATIC@

rpmsort_SOURCES =	rpmsort.c
rpmsort_LDFLAGS =	@LDFLAGS_STATIC@

striptofile_SOURCES =	striptofile.c sections.c utils.c

unstripfile_SOURCES =	unstripfile.c sections.c utils.c

$(PROGRAMS): $(LDADD)
