AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = README.txt xmltest.sh

EXTRA_PROGRAMS = runtests

LDADD = $(top_builddir)/lib/libexpat.la

noinst_HEADERS = chardata.h

runtests_SOURCES = chardata.c runtests.c
