# Makefile for misc library.

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/rpm
AM_CPPFLAGS += -I$(top_srcdir)/misc

EXTRA_DIST = \
	basename.c \
	fakefork.c	fnmatch.c	fnmatch.h \
	getcwd.c	getmntent.c	\
	getwd.c		glob.c		glob.h		\
	myrealloc.c	putenv.c	realpath.c	\
	setenv.c	stpcpy.c	stpncpy.c

noinst_LTLIBRARIES = libmisc.la

libmisc_la_SOURCES =
libmisc_la_LIBADD = @LTLIBOBJS@
