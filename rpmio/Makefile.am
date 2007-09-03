# Makefile for rpm library.

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_NEON_INCLUDE@ \
	@WITH_LUA_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc
AM_CPPFLAGS += -DRPMCONFIGDIR="\"@RPMCONFIGDIR@\""

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	argv.h fts.h rpmdav.h \
	rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h rpmpgp.h \
	rpmsq.h rpmsw.h ugid.h

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c digest.c fts.c macro.c rpmdav.c \
	rpmhook.c rpmio.c rpmlog.c rpmlua.c rpmmalloc.c \
	rpmpgp.c rpmrpc.c rpmsq.c rpmsw.c strcasecmp.c stubs.c url.c ugid.c \
	rpmio_internal.h rpmlua.h rpmhook.h
librpmio_la_LDFLAGS = -release 4.4 \
	@WITH_BEECRYPT_LIB@ \
	@WITH_NEON_LIB@ \
	@WITH_LUA_LIB@ \
	@WITH_MAGIC_LIB@ \
	@WITH_ZLIB_LIB@ \
	-lpthread

check_PROGRAMS =

check_PROGRAMS += tax
tax_SOURCES = tax.c
tax_LDADD = librpmio.la

check_PROGRAMS += tdir
tdir_SOURCES = tdir.c rpmio_internal.h
# tdir_LDFLAGS = @LDFLAGS_STATIC@
tdir_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tfts
tfts_SOURCES = tfts.c rpmio_internal.h
# tfts_LDFLAGS = @LDFLAGS_STATIC@
tfts_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tget
tget_SOURCES = tget.c rpmio_internal.h
# tget_LDFLAGS = @LDFLAGS_STATIC@
tget_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += thkp
thkp_SOURCES = thkp.c rpmio_internal.h
# thkp_LDFLAGS = @LDFLAGS_STATIC@
thkp_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tput
tput_SOURCES = tput.c rpmio_internal.h
# tput_LDFLAGS = @LDFLAGS_STATIC@
tput_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tglob
tglob_SOURCES = tglob.c rpmio_internal.h
# tglob_LDFLAGS = @LDFLAGS_STATIC@
tglob_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tinv
tinv_SOURCES = tinv.c
# tinv_LDFLAGS = @LDFLAGS_STATIC@
tinv_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tkey
tkey_SOURCES = tkey.c rpmio_internal.h
# tkey_LDFLAGS = @LDFLAGS_STATIC@
tkey_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tring
tring_SOURCES = tring.c rpmio_internal.h
# tring_LDFLAGS = @LDFLAGS_STATIC@
tring_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += trpmio
trpmio_SOURCES = trpmio.c
trpmio_LDADD = librpmio.la @WITH_POPT_LIB@

check_PROGRAMS += tsw
tsw_SOURCES = tsw.c
tsw_LDFLAGS = librpmio.la
