# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = tdigest.c tficl.c tkey.c trpmio.c

EXTRA_PROGRAMS = tdigest tkey trpmio

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt @INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h \
	ugid.h
noinst_HEADERS = rpmio_internal.h rpmpgp.h

LIBS +=	-lrt -lpthread

BEECRYPTLOBJS = $(shell cat $(top_builddir)/beecrypt/listobjs)

lib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = digest.c macro.c rpmio.c rpmlog.c rpmmalloc.c \
	rpmpgp.c rpmrpc.c strcasecmp.c stubs.c url.c ugid.c
librpmio_la_LDFLAGS = -release @VERSION@
librpmio_la_LIBADD = $(BEECRYPTLOBJS)
librpmio_la_DEPENDENCIES = .created

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lpopt|" < librpmio.la > .librpmio.la && \
	mv .librpmio.la librpmio.la

$(top_builddir)/beecrypt/listobjs:
	make -C $(top_builddir)/beecrypt listobjs

.created: $(top_builddir)/beecrypt/listobjs
	for lo in $(BEECRYPTLOBJS); do \
	  [ -f $$lo ] || $(LN_S) $(top_builddir)/beecrypt/$$lo $$lo ; \
	done
	touch $@

clean-local:
	rm -f $(BEECRYPTLOBJS) *.o .created

tdigest_SOURCES = tdigest.c
tdigest_LDADD += librpmio.la $(top_builddir)/popt/libpopt.la

trpmio_SOURCES = trpmio.c
trpmio_LDADD += librpmio.la $(top_builddir)/popt/libpopt.la

tkey_SOURCES = tkey.c
tkey_LDADD += librpmio.la $(top_builddir)/popt/libpopt.la

.PHONY:	sources
sources:
	@echo $(librpmio_la_SOURCES:%=rpmio/%)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES)

tficl.o: tficl.c
	$(COMPILE) -I/usr/include/ficl -o $@ -c tficl.c 

tficl: tficl.o
	$(LINK) -o $@ tficl.o -lficl
