# Makefile for rpm tools.

AUTOMAKE_OPTIONS = 1.3 foreign

INCLUDES = -I../build -I../lib -I../intl -I../misc @INCPATH@

LDFLAGS = -L../popt -L../build -L../lib -L../misc
LDADD = -lrpm

noinst_PROGRAMS = rpmlead rpmheader rpmarchive rpmsignature dump dumpdb

$(PROGRAMS): ../lib/librpm.a

rpmgettext: rpmgettext.o ../build/librpmbuild.a ../popt/libpopt.a ../lib/librpm.a

