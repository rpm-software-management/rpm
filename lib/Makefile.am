# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt @INCPATH@

EXTRA_DIST = db3.c db2.c db1.c falloc.c

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	header.h misc.h rpmlib.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h falloc.h fprint.h hash.h install.h \
	md5.h rollback.h \
	rpmdb.h rpmlead.h signature.h

mylibpaths =	-L$(top_builddir)/lib/.libs -L$(top_builddir)/rpmio/.libs \
		-L$(top_builddir)/popt/.libs
mylibs =	-lrpm -lrpmio -lpopt @LIBS@ @INTLLIBS@ @LIBMISC@
LIBS =

lib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c $(DBLIBSRCS) depends.c \
	formats.c fprint.c fs.c hash.c header.c install.c \
	md5.c md5sum.c misc.c package.c problems.c \
	poptBT.c poptQV.c query.c rollback.c \
	rpmchecksig.c rpmdb.c rpminstall.c \
	rpmlead.c rpmlibprov.c rpmrc.c scriptlet.c signature.c stringbuf.c \
	tagName.c tagtable.c transaction.c uninstall.c verify.c
librpm_la_LDFLAGS = @libdb3@ @libdb2@ @libdb1@
librpm_la_LIBADD = $(DBLIBOBJS)
librpm_la_DEPENDENCIES = $(DBLIBOBJS)

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lrpmio|" < librpm.la > .librpm.la && \
	mv .librpm.la librpm.la

falloc.lo: falloc.c $(top_srcdir)/system.h $(top_srcdir)/rpmio/rpmio.h falloc.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

tagtable.c: rpmlib.h 
	@echo '#include "system.h"' > tagtable.c
	@echo '#include "rpmlib.h"' >> tagtable.c
	@echo '' >> tagtable.c
	@echo 'const struct headerTagTableEntry rpmTagTable[] = {' >> tagtable.c
	$(AWK) '/(RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ { if ($$2 == "=") { printf("\t{ \"%s\", %s },\n", $$1, $$3); } else { printf("\t{ \"%s\", %s },\n", $$2, $$3); } }' < $(srcdir)/rpmlib.h >> tagtable.c
	@echo '	{ NULL, 0 }' >> tagtable.c
	@echo '};' >> tagtable.c
	@echo '' >> tagtable.c
	@echo 'const int rpmTagTableSize = sizeof(rpmTagTable) / sizeof(struct headerTagTableEntry) - 1;' >> tagtable.c

BUILT_SOURCES = tagtable.c

.PHONY:	lclint
.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpm_la_SOURCES)

th: th.c librpm.la
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)
