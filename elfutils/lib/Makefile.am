## Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS = 1.4 gnits

noinst_LTLIBRARIES = libbu.la

INCLUDES = -I$(srcdir)/../libelf -I..

libbu_la_SOURCES = xstrdup.c xstrndup.c xmalloc.c next_prime.c

noinst_HEADERS = fixedsizehash.h system.h \
		 dynamicsizehash.h dynamicsizehash.c
