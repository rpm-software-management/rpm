# Makefile for popt library.

AUTOMAKE_OPTIONS = 1.3 foreign

SUBDIRS = po

LDFLAGS = -L$(top_builddir)
INCLUDES = -I$(top_srcdir)

noinst_PROGRAMS = test1
test1_SOURCES = test1.c
test1_LDADD = -lpopt

noinst_HEADERS = popt.h
noinst_LIBRARIES = libpopt.a
libpopt_a_SOURCES = popt.c findme.c poptparse.c poptconfig.c popthelp.c
