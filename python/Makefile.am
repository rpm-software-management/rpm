# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

PYVER= @WITH_PYTHON_VERSION@

pylibdir = $(shell python -c 'import sys; print sys.path[1]')
pyincdir = $(prefix)/include/python${PYVER}

SUBDIRS = rpmdb test

EXTRA_DIST = rpmdebug-py.c

INCLUDES = -I. \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	-I$(pyincdir) \
	@WITH_LIBELF_INCLUDE@ \
	@INCPATH@

noinst_HEADERS = header-py.h \
	rpmal-py.h rpmbc-py.h rpmds-py.h rpmdb-py.h rpmfd-py.h rpmfts-py.h \
	rpmfi-py.h rpmmi-py.h rpmmpw-py.h rpmrc-py.h rpmte-py.h rpmts-py.h

mylibs= \
	$(top_builddir)/lib/.libs/librpm.so \
	$(top_builddir)/rpmdb/.libs/librpmdb.so \
	$(top_builddir)/rpmio/.libs/librpmio.so \
	$(top_builddir)/popt/.libs/libpopt.so \
	@WITH_LIBELF_LIB@

LDADD =

pythondir = $(pylibdir)/site-packages
python_PROGRAMS = rpmmodule.so

rpmdbdir = $(pylibdir)/site-packages/rpmdb
rpmdb_PROGRAMS = _rpmdb.so

noinst_PROGRAMS = poptmodule.so

rpmmodule_so_SOURCES =
rpmmodule_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,rpmmodule.so

_rpmdb_so_SOURCES = _rpmdb.c
_rpmdb_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,_rpmdb.so

poptmodule_so_SOURCES = poptmodule.c
poptmodule_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,poptmodule.so

noinst_LTLIBRARIES = librpmmodule.la
librpmmodule_la_SOURCES = rpmmodule.c header-py.c \
	rpmal-py.c rpmbc-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfts-py.c \
	rpmfi-py.c rpmmi-py.c rpmmpw-py.c rpmrc-py.c rpmte-py.c rpmts-py.c

rpmmodule.so$(EXEEXT): $(librpmmodule_la_OBJECTS)
	$(CC) -o $@ $(librpmmodule_la_OBJECTS) $(rpmmodule_so_LDFLAGS)

_rpmdb.so$(EXEEXT): _rpmdb.lo
	$(CC) -o $@ _rpmdb.lo $(_rpmdb_so_LDFLAGS)

poptmodule.so$(EXEEXT): poptmodule.lo
	$(CC) -o $@ poptmodule.lo $(poptmodule_so_LDFLAGS)

# rpmmodule.c header-py.c \
# 	rpmal-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfi-py.c rpmmi-py.c \
# 	rpmrc-py.c rpmte-py.c rpmts-py.c
# rpmmodule.c header-py.c
splint_srcs = \
 	rpmal-py.c rpmbc-py.c rpmds-py.c rpmdb-py.c rpmfd-py.c rpmfts-py.c \
	rpmfi-py.c rpmmi-py.c rpmmpw-py.c rpmrc-py.c rpmte-py.c rpmts-py.c

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(splint_srcs)
