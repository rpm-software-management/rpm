# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/lib \
	-I$(top_srcdir)/popt @INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h \
	ugid.h
noinst_HEADERS = base64.h rpmio_internal.h

lib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = rpmrpc.c rpmio.c rpmio_api.c url.c macro.c \
	base64.c digest.c strcasecmp.c stubs.c \
	ugid.c rpmmalloc.c rpmlog.c

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lpopt|" < librpmio.la > .librpmio.la && \
	mv .librpmio.la librpmio.la

.PHONY:	lclint
.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES)

trpmio.o: trpmio.c

trpmio: librpmio.la trpmio.o
	$(LINK) -o $@ trpmio.o librpmio.la $(top_builddir)/popt/libpopt.la

tkey.o: tkey.c
tkey: librpmio.la tkey.o
	$(LINK) -o $@ tkey.o librpmio.la $(top_builddir)/popt/libpopt.la

tdigest.o: tdigest.c
tdigest: librpmio.la tdigest.o
	$(LINK) -o $@ tdigest.o librpmio.la $(top_builddir)/popt/libpopt.la

tficl.o: tficl.c
	$(COMPILE) -I/usr/include/ficl -o $@ -c tficl.c 

tficl: tficl.o
	$(LINK) -o $@ tficl.o -lficl
