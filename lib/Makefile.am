# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt @INCPATH@

EXTRA_DIST = falloc.c db1.c db2.c db3.c

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	header.h misc.h rpmlib.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h falloc.h fprint.h hash.h install.h \
	md5.h oldheader.h oldrpmdb.h rpm_malloc.h \
	rpmdb.h rpmlead.h signature.h

mylibpaths =	-L$(top_builddir)/lib -L$(top_builddir)/popt
mylibs =	-lrpm -lrpmio -lpopt @INTLLIBS@ @LIBMISC@

lib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c $(DBLIBOBJS) depends.c \
	formats.c fprint.c fs.c hash.c header.c install.c \
	md5.c md5sum.c misc.c oldheader.c package.c problems.c \
	poptBT.c poptQV.c query.c rpmchecksig.c rpmdb.c rpminstall.c \
	rpmlead.c rpmrc.c signature.c stringbuf.c stubs.c \
	tagName.c tagtable.c transaction.c uninstall.c verify.c
librpm_la_LDFLAGS = -L$(top_builddir)/rpmio/.libs -lrpmio
librpm_la_LIBADD = $(subst .c,.lo,$(DBLIBOBJS))

# XXX Drill out -L ldflags remnants until libtool-1.4 appears
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|-L$(top_builddir)/rpmio/.libs ||" < librpm.la > .librpm.la && \
	mv .librpm.la librpm.la

falloc.lo: falloc.c $(top_srcdir)/system.h $(top_srcdir)/rpmio/rpmio.h falloc.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<

db3.lo: db3.c $(top_srcdir)/system.h rpmlib.h rpmdb.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<
	for F in $*.o $*.lo ; do \
	    @__LD@ -r -o $${F}.o $${F} -L/usr/lib -ldb-3.0 ; \
	    @__OBJCOPY@ `\
		@__NM@ -g --defined-only $${F}.o | \
		sed -e '/ [DRTW] /!d' -e 's/.* [DRTW] /-L /' | \
		grep -v '^-L $*'` $${F}.o $${F} ; \
	    rm -f $${F}.o ; \
	done

db2.lo: db2.c $(top_srcdir)/system.h rpmlib.h rpmdb.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<
	for F in $*.o $*.lo ; do \
	    @__LD@ -r -o $${F}.o $${F} -L/usr/lib -ldb ; \
	    @__OBJCOPY@ `\
		@__NM@ -g --defined-only $${F}.o | \
		sed -e '/ [DRTW] /!d' -e 's/.* [DRTW] /-L /' | \
		grep -v '^-L $*'` $${F}.o $${F} ; \
	    rm -f $${F}.o ; \
	done

db1.lo: db1.c falloc.lo $(top_srcdir)/system.h rpmlib.h rpmdb.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<
	for F in $*.o $*.lo ; do \
	    @__LD@ -r -o $${F}.o $${F} falloc.lo -L/usr/lib -ldb1 ; \
	    @__OBJCOPY@ `\
		@__NM@ -g --defined-only $${F}.o | \
		sed -e '/ [DRTW] /!d' -e 's/.* [DRTW] /-L /' | \
		grep -v '^-L $*' | grep -v '^-L fadio'` $${F}.o $${F} ; \
	    rm -f $${F}.o ; \
	done

tagtable.c: rpmlib.h 
	@echo '#include "system.h"' > tagtable.c
	@echo '#include "rpmlib.h"' >> tagtable.c
	@echo '' >> tagtable.c
	@echo 'const struct headerTagTableEntry rpmTagTable[] = {' >> tagtable.c
	@awk '/(RPMTAG_[A-Z0-9]*)[ \t]+([0-9]*)/ && !/internal/ { printf("\t{ \"%s\", %s },\n", $$2, $$3); }' < $(srcdir)/rpmlib.h >> tagtable.c
	@echo '	{ NULL, 0 }' >> tagtable.c
	@echo '};' >> tagtable.c
	@echo '' >> tagtable.c
	@echo 'const int rpmTagTableSize = sizeof(rpmTagTable) / sizeof(struct headerTagTableEntry) - 1;' >> tagtable.c

BUILT_SOURCES = tagtable.c

.PHONY:	lclint
.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpm_la_SOURCES)

tmacro: macro.c
	$(CC) $(CFLAGS) $(DEFS) -DDEBUG_MACROS $(INCLUDES) -o $@ $<

rpmeval: macro.c
	$(CC) $(CFLAGS) $(DEFS) -DDEBUG_MACROS -DEVAL_MACROS $(INCLUDES) -o $@ $<

tufdio: tufdio.c
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)
