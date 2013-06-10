# Makefile for rpmbuild library.

AM_CPPFLAGS =  -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += @WITH_NSS_INCLUDE@
AM_CPPFLAGS += @WITH_MAGIC_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c expression.c files.c misc.c pack.c \
	parseBuildInstallClean.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c reqprov.c rpmfc.c spec.c \
	parsePolicies.c policies.c \
	rpmbuild_internal.h rpmbuild_misc.h

librpmbuild_la_LDFLAGS = -version-info 5:0:2
librpmbuild_la_LIBADD = \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/libmisc.la \
	@WITH_POPT_LIB@ \
	@WITH_MAGIC_LIB@
