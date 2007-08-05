# Makefile for rpm library.

LINT = splint

pylibdir = @WITH_PYTHON_LIB@
pyincdir = @WITH_PYTHON_INCLUDE@

SUBDIRS = 

EXTRA_DIST = system.h rpmdebug-py.c rpm/__init__.py

AM_CPPFLAGS = -I. \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_LIBELF_INCLUDE@ \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc \
	-I$(top_srcdir)/ \
	-I$(pyincdir) \
	@INCPATH@

noinst_HEADERS = header-py.h \
	rpmal-py.h rpmds-py.h rpmdb-py.h rpmfd-py.h rpmfts-py.h \
	rpmfi-py.h rpmmi-py.h rpmps-py.h rpmrc-py.h rpmte-py.h rpmts-py.h \
	spec-py.h

mylibs= \
	$(top_builddir)/build/librpmbuild.la \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_LIBELF_LIB@

LDADD =

pyrpmdir = `${__PYTHON} -c 'from distutils.sysconfig import *; print get_python_lib(1, prefix="@prefix@")'`/rpm
pyrpm_LTLIBRARIES = _rpmmodule.la
pyrpm_SCRIPTS = rpm/__init__.py

_rpmmodule_la_LDFLAGS = $(mylibs) $(LIBS) -module -avoid-version
_rpmmodule_la_LIBADD =  @WITH_BEECRYPT_LIB@

_rpmmodule_la_SOURCES = rpmmodule.c header-py.c \
		       rpmal-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfts-py.c \
		       rpmfi-py.c rpmmi-py.c rpmps-py.c rpmrc-py.c rpmte-py.c \
		       rpmts-py.c spec-py.c

# rpmmodule.c header-py.c \
# 	rpmal-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfi-py.c rpmmi-py.c \
# 	rpmps-py.c rpmrc-py.c rpmte-py.c rpmts-py.c
# rpmmodule.c header-py.c
splint_srcs = \
 	rpmal-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfts-py.c \
	rpmfi-py.c rpmmi-py.c rpmps-py.c rpmrc-py.c rpmte-py.c rpmts-py.c \
	spec-py.c

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(AM_CPPFLAGS) $(splint_srcs)
