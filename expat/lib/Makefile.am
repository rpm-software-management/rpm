AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = xmltok_impl.c xmltok_ns.c

EXTRA_PROGRAMS =

pkgincdir = $(includedir)/expat
pkginc_HEADERS = expat.h

noinst_HEADERS = \
	ascii.h asciitab.h iasciitab.h internal.h latin1tab.h \
	nametab.h utf8tab.h winconfig.h xmlrole.h xmltok.h xmltok_impl.h

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = libexpat.la
libexpat_la_SOURCES = xmlparse.c xmlrole.c xmltok.c 
libexpat_la_LDFLAGS = -no-undefined -version-info @LIBCURRENT@:@LIBREVISION@:@LIBAGE@
