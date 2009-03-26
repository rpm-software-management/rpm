# Makefile for rpm library.

EXTRA_DIST = rpmdebug-py.c rpm/__init__.py

if PYTHON
AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += -I$(top_srcdir)/python
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -I@WITH_PYTHON_INCLUDE@

pkgpyexec_LTLIBRARIES = _rpmmodule.la
pkgpyexec_SCRIPTS = rpm/__init__.py

_rpmmodule_la_LDFLAGS = -module -avoid-version
_rpmmodule_la_LIBADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_PYTHON_LIB@

_rpmmodule_la_SOURCES = rpmmodule.c system.h \
	header-py.c header-py.h \
	rpmds-py.c rpmds-py.h \
	rpmdb-py.c rpmdb-py.h \
	rpmfd-py.c rpmfd-py.h \
	rpmfi-py.c rpmfi-py.h \
	rpmmi-py.c rpmmi-py.h \
	rpmps-py.c rpmps-py.h \
	rpmmacro-py.c rpmmacro-py.h \
	rpmte-py.c rpmte-py.h \
	rpmts-py.c rpmts-py.h \
	spec-py.c spec-py.h
endif
