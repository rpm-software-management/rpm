# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	-I$(top_srcdir)/popt \
	@WITH_LIBELF_INCLUDE@ \
	@WITH_LIBDWARF_INCLUDE@ \
	-I$(top_srcdir)/file/src \
	-I$(top_srcdir)/misc \
	@INCPATH@

EXTRA_DIST =	rpminject.c rpmsort.c rpmxml.c rpmxp.c rpmxp.h hashtab.h

EXTRA_PROGRAMS = debugedit convertdb1 rpminject rpmsort rpmtool rpmxml

LDADD = \
	$(top_builddir)/lib/librpm.la

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

noinst_PROGRAMS = dumpdb

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = @WITH_LIBDWARF_DEBUGEDIT@ javadeps rpmcache rpmdeps rpmfile
MAGIC	= $(pkgbindir)/magic

bin_PROGRAMS =	rpmgraph

convertdb1_SOURCES =	convertdb1.c

debugedit_SOURCES =	debugedit.c hashtab.c
debugedit_LDADD =	@LDFLAGS_STATIC@ @WITH_LIBELF_LIB@ \
	$(top_builddir)/popt/libpopt.la

javadeps_SOURCES =	javadeps.c

rpmcache_SOURCES =	rpmcache.c
#rpmcache_LDFLAGS =	-L$(libdir) # $(staticLDFLAGS)

rpmdeps_SOURCES =	rpmdeps.c
#rpmdeps_LDFLAGS =	$(staticLDFLAGS)
rpmdeps_LDADD =		$(top_builddir)/build/librpmbuild.la

rpmfile.c: $(top_srcdir)/file/src/file.c
	rm -f $@
	ln -s $< $@

rpmfile_SOURCES =	rpmfile.c
rpmfile_CFLAGS =	-DMAGIC='"$(MAGIC)"'
rpmfile_LDADD =		$(top_builddir)/rpmio/librpmio.la
#rpmfile_LDFLAGS =	$(staticLDFLAGS)

rpmgraph_SOURCES =	rpmgraph.c
#rpmgraph_LDFLAGS =	$(staticLDFLAGS)

rpminject_SOURCES =	rpminject.c
#rpminject_LDFLAGS =	$(staticLDFLAGS)

rpmsort_SOURCES =	rpmsort.c
#rpmsort_LDFLAGS =	(staticLDFLAGS)

rpmtool_SOURCES =	rpmtool.c rpmxp.c
rpmtool_CFLAGS =	-I$(includedir)/libxml2
#rpmtool_LDFLAGS =	$(staticLDFLAGS)
rpmtool_LDADD =		$(LDADD) -lxml2

rpmxml_SOURCES =	rpmxml.c # rpmxp.c
rpmxml_CFLAGS =		-I$(includedir)/libxml2
#rpmxml_LDFLAGS =	$(staticLDFLAGS)
rpmxml_LDADD =		$(LDADD) -lxml2

$(PROGRAMS): $(LDADD)
