# Makefile for rpm library.

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)
AM_CPPFLAGS += -I$(top_srcdir)/rpmio
AM_CPPFLAGS += @WITH_NSS_INCLUDE@
AM_CPPFLAGS += @WITH_LUA_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DRPMCONFIGDIR="\"@RPMCONFIGDIR@\""

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c base64.c digest.c fts.c macro.c \
	rpmhook.c rpmio.c rpmlog.c rpmlua.c rpmmalloc.c \
	rpmpgp.c rpmrpc.c rpmsq.c rpmsw.c strcasecmp.c url.c ugid.c \
	rpmio_internal.h rpmlua.h rpmhook.h ugid.h fts.h rpmerr.h
librpmio_la_LDFLAGS = -release 4.4
librpmio_la_LIBADD = \
	../misc/libmisc.la \
	@WITH_NSS_LIB@ \
	@WITH_LUA_LIB@ \
	@WITH_MAGIC_LIB@ \
	@WITH_BZ2_LIB@ \
	@WITH_ZLIB_LIB@ \
	-lpthread

check_PROGRAMS =

check_PROGRAMS += tax
tax_SOURCES = tax.c
tax_LDADD = librpmio.la

check_PROGRAMS += tdir
tdir_SOURCES = tdir.c rpmio_internal.h
tdir_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tfts
tfts_SOURCES = tfts.c rpmio_internal.h
tfts_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tget
tget_SOURCES = tget.c rpmio_internal.h
tget_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += thkp
thkp_SOURCES = thkp.c rpmio_internal.h
thkp_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tglob
tglob_SOURCES = tglob.c rpmio_internal.h
tglob_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tinv
tinv_SOURCES = tinv.c
tinv_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tkey
tkey_SOURCES = tkey.c rpmio_internal.h
tkey_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tring
tring_SOURCES = tring.c rpmio_internal.h
tring_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += trpmio
trpmio_SOURCES = trpmio.c
trpmio_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tsw
tsw_SOURCES = tsw.c
tsw_LDADD = librpmio.la

check_PROGRAMS += tput
tput_SOURCES = tput.c
tput_LDADD = librpmio.la @WITH_POPT_LIB@
