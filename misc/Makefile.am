# Makefile for misc library.

AUTOMAKE_OPTIONS = 1.3 foreign

EXTRA_DIST = \
	alloca.c	fakefork.c	fnmatch.c	getcwd.c	\
	getmntent.c	getwd.c		glob.c		inet_aton.c	\
	memcmp.c	mktime.c	myrealloc.c	putenv.c	\
	realpath.c	strdup.c	strerror.c	strftime.c	\
	strspn.c	strstr.c	strtol.c	strtoul.c

noinst_HEADERS = \
	inet_aton.h	fnmatch.h	glob.h
noinst_LIBRARIES = libmisc.a

libmisc_a_SOURCES =

libmisc_a_LIBADD = @LIBOBJS@ @ALLOCA@
libmisc_a_DEPENDENCIES = $(libmisc_a_LIBADD)
