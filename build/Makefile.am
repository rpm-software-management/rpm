# Makefile for rpmbuild library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@WITH_LIBELF_INCLUDE@ \
	@INCPATH@

LIBS =

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmbuild.h rpmspec.h
noinst_HEADERS = argv.h buildio.h rpmfc.h

lib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	argv.c build.c expression.c files.c misc.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c poptBT.c reqprov.c rpmfc.c spec.c
librpmbuild_la_LDFLAGS = -release @VERSION@

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lrpm-@VERSION@|" < librpmbuild.la > .librpmbuild.la && \
	mv .librpmbuild.la librpmbuild.la

#BUILT_SOURCES = rpmbuild.lcd

rpmbuild.lcd: Makefile.am ${librpmbuild_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmbuild_la_SOURCES:%=build/%)

.PHONY:	lclint
lclint:
	lclint ${DEFS} ${INCLUDES} ${librpmbuild_la_SOURCES}
