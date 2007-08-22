# Makefile for rpm library.

LINT = splint

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_NEON_INCLUDE@ \
	@WITH_LUA_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	argv.h fts.h rpmdav.h \
	rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h rpmpgp.h \
	rpmsq.h rpmsw.h ugid.h
noinst_HEADERS = rpmio_internal.h rpmlua.h rpmhook.h

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c digest.c fts.c macro.c rpmdav.c \
	rpmhook.c rpmio.c rpmlog.c rpmlua.c rpmmalloc.c \
	rpmpgp.c rpmrpc.c rpmsq.c rpmsw.c strcasecmp.c stubs.c url.c ugid.c
librpmio_la_LDFLAGS = -release 4.4 \
	@WITH_BEECRYPT_LIB@ \
	@WITH_NEON_LIB@ \
	@WITH_LUA_LIB@ \
	@WITH_MAGIC_LIB@ \
	@WITH_ZLIB_LIB@ \
	-lpthread

clean-local:
	rm -f *.o

#BUILT_SOURCES = rpmio.lcd

rpmio.lcd: Makefile.am ${librpmio_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint -load ../beecrypt/beecrypt.lcd $(DEFS) $(AM_CPPFLAGS) $(librpmio_la_SOURCES) -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmio_la_SOURCES:%=rpmio/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(AM_CPPFLAGS) $(librpmio_la_SOURCES)

EXTRA_PROGRAMS =

EXTRA_PROGRAMS += tax
tax_SOURCES = tax.c
tax_LDADD = librpmio.la

EXTRA_PROGRAMS += tdigest
tdigest_SOURCES = tdigest.c
tdigest_LDADD = librpmio.la @WITH_POPT_LIB@ -lgcrypt

EXTRA_PROGRAMS += tdir
tdir_SOURCES = tdir.c
# tdir_LDFLAGS = @LDFLAGS_STATIC@
tdir_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += tfts
tfts_SOURCES = tfts.c
# tfts_LDFLAGS = @LDFLAGS_STATIC@
tfts_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += tget
tget_SOURCES = tget.c
# tget_LDFLAGS = @LDFLAGS_STATIC@
tget_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += thkp
thkp_SOURCES = thkp.c
# thkp_LDFLAGS = @LDFLAGS_STATIC@
thkp_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += tput
tput_SOURCES = tput.c
# tput_LDFLAGS = @LDFLAGS_STATIC@
tput_LDADD = librpmio.la @WITH_POPT_LIB@


EXTRA_PROGRAMS += tglob
tglob_SOURCES = tglob.c
# tglob_LDFLAGS = @LDFLAGS_STATIC@
tglob_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += tinv
tinv_SOURCES = tinv.c
# tinv_LDFLAGS = @LDFLAGS_STATIC@
tinv_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += tkey
tkey_SOURCES = tkey.c
# tkey_LDFLAGS = @LDFLAGS_STATIC@
tkey_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += tring
tring_SOURCES = tring.c
# tring_LDFLAGS = @LDFLAGS_STATIC@
tring_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += trpmio
trpmio_SOURCES = trpmio.c
trpmio_LDADD = librpmio.la @WITH_POPT_LIB@

EXTRA_PROGRAMS += tsw
tsw_SOURCES = tsw.c
tsw_LDFLAGS = librpmio.la
