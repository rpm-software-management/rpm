# Makefile for rpmbuild library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt @INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmbuild.h rpmspec.h
noinst_HEADERS = buildio.h myftw.h

lib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c expression.c files.c misc.c myftw.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c reqprov.c spec.c
librpmbuild_la_LIBADD = -L$(top_builddir)/lib/.libs -lrpm

# XXX Drill out -L ldflags remnants until libtool-1.4 appears
install-data-local:
	@cd $(libdir) && \
	sed -e "s|-L$(top_builddir)/lib/.libs ||" < librpmbuild.la > .librpmbuild.la && \
	mv .librpmbuild.la librpmio.la


.PHONY:	lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES}

.PHONY:	genhtml
genhtml:
	doc++ -d html -f -H -S build.c expression.c
#	doc++ -A -d html -f -H -S ${pkginc_HEADERS} ${librpmbuild_la_SOURCES}
