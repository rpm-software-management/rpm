# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/popt \
	-I/usr/include/python1.5 \
        @INCPATH@

mylibs= \
	$(top_builddir)/lib/librpm.la \
	$(top_builddir)/popt/libpopt.la

pythondir = $(prefix)/lib/python1.5/site-packages
python_PROGRAMS = rpmmodule.so

noinst_LTLIBRARIES = librpmmodule.la
librpmmodule_la_SOURCES = rpmmodule.c

rpmmodule.so: rpmmodule.lo
	gcc -shared  rpmmodule.lo  -ldb1 -L/usr/local/lib -lz -lc -o $@

