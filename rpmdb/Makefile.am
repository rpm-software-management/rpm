# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST = db3.c db1.c falloc.c

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = rpmdb.h
noinst_HEADERS = falloc.h fprint.h legacy.h rpmhash.h

mylibpaths = \
	-L$(top_builddir)/lib/.libs \
	-L$(top_builddir)/rpmio/.libs \
	-L$(top_builddir)/popt/.libs

mylibs = -lrpm -lrpmio -lpopt @LIBS@ @INTLLIBS@ @LIBMISC@
LIBS =

DB3LOBJS = $(shell cat $(top_builddir)/$(WITH_DB_SUBDIR)/db3lobjs)

lib_LTLIBRARIES = librpmdb.la
librpmdb_la_SOURCES = $(DBLIBSRCS) \
	dbconfig.c fprint.c hdrNVR.c legacy.c poptDB.c rpmhash.c rpmdb.c \
	tagname.c tagtbl.c
librpmdb_la_LDFLAGS = -release @VERSION@ @libdb3@
librpmdb_la_LIBADD = $(DBLIBOBJS) $(DB3LOBJS)
librpmdb_la_DEPENDENCIES = $(DBLIBOBJS) .created

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lrpmio|" < librpmdb.la > .librpmdb.la && \
	mv .librpmdb.la librpmdb.la

falloc.lo: falloc.c $(top_srcdir)/system.h $(top_srcdir)/rpmio/rpmio.h falloc.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

tagtbl.c: $(top_srcdir)/lib/rpmlib.h 
	@echo '#include "system.h"' > $@
	@echo '#include "lib/rpmlib.h"' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'static const struct headerTagTableEntry_s rpmTagTbl[] = {' >> $@
	$(AWK) '/(RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ { if ($$2 == "=") { printf("\t{ \"%s\", %s },\n", $$1, $$3); } else { printf("\t{ \"%s\", %s },\n", $$2, $$3); } }' < $(top_srcdir)/lib/rpmlib.h >> $@
	@echo '	{ NULL, 0 }' >> $@
	@echo '};' >> $@
	@echo '' >> $@
	@echo '/*@observer@*/ /*@unchecked@*/' >> $@
	@echo 'const struct headerTagTableEntry_s * rpmTagTable = rpmTagTbl;' >> $@
	@echo '' >> $@
	@echo '/*@unchecked@*/' >> $@
	@echo 'const int rpmTagTableSize = sizeof(rpmTagTbl) / sizeof(rpmTagTbl[0]) - 1;' >> $@

.created:
	for lo in $(DB3LOBJS); do \
	  [ -f $$lo ] || $(LN_S) $(top_builddir)/$(WITH_DB_SUBDIR)/$$lo $$lo ; \
	done
	touch $@

clean-local:
	rm -f $(DB3LOBJS) *.o .created

# XXX TODO: figger why .created addition here dinna work
BUILT_SOURCES = tagtbl.c # .created

.PHONY:	sources
sources:
	@echo $(librpmdb_la_SOURCES:%=rpmdb/%)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpmdb_la_SOURCES)

tdbi: librpmdb.la tdbi.o
	$(LINK) -all-static $@.o $< $(mylibpaths) $(mylibs) $(LIBS)

tfalloc: librpmdb.la tfalloc.o
	$(LINK) -all-static $@.o $< $(mylibpaths) $(mylibs) $(LIBS)
