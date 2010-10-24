# Makefile for rpm library.

EXTRA_DIST = rpm/__init__.py rpm/transaction.py

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += -I$(top_srcdir)/python
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -I@WITH_PYTHON_INCLUDE@

pkgpyexec_LTLIBRARIES = _rpmmodule.la _rpmbmodule.la _rpmsmodule.la
pkgpyexec_DATA = rpm/__init__.py rpm/transaction.py

_rpmmodule_la_LDFLAGS = -module -avoid-version -shared
_rpmmodule_la_LIBADD = \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_PYTHON_LIB@

_rpmmodule_la_SOURCES = rpmmodule.c rpmsystem-py.h \
	header-py.c header-py.h \
	rpmds-py.c rpmds-py.h \
	rpmfd-py.c rpmfd-py.h \
	rpmfi-py.c rpmfi-py.h \
	rpmkeyring-py.c rpmkeyring-py.h \
	rpmmi-py.c rpmmi-py.h \
	rpmii-py.c rpmii-py.h \
	rpmps-py.c rpmps-py.h \
	rpmmacro-py.c rpmmacro-py.h \
	rpmtd-py.c rpmtd-py.h \
	rpmte-py.c rpmte-py.h \
	rpmts-py.c rpmts-py.h

_rpmbmodule_la_LDFLAGS = -module -avoid-version -shared
_rpmbmodule_la_LIBADD = \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_PYTHON_LIB@

_rpmbmodule_la_SOURCES = rpmbmodule.c rpmsystem-py.h \
	spec-py.c spec-py.h

_rpmsmodule_la_LDFLAGS = -module -avoid-version -shared
_rpmsmodule_la_LIBADD = \
        $(top_builddir)/sign/librpmsign.la \
        $(top_builddir)/lib/librpm.la \
        $(top_builddir)/rpmio/librpmio.la \
        @WITH_PYTHON_LIB@

_rpmsmodule_la_SOURCES = rpmsmodule.c rpmsystem-py.h

