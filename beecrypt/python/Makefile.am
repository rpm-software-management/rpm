# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign no-dependencies

LINT = splint

SUBDIRS = test

EXTRA_DIST = debug-py.c

INCLUDES = -I. \
	-I$(top_srcdir)/.. @PYTHONINC@

noinst_HEADERS = mpw-py.h rng-py.h

mylibs = $(top_builddir)/libbeecrypt.la

LDADD =

pythondir = @PYTHONLIB@
python_LTLIBRARIES = _bc.la

_bc_la_SOURCES = _bc-py.c mpw-py.c rng-py.c
_bc_la_LDFLAGS = -avoid-version -module
_bc_la_LIBADD = $(mylibs)

splint_srcs = _bc-py.c $(libbc_la_sources)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(splint_srcs)
