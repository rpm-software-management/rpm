# Makefile for rpm library.

AUTOMAKE_OPTIONS = 1.4 foreign

LINT = splint

INCLUDES = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/build \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	-I$(top_srcdir)/popt \
	@INCPATH@

EXTRA_DIST = getdate.y

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	misc.h rpmcli.h rpmlib.h \
	rpmal.h rpmds.h rpmfi.h rpmps.h rpmte.h rpmts.h stringbuf.h
noinst_HEADERS = \
	cpio.h fsm.h manifest.h psm.h rpmlead.h signature.h

mylibs = librpm.la
LIBS =

usrlibdir = $(libdir)@MARK64@
usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c depends.c formats.c fs.c fsm.c getdate.c \
	manifest.c misc.c package.c \
	poptALL.c poptI.c poptQV.c psm.c query.c \
	rpmal.c rpmchecksig.c rpmds.c rpmfi.c rpminstall.c \
	rpmlead.c rpmlibprov.c rpmps.c rpmrc.c rpmte.c rpmts.c rpmvercmp.c \
	signature.c stringbuf.c transaction.c \
	verify.c
librpm_la_LDFLAGS = -release @VERSION@ \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	$(top_builddir)/popt/libpopt.la

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

BUILT_SOURCES = getdate.c # rpmlib.lcd

noinst_PROGRAMS = tds
tds_SOURCES =	tds.c
tds_LDFLAGS =	@LDFLAGS_STATIC@ ./librpm.la

rpmlib.lcd: Makefile.am ${librpm_la_SOURCES} ${pkginc_HEADERS} ${noinst_HEADERS}
	-lclint ${DEFS} ${INCLUDES} ${librpm_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpm_la_SOURCES:%=lib/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(INCLUDES) $(librpm_la_SOURCES)

th: th.c librpm.la
	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibs)

#tds: tds.c librpm.la
#	$(CC) $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibs)
#

trb: trb.o librpm.la
	$(LINK) @LDFLAGS_STATIC@ $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibs)

tplatform: tplatform.o librpm.la
	$(LINK) @LDFLAGS_STATIC@ $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibs)

trhn: trhn.o librpm.la
	$(LINK) @LDFLAGS_STATIC@ $(CFLAGS) $(DEFS) $(INCLUDES) -o $@ $< $(mylibs)
