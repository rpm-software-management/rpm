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
