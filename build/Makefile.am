# Makefile for rpmbuild library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/lib @INCPATH@

rpmincdir = $(includedir)/rpm
rpminc_HEADERS = rpmbuild.h rpmspec.h
noinst_HEADERS = buildio.h myftw.h

lib_LIBRARIES = librpmbuild.a
librpmbuild_a_SOURCES = \
	build.c expression.c files.c misc.c myftw.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c reqprov.c spec.c

.PHONY:	lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${librpmbuild_a_SOURCES}
