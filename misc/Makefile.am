# Makefile for misc library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) @INCPATH@

EXTRA_DIST = \
	alloca.c	error.c		fakefork.c	fnmatch.c	\
	getcwd.c	getmntent.c	getwd.c		glob.c		\
	inet_aton.c	memcmp.c	mktime.c	myrealloc.c	\
	putenv.c	realpath.c	stpcpy.c	stpncpy.c	\
	strcasecmp.c	strncasecmp.c	strcspn.c	strdup.c	\
	strerror.c	strftime.c	strcspn.c	strstr.c	\
	strtol.c	strtoul.c

noinst_HEADERS = \
	error.h		fnmatch.h	glob.h				\
	libgettext.h	message.h	po-lex.h	str-list.h	\
	fstrcmp.h

noinst_LTLIBRARIES = libmisc.la

libmisc_la_SOURCES =

libmisc_la_LIBADD = @LIBOBJS@ @ALLOCA@
libmisc_la_DEPENDENCIES = $(libmisc_la_LIBADD)
