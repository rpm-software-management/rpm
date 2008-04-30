# Makefile for misc library.

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/rpm
AM_CPPFLAGS += -I$(top_srcdir)/misc

EXTRA_DIST = \
	basename.c	err.c		err.h \
	error.c		error.h \
	fakefork.c	fnmatch.c	fnmatch.h \
	getcwd.c	getmntent.c	\
	getwd.c		glob.c		glob.h				\
	mktime.c	myrealloc.c	putenv.c	realpath.c	\
	setenv.c	stpcpy.c	stpncpy.c			\
			strcspn.c	strdup.c	strerror.c	\
	strftime.c	strcspn.c	strstr.c	strtol.c	\
	strtoul.c

noinst_LTLIBRARIES = libmisc.la

libmisc_la_SOURCES =
libmisc_la_LIBADD = @LTLIBOBJS@
