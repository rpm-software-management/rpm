# Makefile for rpmbuild library.

AM_CPPFLAGS = -I.
AM_CPPFLAGS += -I$(top_srcdir)
AM_CPPFLAGS += -I$(top_srcdir)/lib
AM_CPPFLAGS += -I$(top_srcdir)/rpmdb
AM_CPPFLAGS += -I$(top_srcdir)/rpmio
AM_CPPFLAGS += @WITH_BEECRYPT_INCLUDE@
AM_CPPFLAGS += @WITH_MAGIC_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += @WITH_LIBELF_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c buildio.h expression.c files.c misc.c names.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c poptBT.c reqprov.c rpmfc.c spec.c
librpmbuild_la_LDFLAGS = -release 4.4
librpmbuild_la_LIBADD = \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_LIBELF_LIB@ \
	@WITH_MAGIC_LIB@
