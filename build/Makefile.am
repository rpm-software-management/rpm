# Makefile for rpmbuild library.

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@
AM_CFLAGS += @OPENMP_CFLAGS@

AM_CPPFLAGS =  -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += @WITH_MAGIC_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += @LUA_CFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/misc

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmbuild.la
librpmbuild_la_SOURCES = \
	build.c files.c misc.c pack.c \
	parseSimpleScript.c parseChangelog.c parseDescription.c \
	parseFiles.c parsePreamble.c parsePrep.c parseReqs.c parseScript.c \
	parseSpec.c parseList.c reqprov.c rpmfc.c spec.c \
	parsePolicies.c policies.c \
	rpmbuild_internal.h rpmbuild_misc.h \
	speclua.c

librpmbuild_la_LDFLAGS = -version-info $(rpm_version_info)
librpmbuild_la_LIBADD = \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/misc/libmisc.la \
	@LTLIBICONV@ \
	@LUA_LIBS@ \
	@WITH_POPT_LIB@ \
	@WITH_MAGIC_LIB@

if LIBELF
librpmbuild_la_LIBADD += @WITH_LIBELF_LIB@
endif
if LIBDW
librpmbuild_la_LIBADD += @WITH_LIBDW_LIB@
endif
