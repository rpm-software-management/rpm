# Makefile for rpm library.

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += @WITH_NSS_INCLUDE@
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\""
AM_CPPFLAGS += -DLOCALSTATEDIR="\"$(localstatedir)\""
AM_CPPFLAGS += -DRPMCONFIGDIR="\"@RPMCONFIGDIR@\""
AM_CPPFLAGS += -DLIBRPMALIAS_FILENAME="\"@RPMCONFIGDIR@/rpmpopt-${VERSION}\""

EXTRA_DIST = getdate.y

check_PROGRAMS =

usrlibdir = $(libdir)
usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	cpio.c cpio.h depends.c formats.c fs.c fsm.c fsm.h getdate.c \
	idtx.c manifest.c manifest.h misc.c package.c \
	poptALL.c poptI.c poptQV.c psm.c psm.h query.c \
	rpmal.c rpmchecksig.c rpmds.c rpmfi.c rpmfi_internal.h rpmgi.c \
	rpminstall.c rpmtsscore.h rpmts_internal.h \
	rpmlead.c rpmlead.h rpmps.c rpmrc.c \
	rpmte.c rpmte_internal.h rpmts.c \
	rpmvercmp.c signature.c signature.h transaction.c \
	verify.c rpmlock.c rpmlock.h misc.h \
	legacy.c legacy.h

librpm_la_LDFLAGS = -release 4.4

librpm_la_LIBADD = \
	$(top_builddir)/rpmdb/librpmdb.la \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_NSS_LIB@ \
	@WITH_POPT_LIB@ \
	@WITH_SELINUX_LIB@ \
	@LIBINTL@

getdate.c: getdate.y
	@echo expect 10 shift/reduce conflicts
	$(YACC) $(srcdir)/getdate.y
	-@if test -f y.tab.c; then \
	 { sed	-e 's,y.tab.c,getdate.c,' y.tab.c \
		-e 's,^YYSTYPE ,static &,' \
		-e 's,^short ,static &,' \
		-e 's,^const short ,static &,' \
		-e 's,^int yydebug,/*@unused@*/ static &,' \
		-e 's,^int ,static &,' ;\
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

check_PROGRAMS += tds
tds_SOURCES = tds.c
tds_LDADD = librpm.la	
 
check_PROGRAMS += trb
trb_SOURCES = trb.c
trb_LDADD = librpm.la
 
check_PROGRAMS += tthread
tthread_SOURCES = tthread.c
tthread_LDADD = librpm.la @WITH_LIBELF_LIB@
 
check_PROGRAMS += tsystem
tsystem_SOURCES = tsystem.c
tsystem_LDADD = ../rpmio/librpmio.la @WITH_POPT_LIB@
 
check_PROGRAMS += tcpu
tcpu_SOURCES = tcpu.c
tcpu_LDADD = librpm.la
 
check_PROGRAMS += tplatform
tplatform_SOURCES = tplatform.c
tplatform_LDADD = librpm.la
 
check_PROGRAMS += tgi
tgi_SOURCES = tgi.c
tgi_LDADD = librpm.la

