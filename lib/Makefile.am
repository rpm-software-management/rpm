# Makefile for rpm library.

LINT = splint

AM_CPPFLAGS = -I. \
	-I$(top_srcdir) \
	-I$(top_srcdir)/rpmdb \
	-I$(top_srcdir)/rpmio \
	@WITH_BEECRYPT_INCLUDE@ \
	@WITH_POPT_INCLUDE@ \
	-I$(top_srcdir)/misc
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\""

EXTRA_DIST = getdate.y

check_PROGRAMS =

pkgincdir = $(pkgincludedir)
pkginc_HEADERS = \
	misc.h rpmcli.h rpmlib.h \
	rpmal.h rpmds.h rpmfi.h rpmgi.h rpmps.h rpmsx.h rpmte.h rpmts.h \
	stringbuf.h

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c cpio.h depends.c formats.c fs.c fsm.c fsm.h getdate.c \
	manifest.c manifest.h misc.c package.c \
	poptALL.c poptI.c poptQV.c psm.c psm.h query.c \
	rpmal.c rpmchecksig.c rpmds.c rpmfi.c rpmgi.c rpminstall.c \
	rpmlead.c rpmlead.h rpmlibprov.c rpmps.c rpmrc.c rpmsx.c rpmte.c rpmts.c \
	rpmvercmp.c signature.c signature.h stringbuf.c transaction.c \
	verify.c rpmlock.c rpmlock.h
librpm_la_LDFLAGS = -release 4.4 \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_SELINUX_LIB@ \
	@LIBINTL@

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

rpmlib.lcd: Makefile.am ${librpm_la_SOURCES} ${pkginc_HEADERS}
	-lclint ${DEFS} ${AM_CPPFLAGS} ${librpm_la_SOURCES} -dump $@ 2>/dev/null

.PHONY:	sources
sources:
	@echo $(librpm_la_SOURCES:%=lib/%)

.PHONY:	lint
lint:
	$(LINT) $(DEFS) $(AM_CPPFLAGS) $(librpm_la_SOURCES)

if SELINUX
check_PROGRAMS += setfiles
setfiles_SOURCES = setfiles.c
# setfiles_LDFLAGS = @LDFLAGS_STATIC@
setfiles_LDADD = @WITH_SELINUX_LIB@ \
	../rpmio/librpmio.la \
	@WITH_POPT_LIB@
endif

check_PROGRAMS += tds
tds_SOURCES = tds.c
tds_LDADD = librpm.la	
 
check_PROGRAMS += trb
trb_SOURCES = trb.c
#trb_LDFLAGS = @LDFLAGS_STATIC@
trb_LDADD = librpm.la
 
check_PROGRAMS += tthread
tthread_SOURCES = tthread.c
tthread_LDFLAGS =
tthread_LDADD = librpm.la @WITH_LIBELF_LIB@
 
check_PROGRAMS += tsystem
tsystem_SOURCES = tsystem.c
tsystem_LDFLAGS =
tsystem_LDADD = ../rpmio/librpmio.la @WITH_POPT_LIB@
 
check_PROGRAMS += tre
tre_SOURCES = tre.c
tre_LDFLAGS = 
tre_LDADD = librpm.la @WITH_SELINUX_LIB@
 
check_PROGRAMS += tcpu
tcpu_SOURCES = tcpu.c
# tcpu_LDFLAGS = @LDFLAGS_STATIC@
tcpu_LDADD = librpm.la
 
check_PROGRAMS += tplatform
tplatform_SOURCES = tplatform.c
# tplatform_LDFLAGS = @LDFLAGS_STATIC@
tplatform_LDADD = librpm.la
 
check_PROGRAMS += tgi
tgi_SOURCES = tgi.c
# tgi_LDFLAGS = @LDFLAGS_STATIC@
tgi_LDADD = librpm.la

check_PROGRAMS += tufdio
tufdio_SOURCES = tufdio.c
# tgi_LDFLAGS = @LDFLAGS_STATIC@
tufdio_LDADD = librpm.la
