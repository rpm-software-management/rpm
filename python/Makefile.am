# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

PYVER= @WITH_PYTHON_VERSION@

INCLUDES = -I. \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	-I/usr/include/python${PYVER} \
	@INCPATH@

noinst_HEADERS = db-py.h hash.h header-py.h \
	rpmal-py.h rpmds-py.h rpmfi-py.h rpmts-py.h upgrade.h

mylibs= \
	$(top_builddir)/lib/.libs/librpm.so \
	$(top_builddir)/rpmdb/.libs/librpmdb.so \
	$(top_builddir)/rpmio/.libs/librpmio.so \
	$(top_builddir)/popt/.libs/libpopt.so

LDADD =

pythondir = $(prefix)/lib/python${PYVER}/site-packages
python_PROGRAMS = rpmmodule.so poptmodule.so

rpmmodule_so_SOURCES =
rpmmodule_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,rpmmodule.so

poptmodule_so_SOURCES = poptmodule.c
poptmodule_so_LDFLAGS = $(mylibs) $(LIBS) -shared -Wl,-soname,poptmodule.so

noinst_LTLIBRARIES = librpmmodule.la
librpmmodule_la_SOURCES = rpmmodule.c hash.c upgrade.c \
	db-py.c header-py.c rpmal-py.c rpmds-py.c rpmfi-py.c rpmts-py.c

rpmmodule.so$(EXEEXT): $(librpmmodule_la_OBJECTS)
	$(CC) -o $@ $(librpmmodule_la_OBJECTS) $(rpmmodule_so_LDFLAGS)

poptmodule.so$(EXEEXT): $(poptmodule_so_OBJECTS)
	$(CC) -o $@ $(poptmodule_so_OBJECTS) $(poptmodule_so_LDFLAGS)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmmodule_la_SOURCES)
