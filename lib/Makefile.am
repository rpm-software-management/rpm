# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/popt \
	@INCPATH@ \
	-I/usr/include \
	-I$(top_srcdir)/misc

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	dbindex.h header.h misc.h rpmio.h rpmlib.h rpmmacro.h rpmurl.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h falloc.h fprint.h hash.h install.h \
	lookup.h md5.h oldheader.h oldrpmdb.h rpm_malloc.h \
	rpmdb.h rpmlead.h signature.h

lib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c dbindex.c depends.c falloc.c \
	formats.c fprint.c fs.c ftp.c hash.c header.c install.c \
	lookup.c macro.c md5.c md5sum.c \
	messages.c misc.c oldheader.c package.c problems.c query.c \
	rebuilddb.c rpmbzio.c rpmchecksig.c rpmdb.c rpmerr.c rpminstall.c \
	rpmio.c rpmlead.c rpmmalloc.c rpmrc.c signature.c stringbuf.c \
	tagName.c tagtable.c transaction.c tread.c uninstall.c url.c verify.c

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
	lclint ${DEFS} ${INCLUDES} ${librpm_la_SOURCES}

tmacro: macro.c
	$(CC) $(CFLAGS) ${DEFS} -DDEBUG_MACROS ${INCLUDES} -o $@ $<

rpmeval: macro.c
	$(CC) $(CFLAGS) ${DEFS} -DDEBUG_MACROS -DEVAL_MACROS ${INCLUDES} -o $@ $<

