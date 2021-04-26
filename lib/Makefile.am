# Makefile for rpm library.

include $(top_srcdir)/rpm.am
AM_CFLAGS = @RPMCFLAGS@

AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir) -I$(top_builddir)/include/
AM_CPPFLAGS += @WITH_POPT_INCLUDE@
AM_CPPFLAGS += @LUA_CFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/misc
AM_CPPFLAGS += -DLOCALEDIR="\"$(localedir)\""
AM_CPPFLAGS += -DSYSCONFDIR="\"$(sysconfdir)\""
AM_CPPFLAGS += -DLOCALSTATEDIR="\"$(localstatedir)\""
AM_CPPFLAGS += -DLIBRPMALIAS_FILENAME="\"rpmpopt-${VERSION}\""
AM_CPPFLAGS += -DLIBRPMALIAS_EXECPATH="\"$(bindir)\""

usrlibdir = $(libdir)

check_PROGRAMS =
CLEANFILES =
EXTRA_DIST = gentagtbl.sh tagtbl.C rpmhash.C rpmhash.H
EXTRA_PROGRAMS =

usrlib_LTLIBRARIES = librpm.la
librpm_la_SOURCES = \
	backend/dbi.c backend/dbi.h backend/dummydb.c \
	backend/dbiset.c backend/dbiset.h \
	headerutil.c header.c headerfmt.c header_internal.h \
	rpmdb.c rpmdb_internal.h \
	fprint.c fprint.h tagname.c rpmtd.c \
	cpio.c cpio.h depends.c order.c formats.c tagexts.c fsm.c fsm.h \
	manifest.c manifest.h package.c \
	poptALL.c poptI.c poptQV.c psm.c query.c \
	rpmal.c rpmal.h rpmchecksig.c rpmds.c rpmds_internal.h \
	rpmfi.c rpmfi_internal.h \
	rpmgi.h rpmgi.c rpminstall.c rpmts_internal.h \
	rpmlead.c rpmlead.h rpmps.c rpmprob.c rpmrc.c \
	rpmte.c rpmte_internal.h rpmts.c rpmfs.h rpmfs.c \
	signature.c signature.h transaction.c \
	verify.c rpmlock.c rpmlock.h misc.h relocation.c \
	rpmscript.h rpmscript.c \
	rpmchroot.c rpmchroot.h \
	rpmplugins.c rpmplugins.h rpmplugin.h rpmug.c rpmug.h \
	rpmtriggers.h rpmtriggers.c rpmvs.c rpmvs.h

librpm_la_LDFLAGS = -version-info $(rpm_version_info)

librpm_la_LIBADD = \
	$(top_builddir)/rpmio/librpmio.la \
	@WITH_POPT_LIB@ \
	@WITH_CAP_LIB@ \
	@WITH_ACL_LIB@ \
	@LUA_LIBS@ \
	@LIBINTL@

if BDB_RO
librpm_la_SOURCES += backend/bdb_ro.c
endif

if NDB
librpm_la_SOURCES += \
	backend/ndb/glue.c \
	backend/ndb/rpmpkg.c \
	backend/ndb/rpmpkg.h \
	backend/ndb/rpmidx.c \
	backend/ndb/rpmidx.h \
	backend/ndb/rpmxdb.c \
	backend/ndb/rpmxdb.h
endif

if SQLITE
AM_CPPFLAGS += $(SQLITE_CFLAGS)
librpm_la_LIBADD += $(SQLITE_LIBS)
librpm_la_SOURCES += backend/sqlite.c
endif

tagtbl.C: Makefile.am $(srcdir)/rpmtag.h gentagtbl.sh
	@AWK=${AWK} ${SHELL} $(srcdir)/gentagtbl.sh \
		    	     $(srcdir)/rpmtag.h > $@.new && \
	 mv -f $@.new $@
BUILT_SOURCES = tagtbl.C

CLEANFILES += $(BUILT_SOURCES)
