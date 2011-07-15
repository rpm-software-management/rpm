# Makefile for rpm library.

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += @WITH_NSS_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DRPMCONFIGDIR="\"@RPMCONFIGDIR@\""
AM_CPPFLAGS += -DLOCALSTATEDIR="\"$(localstatedir)\""

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c base64.h base64.c digest.h digest.c macro.c \
	rpmhook.c rpmio.c rpmlog.c rpmmalloc.c \
	rpmpgp.c rpmsq.c rpmsw.c url.c \
	rpmio_internal.h rpmhook.h \
	rpmstring.c rpmfileutil.c \
	rpmkeyring.c

librpmio_la_LDFLAGS = -version-info 2:1:0
librpmio_la_LIBADD = \
	../misc/libmisc.la \
	@WITH_NSS_LIB@ \
	@WITH_BZ2_LIB@ \
	@WITH_ZLIB_LIB@ \
	@WITH_LIBELF_LIB@ \
	@WITH_POPT_LIB@ \
	@WITH_LZMA_LIB@ \
	-lpthread

if WITH_LUA
AM_CPPFLAGS += -I$(top_srcdir)/luaext/
AM_CPPFLAGS += @LUA_CFLAGS@
librpmio_la_SOURCES += rpmlua.c rpmlua.h
librpmio_la_LIBADD += @LUA_LIBS@
librpmio_la_LIBADD += $(top_builddir)/luaext/libluaext.la
endif

check_PROGRAMS =

