AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = \
	ct.c readfilemap.c win32filemap.c

EXTRA_PROGRAMS =

LDADD = $(top_builddir)/lib/libexpat.la

noinst_HEADERS = \
	codepage.h filemap.h xmlfile.h xmlmime.h xmltchar.h xmlurl.h

bin_PROGRAMS = xmlwf

xmlwf_SOURCES = \
	codepage.c unixfilemap.c xmlfile.c xmlmime.c xmlwf.c
