# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST = getdate.y

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	header.h misc.h rpmlib.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h fprint.h fsm.h hash.h \
	manifest.h md5.h psm.h \
	rpmlead.h signature.h

mylibpaths = \
	-L$(top_builddir)/lib/.libs \
	-L$(top_builddir)/rpmdb/.libs \
	-L$(top_builddir)/rpmio/.libs \
	-L$(top_builddir)/popt/.libs

mylibs = -lrpm -lrpmdb -lrpmio -lpopt @LIBS@ @INTLLIBS@ @LIBMISC@
LIBS =

lib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c depends.c formats.c fprint.c fs.c fsm.c getdate.c \
	hash.c header.c manifest.c md5.c md5sum.c misc.c package.c \
	problems.c poptBT.c poptQV.c psm.c query.c rpmchecksig.c \
	rpminstall.c rpmlead.c rpmlibprov.c rpmrc.c signature.c \
	stringbuf.c tagName.c tagtable.c transaction.c verify.c

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lrpmdb -lrpmio|" < librpm.la > .librpm.la && \
	mv .librpm.la librpm.la

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

getdate.c: getdate.y
	@echo expect 10 shift/reduce conflicts
	$(YACC) $(srcdir)/getdate.y
	-@if test -f y.tab.c; then \
	 { echo "/*@-globstate -statictrans -unqualifiedtrans @*/";\
	   echo "/*@-retvalint -usedef -varuse -nullderef -nullassign @*/";\
	   sed	-e 's,y.tab.c,getdate.c,' y.tab.c \
		-e 's,^YYSTYPE ,static &,' \
		-e 's,^short ,static &,' \
		-e 's,^const short ,static &,' \
		-e 's,^int yydebug,/*@unused@*/ static &,' \
		-e 's,^int ,static &,' ;\
	   echo "/*@=retvalint =usedef =varuse =nullderef =nullassign @*/";\
	   echo "/*@=globstate =statictrans =unqualifiedtrans @*/";\
	 } > getdate.c ;\
	   rm -f y.tab.c; \
	else \
	   if test -f getdate.tab.c ; then \
	       mv getdate.tab.c getdate.c ; \
	   else \
	       echo '*** Unable to create getdate.c' ;\
	   fi ;\
	fi

BUILT_SOURCES = tagtable.c getdate.c

.PHONY:	sources
sources:
	@echo $(librpm_la_SOURCES:%=lib/%)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpm_la_SOURCES)

th: th.c librpm.la
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)
