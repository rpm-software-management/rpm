# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/popt \
	-I/usr/include/python1.5 \
        @INCPATH@

EXTRA_DIST = hash.h upgrade.h
mylibs= \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/popt/libpopt.la

LDADD =

pythondir = $(prefix)/lib/python1.5/site-packages
python_PROGRAMS = rpmmodule.so
rpmmodule_so_SOURCES =
# XXX rpmmodule_so_LDFLAGS = -L../lib/.libs -lrpm -L../popt/.libs -lpopt $(LIBS) -shared -Wl,-soname,rpmmodule.so
rpmmodule_so_LDFLAGS = -lrpm -lpopt $(LIBS) -shared -Wl,-soname,rpmmodule.so

noinst_LTLIBRARIES = librpmmodule.la
librpmmodule_la_SOURCES = rpmmodule.c hash.c upgrade.c

rpmmodule.so: $(librpmmodule_la_OBJECTS)
	$(CC) -o $@ $^ $(rpmmodule_so_LDFLAGS)
