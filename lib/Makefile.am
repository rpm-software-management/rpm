# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build -I$(top_srcdir)/popt @INCPATH@

EXTRA_DIST = db0.[ch] db1.[ch] db2.[ch]

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	dbindex.h header.h misc.h rpmio.h rpmlib.h rpmmacro.h rpmurl.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h falloc.h fprint.h hash.h install.h \
	lookup.h md5.h oldheader.h oldrpmdb.h rpm_malloc.h \
	rpmdb.h rpmlead.h signature.h

mylibpaths= -L$(top_builddir)/lib -L$(top_builddir)/popt
mylibs=	-lrpm -lpopt @INTLLIBS@ @LIBMISC@

lib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c $(DBLIBOBJS) dbindex.c depends.c falloc.c \
	formats.c fprint.c fs.c hash.c header.c install.c \
	lookup.c macro.c md5.c md5sum.c \
	messages.c misc.c oldheader.c package.c problems.c query.c \
	rebuilddb.c rpmchecksig.c rpmdb.c rpmerr.c rpminstall.c \
	rpmio.c rpmlead.c rpmmalloc.c rpmrc.c signature.c stringbuf.c stubs.c \
	tagName.c tagtable.c transaction.c uninstall.c url.c verify.c
librpm_la_LIBADD = $(subst .c,.lo,$(DBLIBOBJS))
#librpm_la_LIBADD = -lpopt

db0.lo: db0.c $(top_srcdir)/system.h rpmlib.h dbindex.h db0.h
	$(LIBTOOL) --mode=compile $(COMPILE) -c $<
	for F in $*.o $*.lo ; do \
	    @__LD@ -r -o $${F}.o $${F} -L/usr/lib -ldb1 ; \
	    @__OBJCOPY@ `\
		@__NM@ -g --defined-only $${F}.o | \
		sed -e '/ [TWD] /!d' -e 's/.* [TWD] /-L /' | \
		grep -v '^-L $*'` $${F}.o $${F} ; \
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
