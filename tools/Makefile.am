# Makefile for rpm tools.

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	@WITH_LIBELF_INCLUDE@ \
	@WITH_LIBDWARF_INCLUDE@ \
	@WITH_MAGIC_INCLUDE@ \
	-I$(top_srcdir)/misc \
	@INCPATH@

EXTRA_PROGRAMS =

LDADD = ../lib/librpm.la

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

noinst_PROGRAMS = dumpdb

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = @WITH_LIBDWARF_DEBUGEDIT@

EXTRA_PROGRAMS +=	convertdb1
convertdb1_SOURCES =	convertdb1.c

EXTRA_PROGRAMS +=	debugedit
debugedit_SOURCES =	debugedit.c hashtab.c hashtab.h
# debugedit_LDFLAGS =	$(staticLDFLAGS) \
debugedit_LDFLAGS =	@WITH_LIBELF_LIB@ \
	@WITH_BEECRYPT_LIB@ \
	@WITH_POPT_LIB@

pkgbin_PROGRAMS +=	javadeps
javadeps_SOURCES =	javadeps.c

pkgbin_PROGRAMS +=	rpmcache
rpmcache_SOURCES =	rpmcache.c
#rpmcache_LDFLAGS =	-L$(libdir) # $(staticLDFLAGS)

pkgbin_PROGRAMS +=	rpmdeps
rpmdeps_SOURCES =	rpmdeps.c
#rpmdeps_LDFLAGS =	$(staticLDFLAGS)
rpmdeps_LDADD =		../build/librpmbuild.la

bin_PROGRAMS =		rpmgraph
rpmgraph_SOURCES =	rpmgraph.c
#rpmgraph_LDFLAGS =	$(staticLDFLAGS)

EXTRA_PROGRAMS +=	rpminject
rpminject_SOURCES =	rpminject.c
#rpminject_LDFLAGS =	$(staticLDFLAGS)
rpminject_LDADD =	../build/librpmbuild.la

EXTRA_PROGRAMS +=	rpmsort
rpmsort_SOURCES =	rpmsort.c
#rpmsort_LDFLAGS =	$(staticLDFLAGS)

EXTRA_PROGRAMS +=	rpmtool
rpmtool_SOURCES =	rpmtool.c rpmxp.c rpmxp.h
rpmtool_CFLAGS =	-I$(includedir)/libxml2
#rpmtool_LDFLAGS =	$(staticLDFLAGS)
rpmtool_LDADD =		../lib/librpm.la -lxml2

EXTRA_PROGRAMS +=	rpmxml
rpmxml_SOURCES =	rpmxml.c rpmxp.h
rpmxml_CFLAGS =		-I$(includedir)/libxml2
#rpmxml_LDFLAGS =	$(staticLDFLAGS)
rpmxml_LDADD =		../lib/librpm.la -lxml2

CLEANFILES = $(EXTRA_PROGRAMS)
