# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

PYVER= 2.2

SUBDIRS = test

pylibdir = $(shell python -c 'import sys; print sys.path[1]')
pyincdir = $(prefix)/include/python${PYVER}

EXTRA_DIST = debug-py.c

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(pyincdir)

noinst_HEADERS = mpw-py.h rng-py.h

mylibs= $(top_builddir)/.libs/libbeecrypt.so

LDADD =

pythondir = $(pylibdir)/site-packages
python_PROGRAMS = _bc.so

_bc_so_SOURCES =
_bc_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,_bc.so

noinst_LTLIBRARIES = libbc.la
libbc_la_SOURCES = _bc-py.c mpw-py.c rng-py.c

_bc.so$(EXEEXT): $(libbc_la_OBJECTS)
	$(CC) -o $@ $(libbc_la_OBJECTS) $(_bc_so_LDFLAGS)

splint_srcs = _bc-py.c $(libbc_la_sources)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(splint_srcs)
