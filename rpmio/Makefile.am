# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

EXTRA_DIST = tdigest.c tficl.c tkey.c trpmio.c

INCLUDES = \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/popt @INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h \
	ugid.h
noinst_HEADERS = rpmio_internal.h

LIBS +=	-lrt -lpthread

BEECRYPTLOBJS = $(shell cat $(top_builddir)/beecrypt/listobjs)

lib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = rpmrpc.c rpmio.c url.c macro.c \
	digest.c strcasecmp.c stubs.c \
	ugid.c rpmmalloc.c rpmlog.c
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

.PHONY:	sources
sources:
	@echo $(librpmio_la_SOURCES:%=rpmio/%)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES)

trpmio.o: trpmio.c

trpmio: librpmio.la trpmio.o
	$(LINK) -o $@ trpmio.o librpmio.la $(top_builddir)/popt/libpopt.la $(LIBS)

tkey.o: tkey.c
	 $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -o $@ -c tkey.c 

tkey: librpmio.la tkey.o
	$(LINK) -o $@ tkey.o librpmio.la $(top_builddir)/popt/libpopt.la $(LIBS)

tdigest.o: tdigest.c
	 $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) -o $@ -c tdigest.c 

tdigest: librpmio.la tdigest.o
	$(LINK) @LDFLAGS_STATIC@ -o $@ tdigest.o librpmio.la $(top_builddir)/popt/libpopt.la $(LIBS)

tficl.o: tficl.c
	$(COMPILE) -I/usr/include/ficl -o $@ -c tficl.c 

tficl: tficl.o
	$(LINK) -o $@ tficl.o -lficl
