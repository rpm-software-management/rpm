noinst_LTLIBRARIES = libluaext.la

EXTRA_DIST = \
	modemuncher.c \
	userconfig.c \
	linit.lua

AM_CPPFLAGS = -I$(top_builddir) -I$(top_builddir)/include -I.

libluaext_la_CPPFLAGS = $(AM_CPPFLAGS) -DWITH_POSIX
libluaext_la_LIBADD = @WITH_LUA_LIB@
libluaext_la_SOURCES = \
		lposix.h \
		lposix.c \
		lrexlib.h \
		lrexlib.c 

linit.lch: linit.lua
	bin2c linit.lua > linit.lch
