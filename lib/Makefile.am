# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/build @INCPATH@

rpmincdir = $(includedir)/rpm
rpminc_HEADERS = \
	dbindex.h header.h misc.h rpmio.h rpmlib.h rpmmacro.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h falloc.h install.h \
	md5.h oldheader.h oldrpmdb.h rpm_malloc.h \
	rpmdb.h rpmlead.h signature.h tread.h

lib_LIBRARIES = librpm.a
librpm_a_SOURCES = \
	cpio.c dbindex.c depends.c falloc.c \
	formats.c fs.c header.c install.c \
	lookup.c macro.c md5.c md5sum.c \
	messages.c misc.c oldheader.c package.c query.c \
	rebuilddb.c rpmdb.c rpmerr.c rpmio.c rpmlead.c \
	rpmrc.c signature.c stringbuf.c tagtable.c \
	tread.c uninstall.c verify.c transaction.c problems.c hash.c fprint.c

include ../Makefile.inc

#rpmrc.o: rpmrc.c
#	$(COMPILE) -DLIBRPMRC_FILENAME="\"$(LIBRPMRC_FILENAME)"\" -DMACROFILES=\"$(MACROFILES)\" -o $@ -c $<

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
	lclint ${DEFS} ${INCLUDES} ${librpm_a_SOURCES}
