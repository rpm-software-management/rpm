# Makefile for misc library.

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include
AM_CPPFLAGS += -I$(top_srcdir)/misc

EXTRA_DIST = \
	fnmatch.c	fnmatch.h \
	glob.c		glob.h		\
	stpcpy.c	stpncpy.c

noinst_LTLIBRARIES = libmisc.la

libmisc_la_SOURCES = fts.c fts.h
libmisc_la_LIBADD = @LTLIBOBJS@
