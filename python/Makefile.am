# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

PYVER= @WITH_PYTHON_VERSION@

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	-I/usr/include/python${PYVER} \
	@INCPATH@

EXTRA_DIST = hash.h upgrade.h
mylibs= \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la

LDADD =

pythondir = $(prefix)/lib/python${PYVER}/site-packages
python_PROGRAMS = rpmmodule.so poptmodule.so

rpmmodule_so_SOURCES =
rpmmodule_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,rpmmodule.so
poptmodule_so_SOURCES = poptmodule.c
poptmodule_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,poptmodule.so

noinst_LTLIBRARIES = librpmmodule.la
librpmmodule_la_SOURCES = rpmmodule.c hash.c upgrade.c

rpmmodule.so: $(librpmmodule_la_OBJECTS)
	$(LINK) -o $@ $(librpmmodule_la_OBJECTS) $(rpmmodule_so_LDFLAGS)

poptmodule.so: $(poptmodule_so_OBJECTS)
	$(LINK) -o $@ $(poptmodule_so_OBJECTS) $(poptmodule_so_LDFLAGS)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmmodule_la_SOURCES)
