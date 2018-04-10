noinst_LTLIBRARIES = libluaext.la

EXTRA_DIST = \
	modemuncher.c

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include -I.

libluaext_la_CPPFLAGS = $(AM_CPPFLAGS) -DWITH_POSIX @LUA_CFLAGS@
libluaext_la_LIBADD = @LUA_LIBS@
libluaext_la_SOURCES = \
		lposix.h \
		lposix.c \
		lrexlib.h \
		lrexlib.c
