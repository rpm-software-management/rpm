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
	@INCPATH@ \
	-I$(top_srcdir)/misc

EXTRA_DIST =	rpminject.c rpmsort.c hashtab.h

EXTRA_PROGRAMS = rpminject rpmsort

LDADD = \
	$(top_builddir)/lib/librpm.la \
	@INTLLIBS@

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

noinst_PROGRAMS = \
	convertdb1 dump dumpdb rpmarchive rpmheader rpmlead rpmsignature

pkgbindir = @RPMCONFIGDIR@
pkgbin_PROGRAMS = debugedit javadeps rpmcache rpmdeps rpmfile

bin_PROGRAMS =	rpmgraph

convertdb1_SOURCES =	convertdb1.c

debugedit_SOURCES =	debugedit.c hashtab.c
debugedit_LDADD =	@LDFLAGS_STATIC@ @WITH_LIBELF_LIB@ \
	$(top_builddir)/popt/libpopt.la

javadeps_SOURCES =	javadeps.c

rpmcache_SOURCES =	rpmcache.c
#rpmcache_LDFLAGS =	$(staticLDFLAGS)

rpmdeps_SOURCES =	rpmdeps.c
rpmdeps_LDADD =		$(top_builddir)/build/librpmbuild.la
#rpmdeps_LDFLAGS =	$(staticLDFLAGS)

rpmfile.c: $(top_builddir)/file/src/file.c
	rm -f $@
	ln -s $< $@

rpmfile_SOURCES =	rpmfile.c
rpmfile_LDADD =		$(top_builddir)/rpmio/librpmio.la
#rpmfile_LDFLAGS =	$(staticLDFLAGS)

rpmgraph_SOURCES =	rpmgraph.c
#rpmgraph_LDFLAGS =	$(staticLDFLAGS)

rpminject_SOURCES =	rpminject.c
#rpminject_LDFLAGS =	$(staticLDFLAGS)

rpmsort_SOURCES =	rpmsort.c
#rpmsort_LDFLAGS =	$(staticLDFLAGS)

$(PROGRAMS): $(LDADD)
