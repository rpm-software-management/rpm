# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/beecrypt \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST = getdate.y

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	header.h hdrinline.h \
	misc.h rpmcli.h rpmlib.h stringbuf.h
noinst_HEADERS = \
	cpio.h depends.h fsm.h header_internal.h \
	manifest.h psm.h \
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
	cpio.c depends.c formats.c fs.c fsm.c getdate.c \
	header.c header_internal.c \
	manifest.c misc.c package.c \
	problems.c poptI.c poptQV.c psm.c query.c \
	rpmchecksig.c rpminstall.c rpmlead.c rpmlibprov.c rpmrc.c rpmvercmp.c \
	signature.c stringbuf.c transaction.c \
	verify.c
librpm_la_LDFLAGS = -release @VERSION@

# XXX Add internal libtool dependence
install-data-local:
	@cd $(DESTDIR)/$(libdir) && \
	sed -e "s|^dependency_libs='|& -lrpmdb -lrpmio -lpopt|" < librpm.la > .librpm.la && \
	mv .librpm.la librpm.la

getdate.c: getdate.y
	@echo expect 10 shift/reduce conflicts
	$(YACC) $(srcdir)/getdate.y
	-@if test -f y.tab.c; then \
	 { echo "/*@-globstate -statictrans -unqualifiedtrans -noparams @*/";\
	   echo "/*@-retvalint -usedef -varuse -nullderef -nullassign @*/";\
	   echo "/*@-readonlytrans -modunconnomods -compdef -noeffectuncon @*/";\
	   echo "/*@-globs -evalorderuncon -modobserveruncon -modnomods @*/";\
	   echo "/*@unused@*/";\
	   sed	-e 's,y.tab.c,getdate.c,' y.tab.c \
		-e 's,^YYSTYPE ,static &,' \
		-e 's,^short ,static &,' \
		-e 's,^const short ,static &,' \
		-e 's,^int yydebug,/*@unused@*/ static &,' \
		-e 's,^int ,static &,' ;\
	   echo "/*@=globs =evalorderuncon =modobserveruncon =modnomods @*/";\
	   echo "/*@=readonlytrans =modunconnomods =compdef =noeffectuncon @*/";\
	   echo "/*@=retvalint =usedef =varuse =nullderef =nullassign @*/";\
	   echo "/*@=globstate =statictrans =unqualifiedtrans =noparams @*/";\
	 } > getdate.c ;\
	   rm -f y.tab.c; \
	else \
	   if test -f getdate.tab.c ; then \
	       mv getdate.tab.c getdate.c ; \
	   else \
	       echo '*** Unable to create getdate.c' ;\
	   fi ;\
	fi

BUILT_SOURCES = getdate.c

.PHONY:	sources
sources:
	@echo $(librpm_la_SOURCES:%=lib/%)

.PHONY:	lclint
lclint:
	lclint $(DEFS) $(INCLUDES) $(librpm_la_SOURCES)

th: th.c librpm.la
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibpaths) $(mylibs) $(LIBS)

trb: trb.o librpm.la
	$(LINK) @LDFLAGS_STATIC@ $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ trb.o $(mylibs) $(LIBS)

trhn: trhn.o librpm.la
	$(LINK) @LDFLAGS_STATIC@ $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ trhn.o $(mylibs) $(LIBS)
