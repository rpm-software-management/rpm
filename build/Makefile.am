# Makefile for rpmbuild library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/lib -I$(top_srcdir)/popt \
	@INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmbuild.h rpmspec.h
noinst_HEADERS = buildio.h myftw.h

lib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c expression.c files.c misc.c myftw.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c reqprov.c spec.c
librpmbuild_la_LIBADD = -lrpm -lpopt

.PHONY:	lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES}

.PHONY:	genhtml
genhtml:
	doc++ -d html -f -H -S build.c expression.c
#	doc++ -A -d html -f -H -S ${pkginc_HEADERS} ${librpmbuild_la_SOURCES}
