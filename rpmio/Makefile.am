# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/lib \
	-I$(top_srcdir)/popt @INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmio.h rpmurl.h rpmmacro.h
noinst_HEADERS = rpmio_internal.h ugid.h

lib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = rpmio.c url.c macro.c ugid.c rpmmalloc.c rpmerr.c messages.c
librpmio_la_LDFLAGS = -L$(top_builddir)/popt/.libs -lpopt
librpmio_la_LIBADD = @LIBMISC@

# XXX Drill out -L ldflags remnants until libtool-1.4 appears
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|-L$(top_builddir)/popt/.libs ||" < librpmio.la > .librpmio.la && \
	mv .librpmio.la librpmio.la

.PHONY:	lclint
.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmio_la_SOURCES)

trpmio: librpmio.la trpmio.o
	$(LINK) -o $@ trpmio.o librpmio.la

