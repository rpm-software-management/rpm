# Makefile for rpm tools.

include $(top_srcdir)/rpm.am

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	@WITH_LIBELF_INCLUDE@ \
	@WITH_MAGIC_INCLUDE@ \
	-I$(top_srcdir)/misc
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""

EXTRA_PROGRAMS =

LDADD = ../lib/librpm.la

staticLDFLAGS = @LDFLAGS_STATIC@ @LDFLAGS_NPTL@

check_PROGRAMS = dumpdb

rpmlibexec_PROGRAMS =

check_PROGRAMS +=	convertdb1
convertdb1_SOURCES =	convertdb1.c

if LIBELF
if LIBDWARF
rpmlibexec_PROGRAMS +=	debugedit
debugedit_SOURCES =	debugedit.c hashtab.c hashtab.h
debugedit_LDADD =	@WITH_LIBELF_LIB@ \
	@WITH_BEECRYPT_LIB@ \
	@WITH_POPT_LIB@
endif
endif

rpmlibexec_PROGRAMS +=	javadeps
javadeps_SOURCES =	javadeps.c

rpmlibexec_PROGRAMS +=	rpmcache
rpmcache_SOURCES =	rpmcache.c

rpmlibexec_PROGRAMS +=	rpmdeps
rpmdeps_SOURCES =	rpmdeps.c
rpmdeps_LDADD =		../build/librpmbuild.la

check_PROGRAMS +=	rpminject
rpminject_SOURCES =	rpminject.c
rpminject_LDADD =	../build/librpmbuild.la

check_PROGRAMS +=	rpmsort
rpmsort_SOURCES =	rpmsort.c

if LIBXML2
check_PROGRAMS +=	rpmtool
rpmtool_SOURCES =	rpmtool.c rpmxp.c rpmxp.h
rpmtool_CPPFLAGS =	$(AM_CPPFLAGS) $(libxml2_CFLAGS)
rpmtool_LDADD =		../lib/librpm.la $(libxml2_LIBS)

check_PROGRAMS +=	rpmxml
rpmxml_SOURCES =	rpmxml.c rpmxp.h
rpmxml_CPPFLAGS =	$(AM_CPPFLAGS) $(libxml2_CFLAGS)
rpmxml_LDADD =		../lib/librpm.la $(libxml2_LIBS)
endif
