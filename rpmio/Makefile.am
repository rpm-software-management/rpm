# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

EXTRA_DIST = tax.c tdigest.c tdir.c tficl.c tfts.c tglob.c tinv.c tkey.c trpmio.c

EXTRA_PROGRAMS = tax tdigest tdir tfts tglob tinv tkey tring trpmio tsw dumpasn1

INCLUDES = -I. \
	-I$(top_srcdir) \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_LUA_INCLUDE@ \
	-I$(top_srcdir)/popt \
	@INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	argv.h fts.h rpmdav.h \
	rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h rpmpgp.h \
	rpmsq.h rpmsw.h ugid.h
noinst_HEADERS = rpmio_internal.h rpmlua.h rpmhook.h

BEECRYPTLOBJS = $(shell test X"@WITH_BEECRYPT_SUBDIR@" != X && cat $(top_builddir)/@WITH_BEECTYPT_SUBDIR@/listobjs)

LDFLAGS = -L$(RPM_BUILD_ROOT)$(usrlibdir) -L$(DESTDIR)$(usrlibdir)

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = \
	argv.c digest.c fts.c macro.c rpmdav.c \
	rpmhook.c rpmio.c rpmlog.c rpmlua.c rpmmalloc.c \
	rpmpgp.c rpmrpc.c rpmsq.c rpmsw.c strcasecmp.c stubs.c url.c ugid.c
librpmio_la_LDFLAGS = -release 4.3 $(LDFLAGS) \
	@WITH_BEECRYPT_LIB@ \
	@WITH_LUA_LIB@ \
	$(top_builddir)/file/src/libmagic.la \
	@WITH_ZLIB_LIB@ \
	-lneon -lrt -lpthread
librpmio_la_LIBADD = # $(BEECRYPTLOBJS)
librpmio_la_DEPENDENCIES = # .created

.created:
	if test X"@WITH_BEECRYPT_SUBDIR@" != X; then \
	${MAKE} -C $(top_builddir)/@WITH_BEECRYPT_SUBDIR@ listobjs ; \
	for lo in $(BEECRYPTLOBJS); do \
	  [ -f $$lo ] || $(LN_S) $(top_builddir)/@WITH_BEECRYPT_SUBDIR@/$$lo $$lo ; \
	done \
	fi
	touch $@

clean-local:
	rm -f $(BEECRYPTLOBJS) *.o .created

#BUILT_SOURCES = rpmio.lcd

rpmio.lcd: Makefile.am ${librpmio_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint -load ../beecrypt/beecrypt.lcd $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES) -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpmio_la_SOURCES:%=rpmio/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES)

tax_SOURCES = tax.c
tax_LDADD = librpmio.la

tdigest_SOURCES = tdigest.c
tdigest_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la -lgcrypt

tdir_SOURCES = tdir.c
tdir_LDFLAGS = -all-static
tdir_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la

tfts_SOURCES = tfts.c
tfts_LDFLAGS = -all-static
tfts_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la

tglob_SOURCES = tglob.c
tglob_LDFLAGS = -all-static
tglob_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la

tinv_SOURCES = tinv.c
tinv_LDFLAGS = -all-static
tinv_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la

tkey_SOURCES = tkey.c
tkey_LDFLAGS = -all-static
tkey_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la

tring_SOURCES = tring.c
tring_LDFLAGS = -all-static
tring_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la

trpmio_SOURCES = trpmio.c
trpmio_LDADD = librpmio.la $(top_builddir)/popt/libpopt.la

tsw_SOURCES = tsw.c
tsw_LDFLAGS = librpmio.la

dumpasn1_SOURCES = dumpasn1.c

tficl.o: tficl.c
	$(COMPILE) -I/usr/include/ficl -o $@ -c tficl.c 

tficl: tficl.o
	$(LINK) -o $@ tficl.o -lficl -lm
