# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

pylibdir = ${prefix}/lib@MARK64@/python@WITH_PYTHON_VERSION@
pyincdir = $(prefix)/include/python@WITH_PYTHON_VERSION@

SUBDIRS = rpmdb rpm

EXTRA_DIST = system.h rpmdebug-py.c

INCLUDES = -I. \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	-I$(top_srcdir)/popt \
	-I$(top_srcdir)/misc \
	-I$(pyincdir) \
	@WITH_LIBELF_INCLUDE@ \
	@INCPATH@

noinst_HEADERS = header-py.h \
	rpmal-py.h rpmds-py.h rpmdb-py.h rpmfd-py.h rpmfts-py.h \
	rpmfi-py.h rpmmi-py.h rpmps-py.h rpmrc-py.h rpmte-py.h rpmts-py.h \
	spec-py.h

mylibs= \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la \
	$(top_builddir)/build/librpmbuild.la \
	@WITH_LIBELF_LIB@

LDADD =

pythondir = $(pylibdir)/site-packages
python_LTLIBRARIES = poptmodule.la

rpmdir = $(pylibdir)/site-packages/rpm
rpm_LTLIBRARIES = _rpmmodule.la

rpmdbdir = $(pylibdir)/site-packages/rpmdb
rpmdb_LTLIBRARIES = _rpmdb.la

_rpmmodule_la_LDFLAGS = $(mylibs) $(LIBS) -module -avoid-version
_rpmmodule_la_LIBADD =  @WITH_BEECRYPT_LIB@

_rpmdb_la_SOURCES = _rpmdb.c
_rpmdb_la_LDFLAGS = $(mylibs) $(LIBS) -module -avoid-version

poptmodule_la_SOURCES = poptmodule.c
poptmodule_la_LDFLAGS = $(mylibs) $(LIBS) -module -avoid-version

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
	$(LINT) $(DEFS) $(INCLUDES) $(splint_srcs)
