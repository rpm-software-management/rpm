# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/lib \
	-I$(top_srcdir)/popt @INCPATH@

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmio.h rpmurl.h rpmmacro.h rpmlog.h rpmmessages.h rpmerr.h
noinst_HEADERS = rpmio_internal.h ugid.h

lib_LTLIBRARIES = librpmio.la
librpmio_la_SOURCES = rpmrpc.c rpmio.c rpmio_api.c url.c macro.c \
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

trpmio: librpmio.la trpmio.o
	$(LINK) -o $@ trpmio.o librpmio.la $(top_builddir)/popt/libpopt.la

