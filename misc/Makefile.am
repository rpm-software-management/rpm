# Makefile for misc library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	@INCPATH@

EXTRA_DIST = \
	alloca.c	basename.c	err.c		error.c		\
	fakefork.c	fnmatch.c	getcwd.c	getmntent.c	\
	getwd.c						memcmp.c	\
	mktime.c	myrealloc.c	putenv.c	realpath.c	\
			stpcpy.c	stpncpy.c	strcasecmp.c	\
	strncasecmp.c	strcspn.c	strdup.c	strerror.c	\
	strftime.c	strcspn.c	strstr.c	strtol.c	\
	strtoul.c

noinst_HEADERS = \
	err.h		error.h		fnmatch.h	fstrcmp.h	\
			libgettext.h	message.h	po-lex.h	\
			str-list.h	fstrcmp.h

noinst_LIBRARIES = libmisc.a

libmisc_a_SOURCES =

libmisc_a_LIBADD = @LIBOBJS@ @ALLOCA@
libmisc_a_DEPENDENCIES = $(libmisc_a_LIBADD)
