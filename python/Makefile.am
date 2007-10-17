# Makefile for rpm library.

EXTRA_DIST = rpmdebug-py.c rpm/__init__.py

if PYTHON
AM_CPPFLAGS = -I.
AM_CPPFLAGS += -I$(top_srcdir)/build
AM_CPPFLAGS += -I$(top_srcdir)/lib
AM_CPPFLAGS += -I$(top_srcdir)/rpmdb
AM_CPPFLAGS += -I$(top_srcdir)/rpmio
AM_CPPFLAGS += @WITH_LIBELF_INCLUDE@
AM_CPPFLAGS += @WITH_BEECRYPT_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -I$(top_srcdir)
AM_CPPFLAGS += -I@WITH_PYTHON_INCLUDE@

pkgpyexec_LTLIBRARIES = _rpmmodule.la
pkgpyexec_SCRIPTS = rpm/__init__.py

_rpmmodule_la_LDFLAGS = -module -avoid-version
_rpmmodule_la_LIBADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_BEECRYPT_LIB@ \
	@WITH_POPT_LIB@ \
	@WITH_LIBELF_LIB@ \
	@WITH_PYTHON_LIB@

_rpmmodule_la_SOURCES = rpmmodule.c system.h \
	header-py.c header-py.h \
	rpmal-py.c rpmal-py.h \
	rpmds-py.c rpmds-py.h \
	rpmdb-py.c rpmdb-py.h \
	rpmfd-py.c rpmfd-py.h \
	rpmfts-py.c rpmfts-py.h \
	rpmfi-py.c rpmfi-py.h \
	rpmmi-py.c rpmmi-py.h \
	rpmps-py.c rpmps-py.h \
	rpmrc-py.c rpmrc-py.h \
	rpmte-py.c rpmte-py.h \
	rpmts-py.c rpmts-py.h \
	spec-py.c spec-py.h
endif
